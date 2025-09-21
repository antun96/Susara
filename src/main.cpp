// #include <Arduino.h>
#include <Wire.h>
#include <max6675.h>
#include <EasyNextionLibrary.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

#define RS485Transmit HIGH
#define RS485Receive LOW
SoftwareSerial SSerial(12, 13); // RX, TX
EasyNex myNex(Serial);

// Pin init

// input pins for end switches
/// @brief end switch on right side of dryier
int rightEndSwitch = 2;

/// @brief end switch on left side of dryier
int leftEndSwitch = 3;

int MIN_SEED_TEMPERATURE = 30;

// RelaySwitches
int fanContactorPin = 4;
int burnerRelayPin = 5;
int mixerContactorPin = 6;
int rightContactorPin = 7;
int leftContactorPin = 8;

// BurnerTermometer
int thermoSCK = 9;
int thermoSO = 10;
int thermoCS = 11;

// SoftwareSerial Communication
int SRX = 12;
int STX = 13;
#define SSerialTxControl A0 // RS485 Direction control

int mixerImpulsePin = A5;

float maxSeedTemp;
float maxTermTemp;
float minTermTemp;
MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);

int CurrentPage = 0;
int currentPageAddress = 0;
int setMaxTermTempAddress = 1;
int setMaxSeedTempAddress = 2;
int mixerDelayTimeAddress = 3;
int setMinTermTempAddress = 4;

unsigned long time;
unsigned long timeForOtherStuff;
unsigned long timeForSerialReset;

bool mixerImpulseLastState = false;
unsigned long lastMixerImpulseTime;
bool mixerError = false;

unsigned long timeForMove;

unsigned long timeForStartMovingMixer;

bool moveButtonsPressed = false;
bool moveToRight = false;
bool moveToLeft = false;

unsigned long timeOfBurnerShutdown[20];
int numberOfShutdowns = 0;

// start procedures
unsigned long timeForStart;

unsigned long timeBurner;
unsigned long timeMixer;
unsigned long timeForShutdown;
unsigned long timeChangeDirection;
unsigned long timeForShutdownMixer;
unsigned long timeIntervalNextion;
unsigned long timeToLog;

unsigned long timeOfLastTurnOnSequence = 0;

// heatModes
typedef enum
{
  AUTO,
  COOL
} HeatMode;

// mixModes
typedef enum
{
  MIX,
  STOP
} MixMode;

typedef enum
{
  LEFT,
  RIGHT,
  NONE
} MovingDirection;

typedef enum
{
  NONE = -1,
  FAN = 0,
  BURNER = 1,
  MIXER = 2,
  MIXER_DIRECTION = 3,
  DONE = 4,
} BootSequence;

BootSequence bootSequence = BootSequence::NONE;
MovingDirection mixerMovingDirection = MovingDirection::NONE;
bool mixerMove = false;
unsigned long lastMixerChangeTime = 0;
MixMode mixMode;
HeatMode heatMode;

bool stopMoving = false;

float mixerDelayTime = 0;

unsigned long timeForOtherStuffInterval = 5000;
// time gap to wait for change direction
unsigned long timeInterval = 2000;
unsigned long timeForSerialResetInterval = 100;

/// @brief minimum time mixer must be off before it can change direction
unsigned long MINIMUM_MIXER_SWITCH_DELAY = 5000;
/// @brief maximum time without impulse on mixer pin, if exceeded, error is triggered
unsigned long MAXIMUM_TIME_WITHOUT_IMPULSE = 1000;
/// @brief time gap between turning on sequences
unsigned long TIME_GAP_BETWEEN_TURNING_ON_SEQUENCES = 15000;

bool startLeft = false;
bool startRight = false;

bool startDrying = false;
bool doneBooting = false;
bool stopDrying = false;

bool resetSerial = false;
String order;

bool fanState = false;
bool startMixer = false;
bool termogenOverheated = false;
double tempOfThermogen;
double tempOfSeed = 0;
double tempOutside;
bool kosticePostigleTemp = false;
bool burnerState = false;
bool doNotTurnOnPlamenikEverAgain = false;
unsigned long timeDrying;
unsigned long timeBurnerOn;
unsigned long timeTurnOnBurner;
unsigned long timeTurnOffBurner;
unsigned long timeOfShutdownStart;

