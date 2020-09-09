//#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <max6675.h>
#include <EasyNextionLibrary.h>
#include <avr/wdt.h>
#include <EEPROM.h>


EasyNex myNex(Serial);

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

int thermoSO = 10; //8
int thermoCS = 11; //7
int thermoSCK = 9; //6

//endSensor
int termogenSide = 3;
int goreSide = 2;

int termogenVentSwitch = 6;
int plamenikSwitch = 7;

int mjesalicaSwitch = 8;
int goToRight = A0;
int goToLeft = A1;
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
double tempOfThermogen;
double tempOfSeed;
double tempOutside;
bool kosticePostigleTemp = false;
unsigned long timeDrying;
unsigned long timeOfShutdownStart;

bool shutDownTemperatureReached = false;

int calculateTime = 0;

bool lie = false;
unsigned long lieTime;
int turnCount = 0;

char receivedChar;
bool newData = false;

float maxSeedTemp = 55;
float maxTermTemp = 48;

int stageNumber = 0;
char send[6];

MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

int CurrentPage = 0;



// NexButton Start = NexButton(0, 1, "b0");
// NexButton PostavkeHome = NexButton(0, 2, "b1");
// NexButton PosmakLeft = NexButton(0, 3, "b2");
// NexButton PosmakRight = NexButton(0, 4, "b3");
// NexButton PostavkeDry = NexButton(1, 2, "b1");
// NexButton EndDry = NexButton(1, 1, "b0");
// NexButton DecrementTempOfSeedHome = NexButton(2, 2, "b0");
// NexButton IncrementTempOfSeedHome = NexButton(2, 3, "b1");
// NexButton DecrementTempOfTermHome = NexButton(2, 7, "b2");
// NexButton IncrementTempOfTermHome = NexButton(2, 8, "b3");
// NexButton BackFromSettingsHome = NexButton(2, 10, "b4");
// NexButton BackFromSettingsDry = NexButton(3, 10, "b4");
// NexButton DecrementTempOfSeedDry = NexButton(3, 2, "b0");
// NexButton IncrementTempOfSeedDry = NexButton(3, 3, "b1");
// NexButton DecrementTempOfTermDry = NexButton(3, 7, "b2");
// NexButton IncrementTempOfTermDry = NexButton(3, 8, "b3");
// NexNumber TermNext = NexNumber(1, 7, "n0");
// NexNumber SeedNext = NexNumber(1, 8, "n1");
// NexNumber TimeNext = NexNumber(1, 9, "n2");
// NexNumber maxTermTempMenu = NexNumber(2, 9, "n1");
// NexNumber maxTermTempDry = NexNumber(3, 9, "n1");
// NexNumber maxSeedTempMenu = NexNumber(2, 4, "n0");
// NexNumber maxSeedTempDry = NexNumber(3, 4, "n0");



// NexPage page0 = NexPage(0, 0, "page0");  // Page added as a touch event
// NexPage page1 = NexPage(1, 0, "page1");  // Page added as a touch event
// NexPage page2 = NexPage(2, 0, "page2");  // Page added as a touch event
// NexPage page3 = NexPage(3, 0, "page3");

// NexTouch *nex_listen_list[] = {
//   &Start,
//   &PostavkeHome,
//   &PosmakLeft,
//   &PosmakRight,
//   &PostavkeDry,
//   &EndDry,
//   &DecrementTempOfSeedHome,
//   &IncrementTempOfSeedHome,
//   &DecrementTempOfTermHome,
//   &IncrementTempOfTermHome,
//   &BackFromSettingsHome,
//   &BackFromSettingsDry,
//   &DecrementTempOfSeedDry,
//   &IncrementTempOfSeedDry,
//   &DecrementTempOfTermDry,
//   &IncrementTempOfTermDry,
//   &TermNext,
//   &SeedNext,
//   &TimeNext,
//   &maxTermTempMenu,
//   &maxTermTempDry,
//   &maxSeedTempMenu,
//   &maxSeedTempDry,
//   NULL
// };

void trigger1(){      //start drying
  stopDrying = false;
  startDrying = true;
  doneBooting = false;
  CurrentPage = 1;
}

