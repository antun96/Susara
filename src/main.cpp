//#include <Arduino.h>
#include <Wire.h>
#include <max6675.h>
#include <EasyNextionLibrary.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include<SoftwareSerial.h>

#define RS485Transmit    HIGH
#define RS485Receive     LOW
SoftwareSerial SSerial(12, 13); // RX, TX
EasyNex myNex(Serial);

//Pin init

//input pins as interrupts
int goreSide = 2;
int termogenSide = 3;

//RelaySwitches
int ventSwitch = 4;
int plamenikSwitch = 5;
int mjesalicaSwitch = 6;
int goToRight = 7;
int goToLeft = 8;

//PlamenikTermometer
int thermoSCK = 9;
int thermoSO = 10;
int thermoCS = 11;

//SoftwareSerial Communication
int SRX = 12;
int STX = 13;
#define SSerialTxControl A0   //RS485 Direction control

float maxSeedTemp;
float maxTermTemp;
MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);

int CurrentPage = 0;
int currentPageAddress = 0;
int setMaxTermTempAddress = 1;
int setMaxSeedTempAddress = 2;

unsigned long time;
unsigned long timeForOtherStuff;
unsigned long timeForSerialReset;

unsigned long timeForMove;

unsigned long timeFromLastDirectionChange;
bool directionChangeToLeft = false;
bool directionChangeToRight = false;

unsigned long timeForStartMovingMixer;

bool moveButtonsPressed = false;
bool moveToRight = false;
bool moveToLeft = false;

unsigned long timeOfBurnerShutdown[20];
int numberOfShutdowns = 0;

//start procedures
unsigned long timeForStart;

unsigned long timeBurner;
unsigned long timeMixer;
unsigned long timeForShutdown;
unsigned long timeChangeDirection;
unsigned long timeForShutdownMixer;
unsigned long timeIntervalNextion;
unsigned long timeToLog;

int fanVentStart = 0;

bool stopMoving = false;

bool firstTimeForStart = false;
bool firstTimeBurner = false;
bool firstTimeMixer = false;

bool goreSideWatch = false;
bool termogenSideWatch = false;

unsigned long timeForOtherStuffInterval = 5000;
unsigned long timeInterval = 2000;
unsigned long timeForSerialResetInterval = 100;

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
bool termogenOverheated = false;
double tempOfThermogen;
double tempOfSeed = 0;
double tempOutside;
bool kosticePostigleTemp = false;
bool burnerState = false;
unsigned long timeDrying;
unsigned long timeBurnerOn;
unsigned long timeTurnOnBurner;
unsigned long timeTurnOffBurner;
unsigned long timeOfShutdownStart;

bool shutDownTemperatureReached = false;

bool readSeedTemperature = false;
bool sendLogMessage = false;

int calculateTime = 0;

bool lie = false;
unsigned long lieTime;
int turnCount = 0;

char receivedChar;
int stageNumber = 0;
char send[6];

const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data

bool newData = false;

float dataNumber = 0;  