bool shutDownTemperatureReached = false;

bool readSeedTemperature = false;
bool sendLogMessage = false;

int calculateTime = 0;
int calculateHours = 0;
int calculateMinutes = 0;

bool lie = false;
unsigned long lieTime;
int turnCount = 0;

char receivedChar;
int stageNumber = 0;
char send[6];

const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data

bool newData = false;

float dataNumber = 0;

void MixerDirectionChanger()
{
  if (digitalRead(rightEndSwitch) == LOW)
  {
    mixerMovingDirection = MovingDirection::LEFT;
    goLeft();
    return;
  }
  else if (digitalRead(leftEndSwitch) == LOW)
  {
    mixerMovingDirection = MovingDirection::RIGHT;
    goRight();
    return;
  }

  mixerMovingDirection = MovingDirection::RIGHT;
  goRight();
}

void MixerTurnCommand(bool command)
{
  // in case of some nasty fuckup, turn everything off
  if (mixerError)
  {
    digitalWrite(mixerContactorPin, LOW);
    startMixer = false;
    digitalWrite(leftContactorPin, LOW);
    digitalWrite(rightContactorPin, LOW);
    return;
  }

  if (command == true)
  {
    digitalWrite(mixerContactorPin, HIGH);
    lastMixerImpulseTime = millis();
    startMixer = true;
  }
  else if (command == false)
  {
    digitalWrite(mixerContactorPin, LOW);
    startMixer = false;
  }
}

/// @brief check if mixer is working properly, if there is a change of state on impulse pin, mixer is working properly
void MixerCheckState()
{
  if (startMixer == true)
  {
    if (digitalRead(mixerImpulsePin) == HIGH && mixerImpulseLastState == false)
    {
      mixerImpulseLastState = true;
      lastMixerImpulseTime = millis();
    }
    else if (digitalRead(mixerImpulsePin) == LOW && mixerImpulseLastState == true)
    {
      mixerImpulseLastState = false;
      lastMixerImpulseTime = millis();
    }
    if (MAXIMUM_TIME_WITHOUT_IMPULSE < millis() - lastMixerImpulseTime)
    {
      MixerTurnCommand(false);
      mixerError = true;
    }
  }
}

/// @brief update sreen parameters
/// @param force - forces screen update
void UpdateDryingPageParameters(bool force)
{
  if (CurrentPage != 1)
    return;
  else if (force == false && 2500 > millis() - timeIntervalNextion)
    return;

  calculateTime = (millis() - timeDrying) / 1000 / 60;
  calculateHours = calculateTime / 60;
  calculateMinutes = calculateTime - calculateHours * 60;
  myNex.writeNum("n0.val", (int)tempOfThermogen);
  myNex.writeNum("n1.val", (int)tempOfSeed);
  myNex.writeStr("t6.txt", String(calculateHours) + ":" + String(calculateMinutes));
  if (burnerState == true)
  {
    myNex.writeNum("b2.bco", 61440);
  }
  else if (burnerState == false)
  {
    myNex.writeNum("b2.bco", 2300);
  }
  if (heatMode == AUTO)
  {
    myNex.writeStr("b2.txt", "A");
  }
  else if (heatMode == COOL)
  {
    myNex.writeStr("b2.txt", "C");
  }

  if (mixerError)
  {
    myNex.writeStr("b3.txt", "E");
    myNex.writeNum("b3.bco", 61530);
  }
  else if (mixMode == MixMode::MIX)
  {
    myNex.writeStr("b3.txt", "A");
    myNex.writeNum("b3.bco", 1024);
  }
  else if (mixMode == MixMode::STOP)
  {
    myNex.writeStr("b3.txt", "S");
    myNex.writeNum("b3.bco", 61440);
  }

  timeIntervalNextion = millis();
}

void UpdateSettingsParameters()
{
  myNex.writeNum("n0.val", (int)maxSeedTemp);
  myNex.writeNum("n1.val", (int)maxTermTemp);
  myNex.writeNum("n2.val", (int)minTermTemp);
  mixerDelayTimeUpdate();
}