void trigger8(){    //stop drying
  startDrying = false;
  doneBooting = false;
  stopDrying = true;
  CurrentPage = 0;
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
void trigger14(void *ptr){    //back from settings to drying
  CurrentPage = 1;
  timeIntervalNextion = millis();
}
void trigger9(void *ptr){   //back from settings to home
  CurrentPage = 0;
}
void trigger10(){    //DecrementTempOfSeed
  maxSeedTemp = maxSeedTemp - 1;
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
  }
}
void trigger12(){   //DecrementTempOfTerm
  maxTermTemp = maxTermTemp - 1;
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
  }

}
void trigger11(){   //IncrementTempOfSeed
  maxSeedTemp = maxSeedTemp + 1;
  if(CurrentPage == 3 || CurrentPage == 2){
    myNex.writeNum("n0.val", (int)maxSeedTemp);
    myNex.writeNum("n1.val", (int)maxTermTemp);
  }

}
void trigger13(){    //IncrementTempOfTerm
  maxTermTemp = maxTermTemp + 1;
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


void setup() { 
  
  //Serial.begin(9600);  // Start serial comunication at baud=9600

  myNex.begin(9600);

  pinMode(termogenSide, INPUT);
  pinMode(goreSide, INPUT);
  pinMode(goToLeft, OUTPUT);
  pinMode(goToRight, OUTPUT);


  pinMode(termogenVentSwitch, OUTPUT);
  pinMode(plamenikSwitch, OUTPUT);
  pinMode(mjesalicaSwitch, OUTPUT);

  digitalWrite(goToLeft, LOW);
  digitalWrite(goToRight, LOW);
  digitalWrite(termogenVentSwitch, LOW);
  digitalWrite(plamenikSwitch, LOW);
  digitalWrite(mjesalicaSwitch, LOW);

  Wire.setClock(10000);
  mlx.begin();
  timeForOtherStuff = millis();
  timeIntervalNextion = millis();
  time = millis();

  CurrentPage = myNex.currentPageId;
  if(CurrentPage == 1){
    digitalWrite(termogenVentSwitch, HIGH);
    digitalWrite(plamenikSwitch, HIGH);
    digitalWrite(mjesalicaSwitch, HIGH);
    if(digitalRead(termogenSide) != LOW){
      goLeft();
    }else if(digitalRead(goreSide) != LOW){
      goRight();
    }
    stopDrying = false;
    startDrying = true;
    doneBooting = true;
  }

  wdt_enable(WDTO_60MS);

}

void loop() {


  //receivedChar = Serial.read();
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
  }
  //after start command is received, booting process begins
  if(startDrying == true && stopDrying == false && doneBooting == false){
    if(fanVentStart == 0){  
      digitalWrite(termogenVentSwitch, HIGH);
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
    if(timeForOtherStuffInterval <= millis() - timeForOtherStuff){
      // Serial.print("citam termogen");
      tempOfThermogen = thermocouple.readCelsius();
      // Serial.println(tempOfThermogen);
      // Serial.println("citam kostice");
      tempOfSeed = mlx.readObjectTempC();
      // Serial.println(tempOfSeed);
      // Serial.println("citam vanjsku temp");
      tempOutside = mlx.readAmbientTempC();
      // Serial.println(tempOutside);
      // Serial.println("Procitao");
      
      if(tempOfSeed > maxSeedTemp && kosticePostigleTemp == false){
      kosticePostigleTemp = true;
      timeOfBurnerShutdown[numberOfShutdowns] = millis();
      numberOfShutdowns++;
      }else if(tempOfSeed <35){
        kosticePostigleTemp = false;
      }

      if(tempOfThermogen > maxTermTemp || kosticePostigleTemp == true){
      digitalWrite(plamenikSwitch, LOW);
      //Serial.println("hladi");
      }
      else if(kosticePostigleTemp == true && tempOfSeed <= 30){
        digitalWrite(plamenikSwitch, HIGH);
        kosticePostigleTemp = false;
        
        //Serial.println("grije");
      } else if(kosticePostigleTemp == false && (tempOfSeed < 30 || tempOfThermogen < maxTermTemp - 15)){
        digitalWrite(plamenikSwitch, HIGH);
        //Serial.println("grije");
      }
      //logic to find out when to shutdown dryer
      if(numberOfShutdowns >= 5){
        if(timeOfBurnerShutdown[numberOfShutdowns - 1] - timeOfBurnerShutdown[numberOfShutdowns - 5] < 3600000){
          startDrying = false;
          stopDrying = true;
          doneBooting = false;
          digitalWrite(plamenikSwitch, LOW);
          timeOfShutdownStart = millis();
        }
     
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
      // Serial.println("citam termogen");
      // tempOfThermogen = thermocouple.readCelsius();
      // Serial.println("citam kostice");
      // tempOfSeed = mlx.readObjectTempC();
      // Serial.println("citam vanjsku temp");
      // tempOutside = mlx.readAmbientTempC();
      // Serial.println("Procitao");
      
      timeForOtherStuff = millis();

    }

    // if(tempOfSeed < (tempOutside + 4)){
    if((1200000 < millis() - timeOfShutdownStart && tempOfSeed <= (tempOutside + 4)) || shutDownTemperatureReached == true){
      shutDownTemperatureReached = true;
      
      //odi skroz u lijevo
      if(startMixer == true && startLeft == true && timeInterval < millis() - time){
        goLeft();
      }else if(startRight == true && startMixer == true && timeInterval < millis() - time){
          startMixer = false;
          digitalWrite(mjesalicaSwitch, LOW);
          digitalWrite(goToLeft, LOW);
          digitalWrite(goToRight, LOW);
          digitalWrite(termogenVentSwitch, LOW);
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
      //     digitalWrite(termogenVentSwitch, LOW);
      //     digitalWrite(termogenVentSwitch, LOW);
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
      Serial.print("Desno");
      Serial.print("\n");
  }
if(digitalRead(goreSide) == LOW && goreSideWatch == true && moveButtonsPressed == false){
    digitalWrite(goToRight, LOW);
    goreSideWatch = false;
    moveToRight = false;
    startRight = false;
    startLeft = true;
    time = millis();
    timeChangeDirection = millis();
    Serial.print("Lijevo");
    Serial.print("\n");
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



