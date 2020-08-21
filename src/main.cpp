//#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <max6675.h>
#include <Nextion.h>
#include <SD.h>
#include <SoftwareSerial.h>

unsigned long time;
unsigned long timeForOtherStuff;
unsigned long timeForSerialReset;

unsigned long timeForMove;

unsigned long timeFromLastDirectionChange;
bool directionChangeToLeft = false;
bool directionChangeToRight = false;

unsigned long timeForStartMovingMixer;
bool moveButtonsPressed = false;
bool moveToLeft = false;
bool moveToRight = false;

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

bool goreSideAttachedInterrupt = false;
bool termogenSideAttachedInterrupt = false;

unsigned long timeForOtherStuffInterval = 5000;
unsigned long timeInterval = 2000;
unsigned long timeForSerialResetInterval = 100;

int thermoSO = 10; //8
int thermoCS = 11; //7
int thermoSCK = 9; //6

//endSensor
int termogenSide = 2;
int goreSide = 3;



int termogenVentSwitch = 6;
int plamenikSwitch = 7;

int mjesalicaSwitch = 8;
int goToRight = A1;
int goToLeft = A0;
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

int calculateTime = 0;

bool lie = false;
unsigned long lieTime;
int turnCount = 0;

char receivedChar;
bool newData = false;

float maxSeedTemp = 55;
float maxTermTemp = 65;

int stageNumber = 0;
char send[6];

MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

int CurrentPage = 0;



NexButton Start = NexButton(0, 1, "b0");
NexButton PostavkeHome = NexButton(0, 2, "b1");
NexButton PosmakLeft = NexButton(0, 3, "b2");
NexButton PosmakRight = NexButton(0, 4, "b3");
NexButton PostavkeDry = NexButton(1, 2, "b1");
NexButton EndDry = NexButton(1, 1, "b0");
NexButton DecrementTempOfSeedHome = NexButton(2, 2, "b0");
NexButton IncrementTempOfSeedHome = NexButton(2, 3, "b1");
NexButton DecrementTempOfTermHome = NexButton(2, 7, "b2");
NexButton IncrementTempOfTermHome = NexButton(2, 8, "b3");
NexButton BackFromSettingsHome = NexButton(2, 10, "b4");
NexButton BackFromSettingsDry = NexButton(3, 10, "b4");
NexButton DecrementTempOfSeedDry = NexButton(3, 2, "b0");
NexButton IncrementTempOfSeedDry = NexButton(3, 3, "b1");
NexButton DecrementTempOfTermDry = NexButton(3, 7, "b2");
NexButton IncrementTempOfTermDry = NexButton(3, 8, "b3");
NexNumber TermNext = NexNumber(1, 7, "n0");
NexNumber SeedNext = NexNumber(1, 8, "n1");
NexNumber TimeNext = NexNumber(1, 9, "n2");
NexNumber maxTermTempMenu = NexNumber(2, 9, "n1");
NexNumber maxTermTempDry = NexNumber(3, 9, "n1");
NexNumber maxSeedTempMenu = NexNumber(2, 4, "n0");
NexNumber maxSeedTempDry = NexNumber(3, 4, "n0");



NexPage page0 = NexPage(0, 0, "page0");  // Page added as a touch event
NexPage page1 = NexPage(1, 0, "page1");  // Page added as a touch event
NexPage page2 = NexPage(2, 0, "page2");  // Page added as a touch event
NexPage page3 = NexPage(3, 0, "page3");

NexTouch *nex_listen_list[] = {
  &Start,
  &PostavkeHome,
  &PosmakLeft,
  &PosmakRight,
  &PostavkeDry,
  &EndDry,
  &DecrementTempOfSeedHome,
  &IncrementTempOfSeedHome,
  &DecrementTempOfTermHome,
  &IncrementTempOfTermHome,
  &BackFromSettingsHome,
  &BackFromSettingsDry,
  &DecrementTempOfSeedDry,
  &IncrementTempOfSeedDry,
  &DecrementTempOfTermDry,
  &IncrementTempOfTermDry,
  &TermNext,
  &SeedNext,
  &TimeNext,
  &maxTermTempMenu,
  &maxTermTempDry,
  &maxSeedTempMenu,
  &maxSeedTempDry,
  NULL
};

