#include <Arduino.h>

#include <nRF24L01.h> 
#include <RF24.h>

#define DEBUG 
// #define TEST_RESULT

#define CHANNEL 0x6f
#define PIPE 0xF0F0F0F0A1LL

#define PIN_FAST 3
#define PIN_SLOW 5
#define PIN_DIRECTION_STRAIGHT 2
#define PIN_DIRECTION_BACKWARDS 4
#define PIN_SHOOT 7
#define PIN_POT_LEFT A0
#define PIN_POT_RIGHT A3

#define PIN_CE  9  
#define PIN_CSN 10 

int sput(char c, __attribute__((unused)) FILE* f) {return !Serial.write(c);}

typedef struct MotorsValues {
  unsigned char P1;
  unsigned char P2;

  bool D11;
  bool D12;
  bool D21;
  bool D22;

  bool Shoot;
} MotorsValues;

typedef struct CalcPResult {
    unsigned char P;
    bool D1;
    bool D2;
} CalcPResult;

#ifdef DEBUG 
void PrintValues(MotorsValues v){ printf("-----\nP1: %u D11: %s D12: %s\nP2: %u D21: %s D22: %s Shoot: %s\n", v.P1, v.D11?"true":"false", v.D12?"true":"false", v.P2, v.D21?"true":"false", v.D22?"true":"false",  v.Shoot?"true":"false"); }
#define DP(x) Serial.println(x)
#define PV(v) PrintValues(v)
#else 
#define DP(x)
#define PV(v) 
#endif

CalcPResult CalcP(int p, bool d, int s, int f){
    CalcPResult r = {0};

    if (d){
        if (p > 512){
            r.P = map(p, 513, 1024, s, f);
            r.D1 = true;
            r.D2 = false;
        }else{
            r.P = map(p, 0, 512, f, s);
            r.D1 = false;
            r.D2 = true;
        }
    }else{
        if (p > 512){
            r.P = map(p, 513, 1024, s, f);
            r.D1 = false;
            r.D2 = true;
        }else{
            r.P = map(p, 0, 512, f, s);
            r.D1 = true;
            r.D2 = false;
        }
    }

    return r;
}

// PUB
// p1 and p2 in range of 0...1023
MotorsValues Calculate(int p1, int p2, bool fire, bool dir, bool slow, bool fast) {
    MotorsValues r = {0};
    r.Shoot = fire;

    int s = 0;
    int f = slow ? 70 : (fast ? 255 : 128);

    CalcPResult p1r = CalcP(p1, dir, s, f);
    CalcPResult p2r = CalcP(p2, dir, s, f);

    r.D11 = p1r.D1;
    r.D12 = p1r.D2;

    r.D21 = p2r.D1;
    r.D22 = p2r.D2;

    r.P1 = p1r.P;
    r.P2 = p2r.P;

    return r;
}


void TestCalc(void){
    if (!Serial){
        Serial.begin(9600);
    }

    PrintValues(Calculate(512, 1024, true, true, false, false));
    PrintValues(Calculate(512, 1024, true, true, true, false));
    PrintValues(Calculate(512, 1024, true, true, false, true));

    PrintValues(Calculate(512, 1024, false, false, false, false));
    PrintValues(Calculate(512, 1024, false, false, true, false));
    PrintValues(Calculate(512, 1024, false, false, false, true));

    PrintValues(Calculate(0, 512, true, true, false, false));
    PrintValues(Calculate(0, 512, true, true, true, false));
    PrintValues(Calculate(0, 512, true, true, false, true));

    PrintValues(Calculate(512, 512, false, false, false, false));
    PrintValues(Calculate(512, 512, false, false, true, false));
    PrintValues(Calculate(512, 512, false, false, false, true));

    PrintValues(Calculate(1024, 1024, true, true, false, false));
    PrintValues(Calculate(1024, 1024, true, true, true, false));
    PrintValues(Calculate(1024, 1024, true, true, false, true));

    PrintValues(Calculate(1024, 1024, false, false, false, false));
    PrintValues(Calculate(1024, 1024, false, false, true, false));
    PrintValues(Calculate(1024, 1024, false, false, false, true));

    PrintValues(Calculate(0, 0, true, true, false, false));
    PrintValues(Calculate(0, 0, true, true, true, false));
    PrintValues(Calculate(0, 0, true, true, false, true));

    PrintValues(Calculate(0, 0, false, false, false, false));
    PrintValues(Calculate(0, 0, false, false, true, false));
    PrintValues(Calculate(0, 0, false, false, false, true));
}

FILE f_out;
MotorsValues motor = {0};
bool direction = true;
RF24 radio(PIN_CE, PIN_CSN); 


void ReadMotorValues(){
    int pp1 = analogRead(PIN_POT_LEFT);
    int pp2 = analogRead(PIN_POT_RIGHT);

    bool fast = !digitalRead(PIN_FAST);
    bool slow = !digitalRead(PIN_SLOW);

    bool dStraight = !digitalRead(PIN_DIRECTION_STRAIGHT);
    bool dBackwards = !digitalRead(PIN_DIRECTION_BACKWARDS);

    bool shoot = !digitalRead(PIN_SHOOT);

    if (dStraight){
        direction = true;
    }else if (dBackwards){
        direction = false;
    }

    motor = Calculate(pp1, pp2, shoot, direction, slow, fast);
}

void writeData(const MotorsValues &data) {
    radio.write((char *)&data, sizeof(data));
}

void setup(){
    #ifdef DEBUG 
        Serial.begin(9600);
        Serial.println("Begining Initialization");
    #endif

    fdev_setup_stream(&f_out, sput, nullptr, _FDEV_SETUP_WRITE); 
    stdout = &f_out;
    DP("Test results:");
    #ifdef TEST_RESULT
        TestCalc();
    #endif

    pinMode(PIN_FAST, INPUT_PULLUP);
    pinMode(PIN_SLOW, INPUT_PULLUP);
    pinMode(PIN_DIRECTION_STRAIGHT, INPUT_PULLUP);
    pinMode(PIN_DIRECTION_BACKWARDS, INPUT_PULLUP);
    pinMode(PIN_SHOOT,INPUT_PULLUP);

    pinMode(PIN_POT_LEFT, INPUT);
    pinMode(PIN_POT_RIGHT, INPUT);

    DP("Initializing radio....");

    if (!radio.begin()) {
        while (1) {
            Serial.println("radio hardware not responding!");
        }
    }
    radio.setChannel(CHANNEL);
    radio.setDataRate(RF24_1MBPS); 
    radio.setPALevel(RF24_PA_HIGH); 
    radio.openWritingPipe(PIPE);
    DP("Radio initialized!");
}

void loop(){
    ReadMotorValues(); 
    PV(motor);    
    writeData(motor);
}