void recvWithStartEndMarkers() {
    static bool recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (SSerial.available() > 0 && newData == false) {
        rc = SSerial.read();
       
        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
              
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void convertStringToFloat() {
  
    if (newData == true) {
        tempOfSeed = 0;             // new for this version
        tempOfSeed = atof(receivedChars);   // new for this version
        newData = false;
    }
}

void SendSSMessage(String messageBody){
  digitalWrite(SSerialTxControl, RS485Transmit);
  String message;
  message.concat(String("<"));
  message.concat(messageBody);
  message.concat(String(">"));
  SSerial.print(message);
  SSerial.flush();
  digitalWrite(SSerialTxControl, RS485Receive);
}

void trigger1(){      //start drying
  stopDrying = false;
  startDrying = true;
  doneBooting = false;
  CurrentPage = 1;
  EEPROM.update(currentPageAddress, CurrentPage);
}

void trigger8(){    //stop drying
  startDrying = false;
  doneBooting = false;
  stopDrying = true;
  CurrentPage = 0;
  EEPROM.update(currentPageAddress, CurrentPage);
}
void trigger2(){    //postavke from home
  CurrentPage = 2;
  myNex.writeNum("n0.val", (int)maxSeedTemp);
  myNex.writeNum("n1.val", (int)maxTermTemp);

}
void trigger7(){    //postavke from drying
  CurrentPage = 3;
  myNex.writeNum("n0.val", (int)maxSeedTemp);
  myNex.writeNum("n1.val", (int)maxTermTemp);

}
void trigger3(){    //move to left
  if(digitalRead(termogenSide) != LOW && moveButtonsPressed == false){
    digitalWrite(mjesalicaSwitch, HIGH);
    timeForStartMovingMixer = millis();
    moveToLeft = true;
    termogenSideWatch = true;
    moveButtonsPressed = true;
  }
  
}
void trigger4(){    //stop moving left
  moveToLeft = false;
  termogenSideWatch = false;
  digitalWrite(goToLeft, LOW);
  digitalWrite(mjesalicaSwitch, LOW);
  moveButtonsPressed = false;
  
}
void trigger5(){    //move to right
  if(digitalRead(goreSide) != LOW && moveButtonsPressed == false){
    digitalWrite(mjesalicaSwitch, HIGH);
    timeForStartMovingMixer = millis();
    moveToRight = true;
    goreSideWatch = true;
    moveButtonsPressed = true;
  }
  
}
void trigger6(){    //stop move to right
  moveToRight = false;
  goreSideWatch = false;
  digitalWrite(goToRight, LOW);
  digitalWrite(mjesalicaSwitch, LOW);
  moveButtonsPressed = false;
}
void trigger14(){    //back from settings to drying
  CurrentPage = 1;
  timeIntervalNextion = millis();
  calculateTime = (millis() - timeDrying)/1000/60;
  myNex.writeNum("n0.val", (int)tempOfThermogen);
  myNex.writeNum("n1.val", (int)tempOfSeed);
  myNex.writeNum("n2.val", (int)calculateTime);
  //EEPROM.update(currentPageAddress, CurrentPage);
}
void trigger9(void *ptr){   //back from settings to home
  CurrentPage = 0;
  //EEPROM.update(currentPageAddress, CurrentPage);
}
void trigger10(){    //DecrementTempOfSeed
  maxSeedTemp = maxSeedTemp - 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
    
  }
}
void trigger12(){   //DecrementTempOfTerm
  maxTermTemp = maxTermTemp - 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
    
  }

}
void trigger11(){   //IncrementTempOfSeed
  maxSeedTemp = maxSeedTemp + 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
  }

}
void trigger13(){    //IncrementTempOfTerm
  maxTermTemp = maxTermTemp + 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);  
  }

} 

void goLeft(){
  if(digitalRead(termogenSide != LOW)){
    startLeft = false;
    digitalWrite(goToRight, LOW);
    digitalWrite(goToLeft, HIGH);
    timeFromLastDirectionChange = millis();
    termogenSideWatch = true;
    directionChangeToLeft = true;
  } 
}

void goRight(){
  if(digitalRead(goreSide) != LOW){
    startRight = false;
    digitalWrite(goToLeft, LOW);
    digitalWrite(goToRight, HIGH);
    timeFromLastDirectionChange = millis();
    goreSideWatch = true;
    directionChangeToRight = true;
  }
}

void TurnDownPlamenik(){
  digitalWrite(plamenikSwitch, LOW);
  timeBurnerOn += (millis() - timeTurnOnBurner);
  burnerState = false;
}

void TurnOnPlamenik(){
  timeTurnOnBurner = millis();
  digitalWrite(plamenikSwitch, HIGH);
  burnerState = true;
}


void setup() { 
  
  //Serial.begin(9600);  // Start serial comunication at baud=9600

  myNex.begin(9600);
  SSerial.begin(4800);

  pinMode(termogenSide, INPUT);
  pinMode(goreSide, INPUT);
  pinMode(goToLeft, OUTPUT);
  pinMode(goToRight, OUTPUT);
  pinMode(SSerialTxControl, OUTPUT);


  pinMode(ventSwitch, OUTPUT);
  pinMode(plamenikSwitch, OUTPUT);
  pinMode(mjesalicaSwitch, OUTPUT);

  digitalWrite(goToLeft, LOW);
  digitalWrite(goToRight, LOW);
  digitalWrite(ventSwitch, LOW);
  digitalWrite(plamenikSwitch, LOW);
  digitalWrite(mjesalicaSwitch, LOW);
  digitalWrite(SSerialTxControl, RS485Receive);

  timeForOtherStuff = millis();
  timeIntervalNextion = millis();
  time = millis();

  //CurrentPage = myNex.readNumber("dp");
  //CurrentPage = EEPROM.read(currentPageAddress);
  maxSeedTemp = EEPROM.read(setMaxSeedTempAddress);
  maxTermTemp = EEPROM.read(setMaxTermTempAddress);

  // if(CurrentPage == 1){
  //   digitalWrite(ventSwitch, HIGH);
  //   delay(5000);
  //   digitalWrite(plamenikSwitch, HIGH);
  //   digitalWrite(mjesalicaSwitch, HIGH);
  //   startMixer = true;
  //   delay(2000);
  //   if(digitalRead(termogenSide) != LOW){
  //     goLeft();
  //   }else if(digitalRead(goreSide) != LOW){
  //     goRight();
  //   }
  //   stopDrying = false;
  //   startDrying = true;
  //   doneBooting = true;
  // }

  wdt_enable(WDTO_250MS);

}