void StartPopCallback(void *ptr)  // Release event for button b1
{
  stopDrying = false;
  startDrying = true;
  doneBooting = false;
  CurrentPage = 1;
}
void EndDryPopCallBack(void *ptr){
  startDrying = false;
  doneBooting = false;
  stopDrying = true;
  CurrentPage = 0;
}
void PostavkeHomePopCallBack(void *ptr){
  CurrentPage = 2;
  maxSeedTempMenu.setValue((int)maxSeedTemp);
  maxTermTempMenu.setValue((int)maxTermTemp);
  // Serial.print("n0.val=");
  // Serial.print((int)maxSeedTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.print("n1.val=");
  // Serial.print((int)maxTermTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void PostavkeDryPopCallBack(void *ptr){
  CurrentPage = 3;
  maxSeedTempDry.setValue((int)maxSeedTemp);
  maxTermTempDry.setValue((int)maxTermTemp);
  // Serial.print("n0.val=");
  // Serial.print((int)maxSeedTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.print("n1.val=");
  // Serial.print((int)maxTermTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void PosmakLeftPushCallBack(void *ptr){
  if(digitalRead(goreSide) == HIGH){
    digitalWrite(mjesalicaSwitch, HIGH);
    timeForStartMovingMixer = millis();
    moveToLeft = true;
    moveButtonsPressed = true;
  }
  
}
void PosmakLeftPopCallBack(void *ptr){
  moveToLeft = false;
  digitalWrite(goToLeft, LOW);
  delay(500);
  digitalWrite(mjesalicaSwitch, LOW);
  
}
void PosmakRightPushCallBack(void *ptr){
  if(digitalRead(termogenSide) == HIGH){
    digitalWrite(mjesalicaSwitch, HIGH);
    timeForStartMovingMixer = millis();
    moveToRight = true;
    moveButtonsPressed = true;
  }
  
}
void PosmakRightPopCallBack(void *ptr){
  moveToRight = false;
  digitalWrite(goToRight, LOW);
  delay(500);
  digitalWrite(mjesalicaSwitch, LOW);
  
}
void BackToDry(void *ptr){
  CurrentPage = 1;
}
void BackToHome(void *ptr){
  CurrentPage = 0;
}
void DecrementTempOfSeed(void *ptr){
  maxSeedTemp = maxSeedTemp - 1;
  if(CurrentPage == 3){
    maxSeedTempDry.setValue((int)maxSeedTemp);
    maxTermTempDry.setValue((int)maxTermTemp);
  }else if(CurrentPage == 2){
    maxSeedTempMenu.setValue((int)maxSeedTemp);
    maxTermTempMenu.setValue((int)maxTermTemp);
  }
  
  // Serial.print("n0.val=");
  // Serial.print((int)maxSeedTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void DecrementTempOfTerm(void *ptr){
  maxTermTemp = maxTermTemp - 1;
  if(CurrentPage == 3){
    maxSeedTempDry.setValue((int)maxSeedTemp);
    maxTermTempDry.setValue((int)maxTermTemp);
  }else if(CurrentPage == 2){
    maxSeedTempMenu.setValue((int)maxSeedTemp);
    maxTermTempMenu.setValue((int)maxTermTemp);
  }
  //need to send data to display
  // Serial.print("n1.val=");
  // Serial.print((int)maxTermTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void IncrementTempOfSeed(void *ptr){
  maxSeedTemp = maxSeedTemp + 1;
  if(CurrentPage == 3){
    maxSeedTempDry.setValue((int)maxSeedTemp);
    maxTermTempDry.setValue((int)maxTermTemp);
  }else if(CurrentPage == 2){
    maxSeedTempMenu.setValue((int)maxSeedTemp);
    maxTermTempMenu.setValue((int)maxTermTemp);
  }
  //need to send data to display
  // Serial.print("n0.val=");
  // Serial.print((int)maxSeedTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void IncrementTempOfTerm(void *ptr){
  maxTermTemp = maxTermTemp + 1;
  if(CurrentPage == 3){
    maxSeedTempDry.setValue((int)maxSeedTemp);
    maxTermTempDry.setValue((int)maxTermTemp);
  }else if(CurrentPage == 2){
    maxSeedTempMenu.setValue((int)maxSeedTemp);
    maxTermTempMenu.setValue((int)maxTermTemp);
  }
  //need to send data to display
  // Serial.print("n1.val=");
  // Serial.print((int)maxTermTemp);
  // Serial.write(0xff);
  // Serial.write(0xff);
  // Serial.write(0xff);
} 

