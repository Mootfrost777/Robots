#include "Arduino.h"

#include <nRF24L01.h> 
#include <RF24.h>
#include <Servo.h>

//#define DEBUG 

#define FREQUENCY       0x5c
#define PIPE            0xF0F0F0F0A1LL

#define SERVO_SPUSK     0 
#define SERVO_VSVOD     150
#define SERVO_READY     53


#define PIN_ENA 3 
#define PIN_ENB 6
#define PIN_IN1 2
#define PIN_IN2 4
#define PIN_IN3 8
#define PIN_IN4 7
#define PIN_SERVO 5

#define SERVO_STEP_DURATION 10 // Длительность 1го цикла сервопривода (нужно для уствновления блокировки)


#define PIN_CE  9  
#define PIN_CSN 10 

typedef struct MotorsValues {
  unsigned char P1;
  unsigned char P2;

  bool D11;
  bool D12;
  bool D21;
  bool D22;

  bool Shoot;
} MotorsValues;

int sput(char c, __attribute__((unused)) FILE* f) {return !Serial.write(c);}

#ifdef DEBUG 
void PrintValues(MotorsValues v){ printf("-----\nP1: %u D11: %s D12: %s\nP2: %u D21: %s D22: %s Shoot: %s\n", v.P1, v.D11?"true":"false", v.D12?"true":"false", v.P2, v.D21?"true":"false", v.D22?"true":"false",  v.Shoot?"true":"false"); }
#define DP(x) Serial.println(x)
#define PV(v) PrintValues(v)
#else 
#define DP(x)
#define PV(v) 
#endif


#define BoolToHL(b) ((b == 1) ? HIGH : LOW)

Servo shooter;

void InitMototrs(){
    pinMode(PIN_ENA, OUTPUT); 
    pinMode(PIN_ENB, OUTPUT); 
    pinMode(PIN_IN1, OUTPUT); 
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_IN3, OUTPUT);
    pinMode(PIN_IN4, OUTPUT);
    
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
}

bool Shoot = true;
bool ServoLocked = false;

void MotorsCommand(const MotorsValues &data){
    analogWrite(PIN_ENA, data.P1);
    analogWrite(PIN_ENB, data.P2);

    digitalWrite(PIN_IN1, BoolToHL(data.D11));
    digitalWrite(PIN_IN2, BoolToHL(data.D12));
    if (!ServoLocked){
      Shoot = data.Shoot;
    }

    digitalWrite(PIN_IN3, BoolToHL(data.D21));
    digitalWrite(PIN_IN4, BoolToHL(data.D22));
}



RF24 radio(PIN_CE, PIN_CSN); 
void initRadio(){
    if (!radio.begin()) {
        while (1) {
            Serial.println("radio hardware not responding!");
        }
    }

    radio.setChannel(FREQUENCY); 
    radio.setDataRate(RF24_1MBPS); 
    radio.setPALevel(RF24_PA_HIGH);
    radio.openReadingPipe(1, PIPE); 
    radio.startListening();
}

bool receiveData(MotorsValues &data) {
    if (radio.available()) {
        uint8_t bytes = radio.getPayloadSize();
        if (bytes >= sizeof(data)){
            radio.read((char*)&data, sizeof(data));
            return true;
        }
    }
    return false;
}

FILE f_out;

void setup() {
    #ifdef DEBUG 
        Serial.begin(9600);
        Serial.println("Begining Initialization");
    #endif
    fdev_setup_stream(&f_out, sput, nullptr, _FDEV_SETUP_WRITE); 
    stdout = &f_out;
    
    InitServo(); DP("Servo Initialized!");
    
    initRadio(); DP("Radio Initialized!"); 
    InitMototrs(); DP("Motors Initialized!");
}

MotorsValues motor;

long int LastShooted = 0;
int Pos = 0;
int targets[] = { SERVO_SPUSK, SERVO_VSVOD, SERVO_READY };
int cycle = 0;

void ShooterCycle(long int time){
    if (ServoLocked){
        if ((time - LastShooted) >= SERVO_STEP_DURATION){
            if (Pos < targets[cycle])
                shooter.write(++Pos);
            else
                shooter.write(--Pos);
            
            if (Pos == targets[cycle])
                cycle ++;

            if (cycle == 3)
                ServoLocked = false; 


            LastShooted = time;
        }
    }else if (Shoot){
        cycle = 0;
        ServoLocked = true;
        Shoot = false;
    }
}

void InitServo(){
    shooter.attach(PIN_SERVO);
    DP("Servo Attached");
    DP(shooter.attached());

    ServoLocked = false;
    Shoot = true;
    LastShooted  = millis();
}

void loop() {
    long int NowTime = millis();

    if (receiveData(motor)) {
        PV(motor);

        MotorsCommand(motor); DP("Motor Command Sended");
    }

    ShooterCycle(NowTime);
}
