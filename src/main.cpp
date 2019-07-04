//#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <max6675.h>

unsigned long time;
unsigned long timeForOtherStuff;
unsigned long timeForSerialReset;

unsigned long timeForMove;

unsigned long timeOfBurnerShutdown[20];
int numberOfShutdowns = 0;

//start procedures
unsigned long timeForStart;
unsigned long timeBurner;
unsigned long timeMixer;
unsigned long timeForShutdown;
unsigned long timeChangeDirection;
bool firstTimeForStart = false;
bool firstTimeBurner = false;
bool firstTimeMixer = false;


unsigned long timeForOtherStuffInterval = 5000;
unsigned long timeInterval = 2000;
unsigned long timeForSerialResetInterval = 100;

int thermoDO = 8;
int thermoCS = 7;
int thermoCLK = 6;

//endSensor
int termogenSide = 2;
int goreSide = 3;

int termogenVentSwitch = 9;
int plamenikSwitch = 10;

int mjesalicaSwitch = 11;
int goToRight = A0;
int goToLeft = 12;
bool startLeft = false;
bool startRight = false;

bool hitRight = false;
bool hitLeft = false;

bool startDrying = false;
bool doneBooting = false;

bool stopDrying = false;

bool resetSerial = false;
String order;

bool startVentTermogen = false;
bool startMixer = false;
float tempOfThermogen;
float tempOfSeed;
bool kosticePostigleTemp = false;

char receivedChar;
bool newData = false;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

void goLeft(){
  digitalWrite(goToRight, LOW);
  startLeft = true;
  startRight = false;
  hitLeft = false;
  hitRight = true;
  time = millis();
  timeChangeDirection = millis();
}
void goRight(){
  digitalWrite(goToLeft, LOW);
  startRight = true;
  startLeft = false;
  hitRight = false;
  hitLeft = true;
  time = millis();
  timeChangeDirection = millis();
}

void setup() {
  pinMode(termogenSide, INPUT);
  pinMode(goreSide, INPUT);
  pinMode(goToLeft, OUTPUT);
  pinMode(goToRight, OUTPUT);

  pinMode(termogenVentSwitch, OUTPUT);
  pinMode(plamenikSwitch, OUTPUT);
  pinMode(mjesalicaSwitch, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(termogenSide), goLeft, FALLING);
  attachInterrupt(digitalPinToInterrupt(goreSide), goRight, FALLING);
  
  Serial.begin(9600);
  Serial.println("Adafruit MLX90614 test");  
  Serial.println(timeForOtherStuffInterval);
  mlx.begin();
  timeForOtherStuff = millis();
}