void goLeftInterruptMethod(){
  if(lie == false){
    lie = true;
    lieTime = millis();
    digitalWrite(goToRight, LOW);
    moveToRight = false;
    startRight = false;
    startLeft = true;
    time = millis();
    timeChangeDirection = millis();
    detachInterrupt(digitalPinToInterrupt(termogenSide));
    termogenSideAttachedInterrupt = false;
    // Serial.print("lijevo ");
    // Serial.println(turnCount);
    // turnCount++;
  }else{
    //Serial.println("okinuo desno pogrešno!");
  }
  // Serial.write(0xff);  
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void goRightInterruptMethod(){
  if(lie == false){
    lie = true;
    lieTime = millis();
    digitalWrite(goToLeft, LOW);
    moveToLeft = false;
    startLeft = false;
    startRight = true;
    time = millis();
    timeChangeDirection = millis();
    detachInterrupt(digitalPinToInterrupt(goreSide));
    goreSideAttachedInterrupt = false;
    // Serial.print("desno ");
    // Serial.println(turnCount);
    //turnCount++;
  }else{
    //Serial.println("okinuo lijevo pogrešno!");
  }
  // Serial.write(0xff);  
  // Serial.write(0xff);
  // Serial.write(0xff);
}
void goLeft(){
    startLeft = false;
    digitalWrite(goToRight, LOW);
    digitalWrite(goToLeft, HIGH);
    timeFromLastDirectionChange = millis();
    directionChangeToLeft = true;
    //attachInterrupt(digitalPinToInterrupt(goreSide), goRightInterruptMethod, FALLING);
}
void goRight(){
    startRight = false;
    digitalWrite(goToLeft, LOW);
    digitalWrite(goToRight, HIGH);
    timeFromLastDirectionChange = millis();
    directionChangeToRight = true;
    //attachInterrupt(digitalPinToInterrupt(termogenSide), goLeftInterruptMethod, FALLING);
}


void setup() { 
  
  Serial.begin(9600);  // Start serial comunication at baud=9600

  nexInit();
  // I am going to change the Serial baud to a faster rate.
  // The reason is that the slider have a glitch when we try to read it's value.
  // One way to solve it was to increase the speed of the serial port.
  // delay(500);  // This dalay is just in case the nextion display didn't start yet, to be sure it will receive the following command.
  // Serial.print("baud=115200");  // Set new baud rate of nextion to 115200, but it's temporal. Next time nextion is power on,
  //                               // it will retore to default baud of 9600.
  //                               // To take effect, make sure to reboot the arduino (reseting arduino is not enough).
  //                               // If you want to change the default baud, send the command as "bauds=115200", instead of "baud=115200".
  //                               // If you change the default baud, everytime the nextion is power ON is going to have that baud rate, and
  //                               // would not be necessery to set the baud on the setup anymore.
  // Serial.write(0xff);  // We always have to send this three lines after each command sent to nextion.
  // Serial.write(0xff);
  // Serial.write(0xff);

  // Serial.end();  // End the serial comunication of baud=9600

  // Serial.begin(115200);  // Start serial comunication at baud=115200

  Start.attachPop(StartPopCallback, &Start);
  PostavkeHome.attachPop(PostavkeHomePopCallBack, &PostavkeHome);
  PosmakLeft.attachPush(PosmakLeftPushCallBack, &PosmakLeft);
  PosmakLeft.attachPop(PosmakLeftPopCallBack, &PosmakLeft);
  PosmakRight.attachPush(PosmakRightPushCallBack, &PosmakRight);
  PosmakRight.attachPop(PosmakRightPopCallBack, &PosmakRight);
  PostavkeDry.attachPop(PostavkeDryPopCallBack, &PostavkeDry);
  EndDry.attachPop(EndDryPopCallBack, &EndDry);
  DecrementTempOfSeedHome.attachPop(DecrementTempOfSeed, &DecrementTempOfSeedHome);
  IncrementTempOfSeedHome.attachPop(IncrementTempOfSeed, &IncrementTempOfSeedHome);
  DecrementTempOfTermHome.attachPop(DecrementTempOfTerm, &DecrementTempOfTermHome);
  IncrementTempOfTermHome.attachPop(IncrementTempOfTerm, &IncrementTempOfTermHome);
  DecrementTempOfSeedDry.attachPop(DecrementTempOfSeed, &DecrementTempOfSeedDry);
  IncrementTempOfSeedDry.attachPop(IncrementTempOfSeed, &IncrementTempOfSeedDry);
  DecrementTempOfTermDry.attachPop(DecrementTempOfTerm, &DecrementTempOfTermDry);
  IncrementTempOfTermDry.attachPop(IncrementTempOfTerm, &IncrementTempOfTermDry);
  BackFromSettingsHome.attachPop(BackToHome, &BackFromSettingsHome);
  BackFromSettingsDry.attachPop(BackToDry, &BackFromSettingsDry);
  //dbSerialPrintln("setup done");
  pinMode(termogenSide, INPUT);
  pinMode(goreSide, INPUT);
  pinMode(goToLeft, OUTPUT);
  pinMode(goToRight, OUTPUT);


  pinMode(termogenVentSwitch, OUTPUT);
  pinMode(plamenikSwitch, OUTPUT);
  pinMode(mjesalicaSwitch, OUTPUT);
  // pinMode(goLeftInterruptMethod, INPUT);
  // pinMode(goRightInterruptMethod, INPUT);
  attachInterrupt(digitalPinToInterrupt(termogenSide), goLeftInterruptMethod, FALLING);
  attachInterrupt(digitalPinToInterrupt(goreSide), goRightInterruptMethod, FALLING);
  
  digitalWrite(goToLeft, LOW);
  digitalWrite(goToRight, LOW);
  digitalWrite(termogenVentSwitch, LOW);
  digitalWrite(plamenikSwitch, LOW);
  digitalWrite(mjesalicaSwitch, LOW);

  Wire.setClock(10000);
  mlx.begin();
  timeForOtherStuff = millis();
  timeIntervalNextion = millis();
}

void loop() {


  receivedChar = Serial.read();
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

  //else if(receivedChar == 'l'){
  //   digitalWrite(mjesalicaSwitch, HIGH);
  //   startMixer = true;
  //   timeMixer = millis();
  //   if(1000 <= millis() - timeMixer && startMixer == true && hitLeft == false){
  //     digitalWrite(goToLeft, HIGH);
  //     timeForMove = millis();
  //   }else if(500 < millis() - timeForMove){
  //     digitalWrite(goToLeft, LOW);
  //     digitalWrite(mjesalicaSwitch,LOW);
  //     startMixer = false;
  //   }
  // }else if(receivedChar == 'r'){
  //   digitalWrite(mjesalicaSwitch, HIGH);
  //   startMixer = true;
  //   timeMixer = millis();
  //   if(1000 <= millis() - timeMixer && startMixer == true && hitRight == false){
  //     digitalWrite(goToRight, HIGH);
  //     timeForMove = millis();
  //   }else if(500 < millis() - timeForMove){
  //     digitalWrite(goToRight, LOW);
  //     digitalWrite(mjesalicaSwitch,LOW);
  //     startMixer = false;
  //   }
  // }


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
          if(digitalRead(goreSide) == HIGH){
            goLeft();
          }else{
            goRight();
          }
          if(startMixer == true && startLeft == true && timeInterval < millis() - time){
              goLeft();
            
          }else if(startMixer == true && startRight == true && timeInterval < millis() - time){
              goRight();
            
          } 
          timeDrying = millis();
          doneBooting = true;
        }
      }
    }
    
  }else if(startDrying == true && stopDrying == false && doneBooting == true){
    if(timeForOtherStuffInterval / 5 <= millis() - timeForOtherStuff){
      //Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempC()); 
      //Serial.print("*C\tObject = "); Serial.print(mlx.readObjectTempC()); Serial.println("*C");
      //Serial.print("C = "); 
      //Serial.println(thermocouple.readCelsius());

    tempOfThermogen = thermocouple.readCelsius();
    //Serial.print(thermocouple.readCelsius());

    tempOfSeed = mlx.readObjectTempC();
    tempOutside = mlx.readAmbientTempC();
    // Serial.println("term");
    // Serial.println(tempOfThermogen);
    // Serial.println((int)tempOfThermogen);
    // Serial.println("Seed");
    // Serial.println(tempOfSeed);
    // Serial.println((int)tempOfSeed);


    // check if returns are valid, if they are NaN (not a number) then something went wrong!
    // if (isnan(tempOfSeed) || isnan(tempOfThermogen)) 
    // {
    //   //Serial.println("Failed to read from DHT");
    //   tempOfSeed = 50;
    // } 

    //CurrentPage == 1
    
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
    } else if(kosticePostigleTemp == false && (tempOfSeed < 30 || tempOfThermogen < 55)){
      digitalWrite(plamenikSwitch, HIGH);
      //Serial.println("grije");
    }
    //logic to find out when to shutdown dryer
    if(numberOfShutdowns >= 5){
      if(timeOfBurnerShutdown[numberOfShutdowns - 1] - timeOfBurnerShutdown[numberOfShutdowns - 5] < 3600000){
        startDrying = false;
        stopDrying = true;
        doneBooting = false;
      }
     
    }
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
    digitalWrite(plamenikSwitch, LOW);
    tempOfThermogen = thermocouple.readCelsius();
    tempOfSeed = mlx.readObjectTempC();
    tempOutside = mlx.readAmbientTempC();
    // Serial.println("termogen: ");
    // Serial.println(tempOfThermogen);
    // Serial.println("ambient temp: ");
    //Serial.println(mlx.readAmbientTempC() +5);
    if(tempOfSeed < (tempOutside + 8) || tempOfSeed < (tempOfThermogen + 8)){
      if(startLeft == true  &&  stopMoving == false){
        digitalWrite(goToRight, LOW);
        timeMixer = millis();
        stopMoving = true;
        
      }else if(5000 <= millis() - timeMixer && stopMoving == true){
          startMixer = false;
          digitalWrite(mjesalicaSwitch, LOW);
          digitalWrite(termogenVentSwitch, LOW);
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
        }
        if(startMixer == true && startRight == true && timeInterval < millis() - time){
          goLeft();
        }
    }
  }
  if(moveToLeft == true && 500 <= millis() - timeForStartMovingMixer){
    digitalWrite(goToLeft, HIGH);
  }
  
  if(moveToRight == true && 500 <= millis() - timeForStartMovingMixer){
    digitalWrite(goToRight, HIGH);
  }

  if(directionChangeToLeft == true && 500 <= millis() - timeFromLastDirectionChange){
    attachInterrupt(digitalPinToInterrupt(goreSide), goRightInterruptMethod, FALLING);
    goreSideAttachedInterrupt = true;
    directionChangeToLeft = false;
  }

  if(directionChangeToRight == true && 500 <= millis() - timeFromLastDirectionChange){
    attachInterrupt(digitalPinToInterrupt(termogenSide), goLeftInterruptMethod, FALLING);
    termogenSideAttachedInterrupt = true;
    directionChangeToRight = false;
  }
  if(lie == true && timeInterval + 750 < millis() - lieTime){
    lie = false;
  }

  if(termogenSideAttachedInterrupt == true){
        detachInterrupt(digitalPinToInterrupt(termogenSideAttachedInterrupt));
  }else if(goreSideAttachedInterrupt == true){
        detachInterrupt(digitalPinToInterrupt(goreSide));
  }

  if(CurrentPage == 1 && lie == false && 500 < millis() - timeIntervalNextion)
    {	
      

      calculateTime = timeDrying/1000/60;
      TermNext.setValue(tempOfThermogen);
      SeedNext.setValue(tempOfSeed);
      TimeNext.setValue(calculateTime);
      // Serial.print("termogen:");
      // Serial.println(tempOfThermogen);
      // Serial.print("Kostice:");
      // Serial.println(tempOfSeed);

      // if(stageNumber == 0){
      //   Serial.print("n0.val=");  
      //   stageNumber = 1;
      // }
      // if(stageNumber == 1){
      //   Serial.print((int)tempOfThermogen);
      //   stageNumber = 2;
      // }
      // if(stageNumber == 2){
      //   Serial.write(0xff);
      //   stageNumber = 3;
      // }
      // if(stageNumber == 3){
      //   Serial.write(0xff);
      //   stageNumber = 4;
      // }
      // if(stageNumber == 4){
      //   Serial.write(0xff);
      //   stageNumber = 5;
      // }
      // if(stageNumber == 5){
      //   Serial.print("n1.val=");  
      //   stageNumber = 6;
      // }
      // if(stageNumber == 6){
      //   Serial.print((int)tempOfSeed);
      //   stageNumber = 7;
      // }
      // if(stageNumber == 7){
      //   Serial.write(0xff);
      //   stageNumber = 8;
      // }
      // if(stageNumber == 8){
      //   Serial.write(0xff);
      //   stageNumber = 9;
      // }
      // if(stageNumber == 9){
      //   Serial.write(0xff);
      //   stageNumber = 10;
      // }
      // if(stageNumber == 10){
      //   Serial.print("n2.val=");  
      //   stageNumber = 11;
      // }
      // if(stageNumber == 11){
      //   calculateTime = timeDrying/1000/60;
      //   Serial.print(calculateTime);
      //   stageNumber = 12; 
      // }
      // if(stageNumber == 12){
      //   Serial.write(0xff);
      //   stageNumber = 13;
      // }
      // if(stageNumber == 13){
      //   Serial.write(0xff);
      //   stageNumber = 14;
      // }
      // if(stageNumber == 14){
      //   Serial.write(0xff);
      //   stageNumber = 0;
      // }
      
      timeIntervalNextion = millis();
      
    }

  // Serial.println(digitalRead(goLeftInterruptMethod));
  // Serial.println(digitalRead(goRightInterruptMethod));

  nexLoop(nex_listen_list);

  if(termogenSideAttachedInterrupt == true){
    attachInterrupt(digitalPinToInterrupt(termogenSide), goLeftInterruptMethod, FALLING);
  }else if(goreSideAttachedInterrupt == true){
    attachInterrupt(digitalPinToInterrupt(goreSide), goRightInterruptMethod, FALLING);
  }


// // reset serial to enable communication
//   if(!Serial){
//       Serial.end();
//       resetSerial = true;
//       timeForSerialReset = millis();
//     }
//     if(resetSerial == true && timeForSerialResetInterval <= millis() - timeForSerialReset){
//       Serial.begin(9600);
//     }
}



