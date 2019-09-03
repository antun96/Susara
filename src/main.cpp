//#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <max6675.h>
#include <LiquidCrystal_I2C.h>

unsigned long time;
unsigned long timeForOtherStuff;
unsigned long timeForSerialReset;

unsigned long timeForMove;

unsigned long timeOfBurnerShutdown[20];
int numberOfShutdowns = 0;

//start procedures
unsigned long timeContactorsSwitch;
unsigned long timeFanStarActive;
unsigned long timeForStart;

unsigned long timeBurner;
unsigned long timeMixer;
unsigned long timeForShutdown;
unsigned long timeChangeDirection;

int fanVentStart = 0;
bool fanStarConnectionBool = false;
bool finishedStarConnectionBool = false;
bool fanStarStartedSpinning = false;
bool fanDeltaConnectionBool = false;
bool finishedDeltaConnectionBool = false;
bool fanDeltaStartedSpinning = false;

bool firstTimeForStart = false;
bool firstTimeBurner = false;
bool firstTimeMixer = false;

int buttonUp = PIN_A2;
int buttonDown = PIN_A3;
int buttonBack = PIN_A6;
int buttonProceed = PIN_A7;


unsigned long timeForOtherStuffInterval = 5000;
unsigned long timeInterval = 2000;
unsigned long timeForSerialResetInterval = 100;

int thermoDO = 10; //8
int thermoCS = 11; //7
int thermoCLK = 9; //6

//endSensor
int termogenSide = 2;
int goreSide = 3;

int fanStarConnection = 4;
int fanDeltaConnection = 5;

int termogenVentSwitch = 6; //9
int plamenikSwitch = 7; //10

int mjesalicaSwitch = 8; //11
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
bool kosticePostigleTemp = false;

char receivedChar;
bool newData = false;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

int mainMenuItemCount = 3;
String mainMenu[3] = {"Start",
"Postavke",
"Pomicanje mjesalice"};

String dryingPrompt = "Da li zelite pokrenuti proces susenja?";

String stopDryingPrompt = "Da li zelite prekinuti proces susenja?";

int dryingProcessMenuItemCount = 3;
String dryingProcessMenu[3] = {"Temp koštica:",
  "Temp term:",
  "Vrijeme susenja:"
};
int settingsMenuItemCount = 7;
String settings [7] = {"Max temp kostica:",
"Max temp termogena:",
"Minimalna temperatra kostica:",
"Minimalna temp plamenika za paljenje:"
"Vrijeme ukljucenja",
"Vrijeme stajanja mjesalice:",
"Broj gasenja plamenika za zavrsetak procesa susenja:"};
String currentPosition = "";
int menuRow = 0;
int menuPick = 0;
int lcdRow = 0;
bool buttonUpBool = false;
bool buttonDownBool = false;
bool buttonBackBool = false;
bool buttonProceedBool = false;
unsigned long lcdScreenRefreshInterval;
unsigned long pressedButtonTime;


void readPressedButton(){
  if(500 < millis() - pressedButtonTime){
    if(analogRead(buttonUp) > 800){
      buttonUpBool = true;
      pressedButtonTime = millis();
      lcd.clear();
      Serial.println("up");
    }else if(analogRead(buttonDown) > 800){
      buttonDownBool = true;
      pressedButtonTime = millis();
      lcd.clear();
      Serial.println("down");
    }else if(analogRead(buttonBack) > 800){
      buttonBackBool = true;
      pressedButtonTime = millis();
      lcd.clear();
      Serial.println("back");
    }else if(analogRead(buttonProceed) > 800){
      buttonProceedBool = true;
      pressedButtonTime = millis();
      lcd.clear();
      Serial.println("proceed");
    }
  }

}


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
  lcd.init(); 
  pinMode(termogenSide, INPUT);
  pinMode(goreSide, INPUT);
  pinMode(goToLeft, OUTPUT);
  pinMode(goToRight, OUTPUT);

  pinMode(fanStarConnection, OUTPUT);
  pinMode(fanDeltaConnection, OUTPUT);
  pinMode(termogenVentSwitch, OUTPUT);
  pinMode(plamenikSwitch, OUTPUT);
  pinMode(mjesalicaSwitch, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(termogenSide), goLeft, FALLING);
  attachInterrupt(digitalPinToInterrupt(goreSide), goRight, FALLING);

  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  pinMode(buttonBack, INPUT);
  pinMode(buttonProceed, INPUT);
  lcd.init();
  lcd.backlight();
  currentPosition = "main";
  pressedButtonTime = millis();
  lcdScreenRefreshInterval = millis();
  
  digitalWrite(goToLeft, LOW);
  digitalWrite(goToRight, LOW);
  digitalWrite(fanDeltaConnection, LOW);
  digitalWrite(fanStarConnection, LOW);
  digitalWrite(termogenVentSwitch, LOW);
  digitalWrite(plamenikSwitch, LOW);
  digitalWrite(mjesalicaSwitch, LOW);

  Serial.begin(9600);
  Serial.println("Adafruit MLX90614 test");  
  Serial.println(timeForOtherStuffInterval);
  mlx.begin();
  timeForOtherStuff = millis();
}