void recvWithStartEndMarkers()
{
  static bool recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (SSerial.available() > 0 && newData == false)
  {
    rc = SSerial.read();

    if (recvInProgress == true)
    {
      if (rc != endMarker)
      {
        receivedChars[ndx] = rc;
        ndx++;

        if (ndx >= numChars)
        {
          ndx = numChars - 1;
        }
      }
      else
      {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker)
    {
      recvInProgress = true;
    }
  }
}

void convertStringToFloat()
{
  if (newData == true)
  {
    tempOfSeed = 0;                   // new for this version
    tempOfSeed = atof(receivedChars); // new for this version
    newData = false;
  }
}

void mixerDelayTimeUpdate()
{
  String data = String(mixerDelayTime, 1);
  myNex.writeStr("t5.txt", data);
}

void SendSSMessage(String messageBody)
{
  digitalWrite(SSerialTxControl, RS485Transmit);
  String message;
  message.concat(String("<"));
  message.concat(messageBody);
  message.concat(String(">"));
  SSerial.print(message);
  SSerial.flush();
  digitalWrite(SSerialTxControl, RS485Receive);
}

/// @brief Start button is pressed, drying begins
void trigger1()
{
  stopDrying = false;
  startDrying = true;
  doneBooting = false;
  CurrentPage = 1;
  // TODO: test this if microcontroller is too fast
  UpdateDryingPageParameters(true);
}

/// @brief Stop button is pressed, drying stops
void trigger8()
{
  CurrentPage = 0;
  startDrying = false;
  doneBooting = false;
  stopDrying = true;
}

/// @brief Settings from home screen
void trigger2()
{
  CurrentPage = 2;
  UpdateSettingsParameters();
}

/// @brief Settings from drying screen
void trigger7()
{
  CurrentPage = 3;
  UpdateSettingsParameters();
}

/// @brief Screen button is pressed, mixer is turned on and moving to left
void trigger3()
{
  if (digitalRead(leftEndSwitch) != LOW && moveButtonsPressed == false)
  {
    MixerTurnCommand(true);
    timeForStartMovingMixer = millis();
    moveToLeft = true;
    moveButtonsPressed = true;
  }
}

/// @brief Screen button moved to unpressed state, stop moving left and stop mixer
void trigger4()
{
  moveToLeft = false;
  digitalWrite(leftContactorPin, LOW);
  MixerTurnCommand(false);
  moveButtonsPressed = false;
}

/// @brief  Screen button is pressed, mixer is turned on and moving to right
void trigger5()
{
  if (digitalRead(rightEndSwitch) != LOW && moveButtonsPressed == false)
  {
    MixerTurnCommand(true);
    timeForStartMovingMixer = millis();
    moveToRight = true;
    moveButtonsPressed = true;
  }
}

/// @brief Screen button moved to unpressed state, stop moving right and stop mixer
void trigger6()
{
  moveToRight = false;
  digitalWrite(rightContactorPin, LOW);
  MixerTurnCommand(false);
  moveButtonsPressed = false;
}

/// @brief back from settings to drying
void trigger14()
{
  CurrentPage = 1;
  UpdateDryingPageParameters(true);
}

/// @brief back from settings to home
void trigger9()
{
  CurrentPage = 0;
}

/// @brief Decrement maximum seed temperature
void trigger10()
{
  maxSeedTemp = maxSeedTemp - 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if (CurrentPage == 3 || CurrentPage == 2)
    UpdateSettingsParameters();
}

/// @brief Increment maximum seed temperature
void trigger11()
{
  maxSeedTemp = maxSeedTemp + 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if (CurrentPage == 3 || CurrentPage == 2)
    UpdateSettingsParameters();
}

/// @brief Decrement maximum termogen temperature
void trigger12()
{
  maxTermTemp = maxTermTemp - 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if (CurrentPage == 3 || CurrentPage == 2)
    UpdateSettingsParameters();
}

/// @brief Increment maximum termogen temperature
void trigger13()
{
  maxTermTemp = maxTermTemp + 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if (CurrentPage == 3 || CurrentPage == 2)
    UpdateSettingsParameters();
}

/// @brief Turn off burner
void TurnDownPlamenik()
{
  digitalWrite(burnerRelayPin, LOW);
  timeBurnerOn += (millis() - timeTurnOnBurner);
  burnerState = false;
  if (CurrentPage == 1)
    myNex.writeNum("b2.bco", 2300);
}

void TurnOnPlamenik()
{
  timeTurnOnBurner = millis();
  digitalWrite(burnerRelayPin, HIGH);
  burnerState = true;
  if (CurrentPage == 1)
  {
    if (heatMode == HeatMode::AUTO)
    {
      myNex.writeStr("b2.txt", "A");
    }
    myNex.writeNum("b2.bco", 61440);
  }
}

/// @brief Change heating mode from AUTO to COOL and vice versa
void trigger15()
{
  if (heatMode == HeatMode::AUTO)
  {
    heatMode = HeatMode::COOL;
    myNex.writeStr("b2.txt", "C");
    doNotTurnOnPlamenikEverAgain = true;
    TurnDownPlamenik();
  }
  else if (heatMode == HeatMode::COOL)
  {
    heatMode = HeatMode::AUTO;
    doNotTurnOnPlamenikEverAgain = false;
    if (startDrying == true && doneBooting == true && tempOfSeed < maxSeedTemp)
      TurnOnPlamenik();

    myNex.writeStr("b2.txt", "A");
  }
}

/// @brief Decrement mixer delay time by half minute and save to EEPROM
void trigger16()
{
  if (mixerDelayTime != 0)
  {
    mixerDelayTime -= 0.5;
    EEPROM.update(mixerDelayTimeAddress, (int)mixerDelayTime * 10 / 5);
  }
  mixerDelayTimeUpdate();
}

/// @brief Increment mixer delay time by half minute and save to EEPROM
void trigger17()
{
  mixerDelayTime += 0.5;
  EEPROM.update(mixerDelayTimeAddress, (int)mixerDelayTime * 10 / 5);
  mixerDelayTimeUpdate();
}

/// @brief Decrement minimum termogen temperature and save to EEPROM
void trigger18()
{
  minTermTemp -= 1;
  EEPROM.update(setMinTermTempAddress, minTermTemp);
  myNex.writeNum("n2.val", (int)minTermTemp);
}

/// @brief Increment minimum termogen temperature and save to EEPROM
void trigger19()
{
  minTermTemp += 1;
  EEPROM.update(setMinTermTempAddress, minTermTemp);
  myNex.writeNum("n2.val", (int)minTermTemp);
}

/// @brief Check if left end switch is pressed for safety, then move mixer to left
void goLeft()
{
  if (digitalRead(leftEndSwitch) == LOW)
    return;
  
  digitalWrite(rightContactorPin, LOW);
  digitalWrite(leftContactorPin, HIGH);
}

/// @brief Check if right end switch is pressed for safety, then move mixer to right
void goRight()
{
  if (digitalRead(rightEndSwitch) == LOW)
    return;

  digitalWrite(leftContactorPin, LOW);
  digitalWrite(rightContactorPin, HIGH);
}

/// @brief Start/Stop mixer or reset error
void trigger20()
{
  if (mixerError)
  {
    mixerError = false;
    myNex.writeStr("b3.txt", "S");
    myNex.writeNum("b3.bco", 61440);
    mixMode = MixMode::STOP;
    MixerTurnCommand(false);
    digitalWrite(leftContactorPin, LOW);
    digitalWrite(rightContactorPin, LOW);
    return;
  }

  // stop mixer
  if (mixMode == MixMode::MIX)
  {
    myNex.writeStr("b3.txt", "S");
    myNex.writeNum("b3.bco", 61440);
    mixMode = MixMode::STOP;
    MixerTurnCommand(false);
    digitalWrite(leftContactorPin, LOW);
    digitalWrite(rightContactorPin, LOW);
  }
  else if (mixMode == MixMode::STOP)
  {
    myNex.writeStr("b3.txt", "A");
    myNex.writeNum("b3.bco", 1024);
    mixMode = MixMode::MIX;
    // if (digitalRead(rightEndSwitch) != LOW)
    //   goRight();
    // else if (digitalRead(leftEndSwitch) != LOW)
    //   goLeft();
    // else
    //   goLeft();
  }
}

void BootTurnOn(BootSequence sequence)
{
  switch (sequence)
  {
  case BootSequence::FAN:
    digitalWrite(fanContactorPin, HIGH);
    fanState = true;
    timeOfLastTurnOnSequence = millis();
    bootSequence = BootSequence::BURNER;
    break;
  case BootSequence::BURNER:
    digitalWrite(burnerRelayPin, HIGH);
    burnerState = true;
    timeOfLastTurnOnSequence = millis();
    bootSequence = BootSequence::MIXER;
    break;
  case BootSequence::MIXER:
    MixerTurnCommand(true);
    timeOfLastTurnOnSequence = millis();
    bootSequence = BootSequence::MIXER_DIRECTION;
    break;
  case BootSequence::MIXER_DIRECTION:
    MixerDirectionChanger();
    timeOfLastTurnOnSequence = millis();
    bootSequence = BootSequence::DONE;
  case BootSequence::DONE:
    doneBooting = true;
    break;
  default:
    break;
  }
}

void BootUpProcedure()
{
  if (TIME_GAP_BETWEEN_TURNING_ON_SEQUENCES > millis() - timeOfLastTurnOnSequence)
    return;

  BootTurnOn(bootSequence);
}

void DryingProcess()
{
  if (timeForOtherStuffInterval <= millis() - timeForOtherStuff)
  {
    SendSSMessage("seed");
    recvWithStartEndMarkers();
    convertStringToFloat();
    tempOfThermogen = thermocouple.readCelsius();

    if (tempOfSeed > maxSeedTemp && kosticePostigleTemp == false)
    {
      kosticePostigleTemp = true;
      // timeOfBurnerShutdown[numberOfShutdowns] = millis();
      // numberOfShutdowns++;
    }
    else if (tempOfSeed < 35)
    {
      kosticePostigleTemp = false;
    }

    if (tempOfThermogen > maxTermTemp || kosticePostigleTemp == true)
    {
      TurnDownPlamenik();
      termogenOverheated = true;
      // Serial.println("hladi");
    }
    else if (kosticePostigleTemp == true && tempOfSeed <= MIN_SEED_TEMPERATURE && tempOfThermogen < maxTermTemp)
    {
      TurnOnPlamenik();
      kosticePostigleTemp = false;

      // Serial.println("grije");
      //  } else if(kosticePostigleTemp == false && (tempOfSeed < 30 || tempOfThermogen < maxTermTemp - 15)){
    }
    else if (kosticePostigleTemp == false && termogenOverheated == true && tempOfThermogen < minTermTemp)
    {

      TurnOnPlamenik();
      termogenOverheated = false;
    }
    timeForOtherStuff = millis();
  }

  if (mixerDelayTime != 0 && mixerDelayTime * 60 * 1000 < millis() - time && mixMode == MixMode::MIX)
  {
    if (startMixer == false)
    {
      MixerTurnCommand(true);
      timeMixer = millis();
    }

    if (startMixer == true && startLeft == true && 5000 < millis() - timeMixer)
    {
      goLeft();
    }
    else if (startMixer == true && startRight == true && 5000 < millis() - timeMixer)
    {
      goRight();
    }
  }
  else if (mixerDelayTime == 0 && MINIMUM_MIXER_SWITCH_DELAY < millis() - time && mixMode == MixMode::MIX)
  {

    if (startMixer == false)
    {
      MixerTurnCommand(true);
      timeMixer = millis();
    }

    if (startMixer == true && startLeft == true && 5000 < millis() - timeMixer)
    {
      goLeft();
    }
    else if (startMixer == true && startRight == true && 5000 < millis() - timeMixer)
    {
      goRight();
    }
  }
}

void StopDrying()
{
  digitalWrite(burnerRelayPin, LOW);
  CurrentPage = 0;
  MixerTurnCommand(false);
  digitalWrite(leftContactorPin, LOW);
  digitalWrite(rightContactorPin, LOW);
  digitalWrite(fanContactorPin, LOW);
  fanState = false;
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

/// @brief Check if mixer can move and moves it
void MoveMixer()
{
  // turn on moveToLeftContactor when button is pressed
  if (moveToLeft == true)
  {
    // shutdown moving left when mixer hits left end switch
    if (digitalRead(leftEndSwitch) == LOW)
      digitalWrite(leftContactorPin, LOW);
    else if (500 <= (millis() - timeForStartMovingMixer) && digitalRead(leftEndSwitch) != LOW)
      digitalWrite(leftContactorPin, HIGH);
  }

  // turn on moveToRightContactor when button is pressed
  else if (moveToRight == true)
  {
    // shutdown moving right when mixer hits right end switch
    if (digitalRead(rightEndSwitch) == LOW && moveToRight == true)
      digitalWrite(rightContactorPin, LOW);
    else if (500 <= (millis() - timeForStartMovingMixer) && digitalRead(rightEndSwitch) != LOW)
      digitalWrite(rightContactorPin, HIGH);
  }
}

void MixerEndSwitchCheck()
{
  if (digitalRead(leftEndSwitch) == LOW)
  {
    digitalWrite(leftContactorPin, LOW);
    moveToLeft = false;
    startLeft = false;
    time = millis();
    timeChangeDirection = millis();
    if (moveButtonsPressed == false && mixerError == false)
    {
      startRight = true;
      if (mixerDelayTime > 0)
        MixerTurnCommand(false);
    }
  }
  if (digitalRead(rightEndSwitch) == LOW)
  {
    digitalWrite(rightContactorPin, LOW);
    moveToRight = false;
    startRight = false;
    time = millis();
    timeChangeDirection = millis();
    if (moveButtonsPressed == false && mixerError == false)
    {
      startLeft = true;
      if (mixerDelayTime > 0)
        MixerTurnCommand(false);
    }
  }
}

void setup()
{
  // Serial.begin(9600);  // Start serial comunication at baud=9600

  myNex.begin(9600);
  SSerial.begin(4800);

  pinMode(leftEndSwitch, INPUT);
  pinMode(rightEndSwitch, INPUT);
  pinMode(leftContactorPin, OUTPUT);
  pinMode(rightContactorPin, OUTPUT);
  pinMode(SSerialTxControl, OUTPUT);

  pinMode(fanContactorPin, OUTPUT);
  pinMode(burnerRelayPin, OUTPUT);
  pinMode(mixerContactorPin, OUTPUT);
  pinMode(mixerImpulsePin, INPUT);

  digitalWrite(leftContactorPin, LOW);
  digitalWrite(rightContactorPin, LOW);
  digitalWrite(fanContactorPin, LOW);
  digitalWrite(burnerRelayPin, LOW);
  digitalWrite(mixerContactorPin, LOW);
  digitalWrite(SSerialTxControl, RS485Receive);

  timeForOtherStuff = millis();
  timeIntervalNextion = millis();
  time = millis();
  heatMode = HeatMode::AUTO;
  mixMode = MixMode::MIX;

  maxSeedTemp = EEPROM.read(setMaxSeedTempAddress);
  maxTermTemp = EEPROM.read(setMaxTermTempAddress);
  mixerDelayTime = EEPROM.read(mixerDelayTimeAddress) * 0.5;
  minTermTemp = EEPROM.read(setMinTermTempAddress);

  wdt_enable(WDTO_250MS);
}

void loop()
{
  // after start command is received, booting process begins
  if (startDrying == true && stopDrying == false && doneBooting == false)
    BootUpProcedure();

  // procedure after boot is completed
  else if (startDrying == true && stopDrying == false && doneBooting == true)
    DryingProcess();

  // shutdown procedure
  else if (startDrying == false && stopDrying == true && doneBooting == false)
    StopDrying();

  if (moveButtonsPressed == true && mixerError == false)
    MoveMixer();

  if (CurrentPage == 1)
    UpdateDryingPageParameters(false);

  MixerEndSwitchCheck();
  MixerCheckState();
  myNex.NextionListen();
  wdt_reset();
}