void loop() {


  //receivedChar = Serial.read();
  //receive command for start
  if(receivedChar == 's'){
    if(startDrying == false){
      stopDrying = false;
      startDrying = true;
      doneBooting = false;
      CurrentPage = 1;
    }
  }else if(receivedChar == 'f'){      //receive command for stop
    if(startDrying == true){
      startDrying = false;
      doneBooting = false;
      stopDrying = true;
    }
  }
  //after start command is received, booting process begins
  if(startDrying == true && stopDrying == false && doneBooting == false){
    if(fanVentStart == 0){ 
      SendSSMessage("Start");
      digitalWrite(ventSwitch, HIGH);
      timeForStart = millis();
      startVentTermogen = true;
      fanVentStart = 1;
      //Serial.println("pali termogen");
    }
    if(5000 <= millis() - timeForStart && startVentTermogen == true){
      digitalWrite(plamenikSwitch, HIGH);         //starts burner, after some time termogen fan starts
      if(firstTimeBurner == false){
        timeBurner = millis();
        firstTimeBurner = true;
      }
      if(5000 <= millis() - timeBurner){
        digitalWrite(mjesalicaSwitch, HIGH);      //starts mixer
        startMixer = true;
        if(firstTimeMixer == false){
          timeMixer = millis();
          firstTimeMixer = true;
        }
        if(5000 <= millis() - timeMixer){         //start moving mixer
          if(digitalRead(termogenSide) != LOW){
            goLeft();
          }else if(digitalRead(goreSide) != LOW){
            goRight();
          }
          timeDrying = millis();
          doneBooting = true;
        }
      }
    }
    
    //procedure after boot is completed
  }else if(startDrying == true && stopDrying == false && doneBooting == true){
    if(timeForOtherStuffInterval  <= millis() - timeForOtherStuff){
      SendSSMessage("seed");
      recvWithStartEndMarkers();
      convertStringToFloat();
      tempOfThermogen = thermocouple.readCelsius();

      // if(tempOfSeed == 99){
      //   tempOfSeed = 0;
      // }
      
      if(tempOfSeed > maxSeedTemp && kosticePostigleTemp == false){
      kosticePostigleTemp = true;
      //timeOfBurnerShutdown[numberOfShutdowns] = millis();
      //numberOfShutdowns++;
      }else if(tempOfSeed < 35){
        kosticePostigleTemp = false;
      }

      if(tempOfThermogen > maxTermTemp || kosticePostigleTemp == true){
        TurnDownPlamenik();
        termogenOverheated = true;
        //Serial.println("hladi");
      }
      else if(kosticePostigleTemp == true && tempOfSeed <= 30){
        TurnOnPlamenik();
        kosticePostigleTemp = false;
        
        //Serial.println("grije");
      // } else if(kosticePostigleTemp == false && (tempOfSeed < 30 || tempOfThermogen < maxTermTemp - 15)){
      } else if(kosticePostigleTemp == false && termogenOverheated == true && tempOfThermogen < maxTermTemp - 15){

        TurnOnPlamenik();
        termogenOverheated = false;
        //Serial.println("grije");
      }
      //logic to find out when to shutdown dryer
      // if(numberOfShutdowns >= 5){
      //   if(timeOfBurnerShutdown[numberOfShutdowns - 1] - timeOfBurnerShutdown[numberOfShutdowns - 5] < 3600000){
      //     startDrying = false;
      //     stopDrying = true;
      //     doneBooting = false;
      //     digitalWrite(plamenikSwitch, LOW);
      //     timeOfShutdownStart = millis();
      //   }
     
      // }
        timeForOtherStuff = millis();
    }

    if(startMixer == true && startLeft == true && timeInterval < millis() - time){
      goLeft();
    }
    if(startMixer == true && startRight == true && timeInterval < millis() - time){
      goRight();
    }

//shutdown procedure
  }else if(startDrying == false && stopDrying == true && doneBooting == false){
    //digitalWrite(plamenikSwitch, LOW);
    if(5000 < millis() - timeForOtherStuff){
      SendSSMessage("seed");
      recvWithStartEndMarkers();
      convertStringToFloat();
      
      timeForOtherStuff = millis();

    }

    if(tempOfSeed < 25){
    //if((1200000 < millis() - timeOfShutdownStart && tempOfSeed <= (tempOutside + 4)) || shutDownTemperatureReached == true){
      shutDownTemperatureReached = true;
      
      //odi skroz u lijevo
      if(startMixer == true && startLeft == true && timeInterval < millis() - time){
        goLeft();
      }else if(startRight == true && startMixer == true && timeInterval < millis() - time){
          startMixer = false;
          digitalWrite(mjesalicaSwitch, LOW);
          digitalWrite(goToLeft, LOW);
          digitalWrite(goToRight, LOW);
          digitalWrite(ventSwitch, LOW);
          startVentTermogen = false;
          fanVentStart = 0;
          firstTimeBurner = false;
          startMixer = false;
          firstTimeMixer = false;
          startLeft = false;
          startRight = false;
          doneBooting = true;
          stopDrying = false;
          startDrying = false;
          stopMoving = false;
          shutDownTemperatureReached = false;
      }
      // if(startLeft == true  &&  stopMoving == false){
      //   digitalWrite(goToRight, LOW);
      //   timeMixer = millis();
      //   stopMoving = true;
        
      // }else if(5000 <= millis() - timeMixer && stopMoving == true){
      //     startMixer = false;
      //     digitalWrite(mjesalicaSwitch, LOW);
      //     digitalWrite(ventSwitch, LOW);
      //     digitalWrite(ventSwitch, LOW);
      //     startVentTermogen = false;
      //     fanVentStart = 0;
      //     firstTimeBurner = false;
      //     startMixer = false;
      //     firstTimeMixer = false;
      //     startLeft = false;
      //     startRight = false;
      //     doneBooting = true;
      //     stopDrying = false;
      //     startDrying = false;
      //     stopMoving = false;
      //   }
        
    } else {
      if(startMixer == true && startLeft == true && timeInterval < millis() - time){
        goLeft();
      }
      if(startMixer == true && startRight == true && timeInterval < millis() - time){
        goRight();
      }
    }
  }
  //end of shutdown procedure

  if(moveButtonsPressed == true){
    //turn on posmak when button is pressed
    if(moveToLeft == true && 500 <= (millis() - timeForStartMovingMixer) && digitalRead(termogenSide) != LOW){
      digitalWrite(goToLeft, HIGH);
    }
    //shutdown posmak when button is not pressed
    if(digitalRead(termogenSide) == LOW && moveToLeft == true){
      digitalWrite(goToLeft, LOW);
    }
    //turn on posmak when button is pressed
    if(moveToRight == true && 500 <= (millis() - timeForStartMovingMixer) && digitalRead(goreSide) != LOW){
      digitalWrite(goToRight, HIGH);
    }
    //shutdown posmak when button is not pressed
    if(digitalRead(goreSide) == LOW && moveToRight == true){
      digitalWrite(goToRight, LOW);
    }
  }

  if(CurrentPage == 1 && 2500 < millis() - timeIntervalNextion)
  {
      calculateTime = (millis() - timeDrying)/1000/60;
      myNex.writeNum("n0.val", (int)tempOfThermogen);
      myNex.writeNum("n1.val", (int)tempOfSeed);
      myNex.writeNum("n2.val", (int)calculateTime);

      timeIntervalNextion = millis();    
  }

  if(digitalRead(termogenSide) == LOW && termogenSideWatch == true && moveButtonsPressed == false){
      digitalWrite(goToLeft, LOW);
      termogenSideWatch = false;
      moveToLeft = false;
      startLeft = false;
      startRight = true;
      time = millis();
      timeChangeDirection = millis();
      // Serial.print("Desno");
      // Serial.print("\n");
  }
if(digitalRead(goreSide) == LOW && goreSideWatch == true && moveButtonsPressed == false){
    digitalWrite(goToRight, LOW);
    goreSideWatch = false;
    moveToRight = false;
    startRight = false;
    startLeft = true;
    time = millis();
    timeChangeDirection = millis();
    // Serial.print("Lijevo");
    // Serial.print("\n");
}

//log process of drying
if(60000 >= millis() - timeToLog && startDrying == true){
  String message;
  message.concat(calculateTime);
  message.concat(",");
  message.concat(timeBurnerOn/60000);
  message.concat(",");
  message.concat(tempOfThermogen);
  message.concat(",");
  message.concat(burnerState);
  message.concat(",");
  message.concat(tempOfSeed);
  message.concat(",");
  message.concat(kosticePostigleTemp);
  SendSSMessage(message);
  timeToLog = millis();
}

myNex.NextionListen();
wdt_reset();
//nexLoop(nex_listen_list);

// reset serial to enable communication
// if(!Serial){
//     Serial.end();
//     resetSerial = true;
//     timeForSerialReset = millis();
//   }
// if(resetSerial == true && timeForSerialResetInterval <= millis() - timeForSerialReset){
//   Serial.begin(9600);
// }



}