void loop() {


    if(currentPosition == "main"){
    readPressedButton();
    if(buttonUpBool == true && menuPick > 0){
      menuRow = menuRow - 1;
      menuPick = menuPick - 1;
      buttonUpBool = false;
    }else{
      buttonUpBool = false;
    }
    if(buttonDownBool == true && menuRow < 1){
      //menuRow = menuRow + 1;
      menuPick = menuPick + 1;
      buttonDownBool = false;
    }else if(buttonDownBool == true && menuRow == 1){
      menuPick = menuPick + 1;
      menuRow = menuRow + 1;
      buttonDownBool = false;
    }else{
      buttonDownBool = false;
    }
    if(buttonProceedBool == true){
      Serial.println("ne valja");
      if(menuPick == 0){
        currentPosition = "startPrompt";
      }else if( menuPick == 1){
        currentPosition = "settings";
      }else if (menuPick == 2){
        currentPosition = "mixerMove";
      }
      buttonProceedBool = false;
    }
    if(menuPick <= 1){
      lcd.setCursor(1, menuPick % 2);
      lcd.print(">");
      lcd.setCursor(2,lcdRow);
      lcd.print(mainMenu[menuRow]);
      lcd.setCursor(2,lcdRow + 1);
      lcd.print(mainMenu[menuRow + 1]);
    }else if(menuPick == 2){
      lcd.setCursor(1, 2);
      lcd.print(">");
      lcd.setCursor(2,lcdRow);
      lcd.print(mainMenu[menuRow]);
      lcd.setCursor(2,lcdRow + 1);
      lcd.print(mainMenu[menuRow + 1]);
    }

  }
  if(currentPosition == "startPrompt"){
    readPressedButton();
    if(buttonUpBool == true && menuRow >= 2){
      menuRow = menuRow - 1;
      menuPick = menuPick - 1;
      buttonUpBool = false;
    }else{
      buttonUpBool = false;
    }
    if(buttonDownBool == true && menuRow < 2){
      menuRow = menuRow + 1;
      menuPick = menuPick + 1;
      buttonDownBool = false;
    }else if(buttonDownBool == true && menuRow == 2){
      menuPick = menuPick + 1;
      buttonDownBool = false;
    }else{
      buttonDownBool = false;
    }
    if(buttonProceedBool == true){
      if(menuPick == 1){
        currentPosition = "startPrompt";
      }else if( menuPick == 2){
        currentPosition = "settings";
      }else if (menuPick == 3){
        currentPosition = "mixerMove";
      }
      buttonProceedBool = false;
    }
    lcd.setCursor(1, menuPick % 4);
    lcd.print(">");
    lcd.setCursor(2,lcdRow);
    lcd.print(mainMenu[menuRow]);
    lcd.setCursor(2,lcdRow + 1);
    lcd.print(mainMenu[menuRow + 1]);
    lcd.setCursor(3,lcdRow);
    lcd.print(mainMenu[menuRow]);
    lcd.setCursor(3,lcdRow + 1);
    lcd.print(mainMenu[menuRow + 1]);

  }

  
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
    //if(fanStarConnectionBool == false && fanDeltaConnectionBool == false && finishedStarConnectionBool == false){        //connects in star
    if(fanVentStart == 0){  
      digitalWrite(fanDeltaConnection, LOW);
      digitalWrite(fanStarConnection, HIGH);
      fanStarConnectionBool = true;
      finishedStarConnectionBool = true;
      timeContactorsSwitch = millis();
      fanVentStart = 1;
    }//else if(500 <= millis() - timeContactorsSwitch && fanStarConnectionBool == true && finishedStarConnectionBool == true && fanStarStartedSpinning == false){
    else if(500 <= millis() - timeContactorsSwitch && fanVentStart == 1){
      
      digitalWrite(termogenVentSwitch, HIGH);       //starts in star connection squirrel fan and gives power to termogen
      fanStarStartedSpinning = true;
      timeFanStarActive = millis();
      fanVentStart = 2;
    }//else if(5000 <= millis() - timeFanStarActive && fanStarStartedSpinning == true){
    else if(10000 <= millis() - timeFanStarActive && fanVentStart == 2){
      digitalWrite(termogenVentSwitch, HIGH);        //disconnects all contactors
      digitalWrite(fanStarConnection, LOW);
      digitalWrite(fanDeltaConnection, LOW);
      if(fanStarConnectionBool == true){
        fanStarConnectionBool = false;
        timeContactorsSwitch = millis();
        fanVentStart = 3;
      }
    }//else if(500 <= millis() -  timeContactorsSwitch && fanDeltaConnectionBool == false && fanStarConnectionBool == false){   //switches to delta connection
    else if(500 <= millis() -  timeContactorsSwitch && fanVentStart == 3){   //switches to delta connection
        digitalWrite(fanDeltaConnection, HIGH);
        fanDeltaConnectionBool = true;
        timeContactorsSwitch = millis();

        fanStarStartedSpinning = false;
        startVentTermogen = true;
        if(firstTimeForStart == false){
          timeForStart = millis();
          firstTimeForStart = true;
          fanVentStart = -1;
        }
    }
    if(5000 <= millis() - timeForStart && startVentTermogen == true && fanDeltaConnectionBool == true){
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

    tempOfSeed = mlx.readAmbientTempC();
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
      if(timeOfBurnerShutdown[numberOfShutdowns - 1] - timeOfBurnerShutdown[numberOfShutdowns - 5] < 3600000){
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