void loop() {
  if (Serial.available() > 0) {
        receivedChar = Serial.read();
        newData = true;
    }
  //receive command for start
  if(receivedChar == 's'){
    if(startDrying == false){
      stopDrying = false;
      startDrying = true;
      doneBooting = false;
    }
  }else if(receivedChar == 'f'){      //receive command for stop
    if(startDrying == true){
      startDrying = false;
      doneBooting = false;
      stopDrying = true;
    }
  }else if(receivedChar == 'l'){
    digitalWrite(mjesalicaSwitch, HIGH);
    startMixer = true;
    timeMixer = millis();
    if(1000 <= millis() - timeMixer && startMixer == true && hitLeft == false){
      digitalWrite(goToLeft, HIGH);
      timeForMove = millis();
    }else if(500 < millis() - timeForMove){
      digitalWrite(goToLeft, LOW);
      digitalWrite(mjesalicaSwitch,LOW);
      startMixer = false;
    }
  }else if(receivedChar == 'r'){
    digitalWrite(mjesalicaSwitch, HIGH);
    startMixer = true;
    timeMixer = millis();
    if(1000 <= millis() - timeMixer && startMixer == true && hitRight == false){
      digitalWrite(goToRight, HIGH);
      timeForMove = millis();
    }else if(500 < millis() - timeForMove){
      digitalWrite(goToRight, LOW);
      digitalWrite(mjesalicaSwitch,LOW);
      startMixer = false;
    }
  }
  //after start command is received, booting process begins
  if(startDrying == true && stopDrying == false && doneBooting == false){
    digitalWrite(termogenVentSwitch, HIGH);       //starts squirrel fan and gives power to termogen
    startVentTermogen = true;
    if(firstTimeForStart == false){
      timeForStart = millis();
      firstTimeForStart = true;
    }
    if(5000 <= millis() - timeForStart){
      digitalWrite(plamenikSwitch, HIGH);         //starts burner, after some time termogen fan starts
      if(firstTimeBurner == false){
        timeBurner = millis();
        firstTimeBurner = true;
      }
      if(10000 <= millis() - timeBurner){
        digitalWrite(mjesalicaSwitch, HIGH);      //starts mixer
        startMixer = true;
        if(firstTimeMixer == false){
          timeMixer = millis();
          firstTimeMixer = true;
        }
        if(5000 <= millis() - timeMixer){         //start moving mixer
          goLeft();
          if(startMixer == true && startLeft == true && timeInterval < millis() - time){
            startLeft = false;
            digitalWrite(goToRight, LOW);
            digitalWrite(goToLeft, HIGH);
          }else if(startMixer == true && startRight == true && timeInterval < millis() - time){
            startRight = false;
            digitalWrite(goToLeft, LOW);
            digitalWrite(goToRight, HIGH);
          } 
          doneBooting = true;
        }
      }
    }
  }else if(startDrying == true && stopDrying == false && doneBooting == true){
    if(timeForOtherStuffInterval <= millis() - timeForOtherStuff){
      //Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempC()); 
      Serial.print("*C\tObject = "); Serial.print(mlx.readObjectTempC()); Serial.println("*C");
      //Serial.print("C = "); 
      //Serial.println(thermocouple.readCelsius());

    tempOfThermogen = thermocouple.readCelsius();
    Serial.print(thermocouple.readCelsius());

    float tempOfSeed = mlx.readAmbientTempC();
    // check if returns are valid, if they are NaN (not a number) then something went wrong!
    if (isnan(tempOfSeed) || isnan(tempOfThermogen)) 
    {
      Serial.println("Failed to read from DHT");
      tempOfSeed = 50;
    } 
    else 
    {
      Serial.print("TemperaturaKosica: "); 
      Serial.print(tempOfSeed);
      Serial.println(" *C");
    }
    if(tempOfSeed > 55){
    kosticePostigleTemp = true;
    }

    if(tempOfThermogen > 79 || kosticePostigleTemp == true){
    digitalWrite(plamenikSwitch, LOW);
    Serial.println("hladi");
    }
    else if(kosticePostigleTemp == true && tempOfSeed <= 30){
      digitalWrite(plamenikSwitch, HIGH);
      kosticePostigleTemp = false;
      timeOfBurnerShutdown[numberOfShutdowns] = millis();
      numberOfShutdowns++;
      Serial.println("grije");
    } else if(kosticePostigleTemp == false && (tempOfSeed < 30 || tempOfThermogen < 60)){
      digitalWrite(plamenikSwitch, HIGH);
      Serial.println("grije");
    }
    //logic to find out when to shutdown dryer
    if(numberOfShutdowns >= 5){
      if(timeOfBurnerShutdown[numberOfShutdowns - 1] - numberOfShutdowns[numberOfShutdowns - 5] < 3600000){
        startDrying = false;
        stopDrying = true;
        doneBooting = false;
      }
     
    }
      timeForOtherStuff = millis();
    }

    if(startMixer == true && startLeft == true && timeInterval < millis() - time){
      startLeft = false;
      digitalWrite(goToRight, LOW);
      digitalWrite(goToLeft, HIGH);
    }
    if(startMixer == true && startRight == true && timeInterval < millis() - time){
      startRight = false;
      digitalWrite(goToLeft, LOW);
      digitalWrite(goToRight, HIGH);
    }

//shutdown procedure
  }else if(startDrying == false && stopDrying == true && doneBooting == false){
    digitalWrite(plamenikSwitch, LOW);
    tempOfThermogen = thermocouple.readCelsius();
    Serial.println("termogen: ");
    Serial.println(tempOfThermogen);
    Serial.println("ambient temp: ");
    //Serial.println(mlx.readAmbientTempC() +5);
    if(mlx.readObjectTempC() < mlx.readAmbientTempC() + 8 || mlx.readObjectTempC() < tempOfThermogen + 8){
      if(hitRight == true){
        digitalWrite(goToRight, LOW);
        timeMixer = millis();
        if(5000 <= millis() - timeMixer){
          startMixer = false;
          digitalWrite(mjesalicaSwitch, LOW);
          digitalWrite(termogenVentSwitch, LOW);
          doneBooting = true;
          stopDrying = false;
          startDrying = false;
        }
      }
    }
  }
// reset serial to enable communication
  if(!Serial){
      Serial.end();
      resetSerial = true;
      timeForSerialReset = millis();
    }
    if(resetSerial == true && timeForSerialResetInterval <= millis() - timeForSerialReset){
      Serial.begin(9600);
    }
}



