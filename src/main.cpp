// #include <Arduino.h>
#include <Wire.h>
#include <max6675.h>
#include <EasyNextionLibrary.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

#define RS485Transmit HIGH
#define RS485Receive LOW
// SoftwareSerial Communication
int SRX = 12;
int STX = 13;
#define SSerialTxControl A0       // RS485 Direction control
SoftwareSerial SSerial(SRX, STX); // RX, TX
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

int mixerImpulsePin = A5;

float maxSeedTemp;
float maxTermTemp;
float minTermTemp;
MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);

// EEPROM addresses
int setMinTermTempAddress = 1;
int setMaxTermTempAddress = 2;
int setMaxSeedTempAddress = 3;
int mixerDelayTimeAddress = 4;
int totalWorkingTimeAddress = 5;
int totalBurnerTimeAddress = 8;

unsigned long timeMixerHitEndSwitch;
unsigned long lastReadingUpdatingTime;

bool mixerImpulseLastState = false;
unsigned long lastMixerImpulseTime;
bool mixerError = false;

bool moveButtonsPressed = false;

unsigned long timeBurner;
unsigned long timeMixerTurnOn;
unsigned long timeIntervalNextion;

/// @brief Used in boot up procedure to create time gap between turning on sequences
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
  HIT_LEFT,
  RIGHT,
  HIT_RIGHT,
  NONE
} MovingDirection;

typedef enum
{
  FAN,
  BURNER,
  MIXER,
  MIXER_MOVE,
  DONE,
} BootSequence;

typedef enum
{
  BOOTING,
  DRYING,
  SHUTTING_DOWN,
  WAINTING_FOR_START,
} OperationMode;

typedef enum
{
  START_SCREEN,
  DRYING_SCREEN,
  SETTINGS_SCREEN,
  SETTINGS_SCREEN_FROM_DRYING,
  STAT_SCREEN
} NextionScreen;

BootSequence bootSequence = BootSequence::FAN;
MovingDirection mixerMovingDirection = MovingDirection::NONE;
MixMode mixMode;
HeatMode heatMode;
OperationMode operationMode = OperationMode::WAINTING_FOR_START;
NextionScreen currentPage = NextionScreen::START_SCREEN;
bool updateScreenOnChange = false;
unsigned long lastScreenChangeTime = 0;

float mixerDelayTime = 0;

unsigned long READING_UPDATING_INTERVAL = 5000;

/// @brief delay for screen parameter update when screen is changed
unsigned long SCREEN_CHANGE_DELAY_MILLIS = 250;

/// @brief time between mixer turn on and mixer movement
unsigned long SWITCH_STATE_TIME_INTERVAL = 2000;
/// @brief minimum time mixer must be off before it can change direction
unsigned long MINIMUM_MIXER_SWITCH_DELAY = 5000;
/// @brief maximum time without impulse on mixer pin, if exceeded, error is triggered
unsigned long MAXIMUM_TIME_WITHOUT_IMPULSE = 2000;

/// @brief time interval for updating working hours in RAM
unsigned long TIME_INTERVAL_WORKING_HOURS_RAM = 10000; // 10 seconds
/// @brief time interval for updating working hours in EEPROM, described in seconds
unsigned long TIME_INTERVAL_WORKING_HOURS_EEPROM = 600; // 10 minutes
/// @brief time gap between turning on sequences
unsigned long TIME_GAP_BETWEEN_TURNING_ON_SEQUENCES = 8000;

bool fanState = false;
bool mixerState = false;
bool burnerState = false;
bool termogenOverheated = false;
double thermogenTemperature;
double seedTemperature = 0;
bool reachedMaxSeedTemp = false;

unsigned long periodicStoreTimeTask;

unsigned long timeBurnerOnTact;
unsigned long timeDryerOnTact;
unsigned long sessionTimeDryingSeconds;
unsigned long sessionTimeBurnerOnSeconds;
unsigned long totalTimeDryingSeconds;
unsigned long totalTimeBurnerOnSeconds;
unsigned long totalTimeDryingLastSave;
unsigned long totalTimeBurnerLastSave;

bool shutDownTemperatureReached = false;

bool readSeedTemperature = false;
bool sendLogMessage = false;

char receivedChar;
int stageNumber = 0;
char send[6];

const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data

bool newData = false;

float dataNumber = 0;

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

/// @brief Check if mixer can move and moves it
void MoveMixer()
{
  // turn on moveToLeftContactor when button is pressed
  if (mixerMovingDirection == MovingDirection::LEFT && SWITCH_STATE_TIME_INTERVAL <= (millis() - timeMixerTurnOn))
  {
    goLeft();
    return;
  }
  // turn on moveToRightContactor when button is pressed
  else if (mixerMovingDirection == MovingDirection::RIGHT && SWITCH_STATE_TIME_INTERVAL <= (millis() - timeMixerTurnOn))
  {
    goRight();
    return;
  }
}

/// @brief /// Turn mixer on or off, checks for error
/// @param command
void MixerTurnCommand(bool command)
{
  // in case of some nasty fuckup, turn everything off
  if (mixerError)
  {
    digitalWrite(mixerContactorPin, LOW);
    mixerState = false;
    digitalWrite(leftContactorPin, LOW);
    digitalWrite(rightContactorPin, LOW);
    return;
  }

  if (command == true)
  {
    digitalWrite(mixerContactorPin, HIGH);
    mixerState = true;
    timeMixerTurnOn = millis();
    lastMixerImpulseTime = millis();
  }
  else if (command == false)
  {
    digitalWrite(mixerContactorPin, LOW);
    mixerState = false;
  }
}

/// @brief Control mixer operation based on mode and timing
void MixerControl()
{
  if (mixMode == MixMode::STOP || mixerError)
    return;

  // logic to use default time if there is zero delay time
  unsigned long delay = 0;
  if (mixerDelayTime == 0)
    delay = MINIMUM_MIXER_SWITCH_DELAY;
  else
    delay = mixerDelayTime * 60 * 1000;

  if (delay < millis() - timeMixerHitEndSwitch)
  {
    if (mixerState == false)
      MixerTurnCommand(true);

    if (mixerMovingDirection == MovingDirection::HIT_RIGHT)
      mixerMovingDirection = MovingDirection::LEFT;
    else if (mixerMovingDirection == MovingDirection::HIT_LEFT)
      mixerMovingDirection = MovingDirection::RIGHT;
    else if (mixerMovingDirection == MovingDirection::NONE)
      mixerMovingDirection = MovingDirection::RIGHT;

    return;
  }
}

/// @brief check if mixer is working properly, if there is a change of state on impulse pin, mixer is working properly
/// if there is no change of state for too long, mixer error is triggered
void CheckMixerState()
{
  if (mixerState != true)
    return;
  
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

/// @brief update sreen parameters
/// @param force - forces screen update
void UpdateDryingPageParameters(bool force)
{
  if (currentPage != NextionScreen::DRYING_SCREEN)
    return;
  else if (force == false && 2500 > millis() - timeIntervalNextion)
    return;

  unsigned long calculateSessionDryingTime = sessionTimeDryingSeconds / 60;
  int sessionDryingHours = calculateSessionDryingTime / 60;
  int sessionDryingMinutes = calculateSessionDryingTime - sessionDryingHours * 60;

  unsigned long calculateBurnerTime = sessionTimeBurnerOnSeconds / 60;
  int sessionBurnerHours = calculateBurnerTime / 60;
  int sessionBurnerMinutes = calculateBurnerTime - sessionBurnerHours * 60;

  myNex.writeNum("n0.val", (int)thermogenTemperature);
  myNex.writeNum("n1.val", (int)seedTemperature);
  myNex.writeStr("t6.txt", String(sessionDryingHours) + ":" + String(sessionDryingMinutes));
  myNex.writeStr("t8.txt", String(sessionBurnerHours) + ":" + String(sessionBurnerMinutes));
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

void mixerDelayTimeUpdate()
{
  String data = String(mixerDelayTime, 1);
  myNex.writeStr("t5.txt", data);
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
    seedTemperature = 0;                   // new for this version
    seedTemperature = atof(receivedChars); // new for this version
    newData = false;
  }
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
  operationMode = OperationMode::BOOTING;
  currentPage = NextionScreen::DRYING_SCREEN;
  updateScreenOnChange = true;
  lastScreenChangeTime = millis();
}

/// @brief Stop button is pressed, drying stops
void trigger8()
{
  currentPage = NextionScreen::START_SCREEN;
  totalTimeDryingSeconds += (millis() - timeDryerOnTact) / 1000;
  sessionTimeDryingSeconds += (millis() - timeDryerOnTact) / 1000;
  operationMode = OperationMode::SHUTTING_DOWN;
}

/// @brief Settings from home screen
void trigger2()
{
  currentPage = NextionScreen::SETTINGS_SCREEN;
  updateScreenOnChange = true;
  lastScreenChangeTime = millis();
}

/// @brief Settings from drying screen
void trigger7()
{
  currentPage = NextionScreen::SETTINGS_SCREEN_FROM_DRYING;
  updateScreenOnChange = true;
  lastScreenChangeTime = millis();
}

/// @brief Screen button is pressed, mixer is turned on and moving to left
void trigger3()
{
  if (digitalRead(leftEndSwitch) != LOW && moveButtonsPressed == false)
  {
    MixerTurnCommand(true);
    mixerMovingDirection = MovingDirection::LEFT;
    moveButtonsPressed = true;
  }
}

/// @brief Screen button moved to unpressed state, stop moving left and stop mixer
void trigger4()
{
  digitalWrite(leftContactorPin, LOW);
  mixerMovingDirection = MovingDirection::NONE;
  MixerTurnCommand(false);
  moveButtonsPressed = false;
}

/// @brief  Screen button is pressed, mixer is turned on and moving to right
void trigger5()
{
  if (digitalRead(rightEndSwitch) != LOW && moveButtonsPressed == false)
  {
    MixerTurnCommand(true);
    mixerMovingDirection = MovingDirection::RIGHT;
    moveButtonsPressed = true;
  }
}

/// @brief Screen button moved to unpressed state, stop moving right and stop mixer
void trigger6()
{
  digitalWrite(rightContactorPin, LOW);
  mixerMovingDirection = MovingDirection::NONE;
  MixerTurnCommand(false);
  moveButtonsPressed = false;
}

/// @brief back from settings to drying
void trigger14()
{
  currentPage = NextionScreen::DRYING_SCREEN;
  updateScreenOnChange = true;
  lastScreenChangeTime = millis();
}

/// @brief back from settings or stats to home
void trigger9()
{
  currentPage = NextionScreen::START_SCREEN;
}

/// @brief Decrement maximum seed temperature
void trigger10()
{
  maxSeedTemp = maxSeedTemp - 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if (currentPage == NextionScreen::SETTINGS_SCREEN_FROM_DRYING || currentPage == NextionScreen::SETTINGS_SCREEN)
    UpdateSettingsParameters();
}

/// @brief Increment maximum seed temperature
void trigger11()
{
  maxSeedTemp = maxSeedTemp + 1;
  EEPROM.update(setMaxSeedTempAddress, (int)maxSeedTemp);
  if (currentPage == NextionScreen::SETTINGS_SCREEN_FROM_DRYING || currentPage == NextionScreen::SETTINGS_SCREEN)
    UpdateSettingsParameters();
}

/// @brief Decrement maximum termogen temperature
void trigger12()
{
  maxTermTemp = maxTermTemp - 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if (currentPage == NextionScreen::SETTINGS_SCREEN_FROM_DRYING || currentPage == NextionScreen::SETTINGS_SCREEN)
    UpdateSettingsParameters();
}

/// @brief Increment maximum termogen temperature
void trigger13()
{
  maxTermTemp = maxTermTemp + 1;
  EEPROM.update(setMaxTermTempAddress, (int)maxTermTemp);
  if (currentPage == NextionScreen::SETTINGS_SCREEN_FROM_DRYING || currentPage == NextionScreen::SETTINGS_SCREEN)
    UpdateSettingsParameters();
}

/// @brief Turn off burner
void TurnDownBurner()
{
  digitalWrite(burnerRelayPin, LOW);
  totalTimeBurnerOnSeconds += (millis() - timeBurnerOnTact) / 1000;
  sessionTimeBurnerOnSeconds += (millis() - timeBurnerOnTact) / 1000;
  burnerState = false;
  if (currentPage == NextionScreen::DRYING_SCREEN)
    myNex.writeNum("b2.bco", 2300);
}

void TurnOnBurner()
{
  timeBurnerOnTact = millis();
  digitalWrite(burnerRelayPin, HIGH);
  burnerState = true;
  if (currentPage == NextionScreen::DRYING_SCREEN)
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
    TurnDownBurner();
  }
  else if (heatMode == HeatMode::COOL)
  {
    heatMode = HeatMode::AUTO;
    if (operationMode == OperationMode::DRYING && seedTemperature < maxSeedTemp && burnerState == false && termogenOverheated == false && thermogenTemperature < maxTermTemp)
      TurnOnBurner();

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
    mixerMovingDirection = MovingDirection::NONE;
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
    mixerMovingDirection = MovingDirection::NONE;
  }
  else if (mixMode == MixMode::STOP)
  {
    myNex.writeStr("b3.txt", "A");
    myNex.writeNum("b3.bco", 1024);
    mixMode = MixMode::MIX;
  }
}

/// @brief Go to statistics screen and calculate working hours
void trigger21()
{
  currentPage = NextionScreen::STAT_SCREEN;
  updateScreenOnChange = true;
  lastScreenChangeTime = millis();
}

void ReadSensors()
{
  SendSSMessage("seed");
  recvWithStartEndMarkers();
  convertStringToFloat();
  thermogenTemperature = thermocouple.readCelsius();
}

void UpdateStatisticsParameters()
{
  unsigned long calculateTotalDryingTime = totalTimeDryingSeconds / 60;
  int totalDryingHours = calculateTotalDryingTime / 60;
  int totalDryingMinutes = calculateTotalDryingTime - totalDryingHours * 60;

  unsigned long calculateTotalBurnerTime = totalTimeBurnerOnSeconds / 60;
  int totalBurnerHours = calculateTotalBurnerTime / 60;
  int totalBurnerMinutes = calculateTotalBurnerTime - totalBurnerHours * 60;

  myNex.writeStr("t3.txt", String(totalDryingHours) + ":" + String(totalDryingMinutes));
  myNex.writeStr("t4.txt", String(totalBurnerHours) + ":" + String(totalBurnerMinutes));
}

void PeriodicTasks()
{
  if (updateScreenOnChange == true && SCREEN_CHANGE_DELAY_MILLIS < millis() - lastScreenChangeTime)
  {
    if(currentPage == NextionScreen::DRYING_SCREEN)
      UpdateDryingPageParameters(true);
    else if(currentPage == NextionScreen::SETTINGS_SCREEN)
      UpdateSettingsParameters();
    else if(currentPage == NextionScreen::SETTINGS_SCREEN_FROM_DRYING)
      UpdateSettingsParameters();
    else if (currentPage == NextionScreen::STAT_SCREEN)
      UpdateStatisticsParameters();
    
    updateScreenOnChange = false;
  }
  if (READING_UPDATING_INTERVAL < millis() - lastReadingUpdatingTime)
  {
    ReadSensors();
    UpdateDryingPageParameters(false);

    lastReadingUpdatingTime = millis();
  }
}

void TemperatureControl()
{
  if(heatMode == HeatMode::COOL)
  {
    if (burnerState == true)
      TurnDownBurner();
    return;
  }
  if (seedTemperature > maxSeedTemp && reachedMaxSeedTemp == false)
  {
    reachedMaxSeedTemp = true;
    TurnDownBurner();
    return;
  }
  else if (seedTemperature < 35)
    reachedMaxSeedTemp = false;

  if (thermogenTemperature > maxTermTemp)
  {
    TurnDownBurner();
    termogenOverheated = true;
  }
  else if (reachedMaxSeedTemp == true && seedTemperature <= MIN_SEED_TEMPERATURE && thermogenTemperature < maxTermTemp)
  {
    TurnOnBurner();
    reachedMaxSeedTemp = false;
  }
  else if (reachedMaxSeedTemp == false && termogenOverheated == true && thermogenTemperature < minTermTemp)
  {
    TurnOnBurner();
    termogenOverheated = false;
  }
}

void BootTurnOn()
{
  switch (bootSequence)
  {
  case BootSequence::FAN:
    digitalWrite(fanContactorPin, HIGH);
    fanState = true;
    timeOfLastTurnOnSequence = millis();
    timeDryerOnTact = millis();
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
    bootSequence = BootSequence::MIXER_MOVE;
    break;
  case BootSequence::MIXER_MOVE:
    MoveMixer();
    timeOfLastTurnOnSequence = millis();
    bootSequence = BootSequence::DONE;
  case BootSequence::DONE:
    operationMode = OperationMode::DRYING;
    break;
  default:
    break;
  }
}

void BootUpProcedure()
{
  if (TIME_GAP_BETWEEN_TURNING_ON_SEQUENCES > millis() - timeOfLastTurnOnSequence && bootSequence != BootSequence::FAN)
    return;

  BootTurnOn();
}

void DryingProcess()
{
  TemperatureControl();
  MixerControl();
  MoveMixer();
}

void StopDrying()
{
  digitalWrite(burnerRelayPin, LOW);
  currentPage = NextionScreen::START_SCREEN;
  MixerTurnCommand(false);
  digitalWrite(leftContactorPin, LOW);
  digitalWrite(rightContactorPin, LOW);
  digitalWrite(fanContactorPin, LOW);
  fanState = false;
  mixerState = false;
  operationMode = OperationMode::WAINTING_FOR_START;
  shutDownTemperatureReached = false;
}

void MixerEndSwitchCheck()
{
  if (digitalRead(leftEndSwitch) == LOW && mixerMovingDirection == MovingDirection::LEFT)
  {
    digitalWrite(leftContactorPin, LOW);
    timeMixerHitEndSwitch = millis();
    mixerMovingDirection = MovingDirection::HIT_LEFT;
    if (moveButtonsPressed == false && mixerError == false)
    {
      // if mixer delay time is 0, mixer is always on, because there is no waiting time on the end
      if (mixerDelayTime > 0)
        MixerTurnCommand(false);
    }
  }

  if (digitalRead(rightEndSwitch) == LOW && mixerMovingDirection == MovingDirection::RIGHT)
  {
    digitalWrite(rightContactorPin, LOW);
    timeMixerHitEndSwitch = millis();
    mixerMovingDirection = MovingDirection::HIT_RIGHT;
    if (moveButtonsPressed == false && mixerError == false)
    {
      // if mixer delay time is 0, mixer is always on, because there is no waiting time on the end
      if (mixerDelayTime > 0)
        MixerTurnCommand(false);
    }
  }
}

void WorkingHoursCalculation()
{
  if(TIME_INTERVAL_WORKING_HOURS_RAM > millis() - periodicStoreTimeTask)
    return;
  
  if(burnerState == true)
  {
    sessionTimeBurnerOnSeconds += (millis() - timeBurnerOnTact) / 1000;
    totalTimeBurnerOnSeconds += (millis() - timeBurnerOnTact) / 1000;
    timeBurnerOnTact = millis();
  }

  if(operationMode == OperationMode::BOOTING || operationMode == OperationMode::DRYING)
  {
    sessionTimeDryingSeconds += (millis() - timeDryerOnTact) / 1000;
    totalTimeDryingSeconds += (millis() - timeDryerOnTact) / 1000;
    timeDryerOnTact = millis();
  }

  if(totalTimeBurnerOnSeconds >= totalTimeBurnerLastSave +  TIME_INTERVAL_WORKING_HOURS_EEPROM)
  {
    EEPROM.put(totalBurnerTimeAddress, totalTimeBurnerOnSeconds);
    totalTimeBurnerLastSave = totalTimeBurnerOnSeconds;
  }

  if(totalTimeDryingSeconds >= totalTimeDryingLastSave + TIME_INTERVAL_WORKING_HOURS_EEPROM)
  {
    EEPROM.put(totalWorkingTimeAddress, totalTimeDryingSeconds);
    totalTimeDryingLastSave = totalTimeDryingSeconds;
  }

  periodicStoreTimeTask = millis();
}

void ReadEEPROM()
{
  EEPROM.get(totalWorkingTimeAddress, totalTimeDryingSeconds);
  EEPROM.get(totalBurnerTimeAddress, totalTimeBurnerOnSeconds);
}

void WriteEEPROM()
{
  totalTimeDryingSeconds = 0;
  totalTimeBurnerOnSeconds = 0;
  EEPROM.put(totalWorkingTimeAddress, totalTimeDryingSeconds);
  EEPROM.put(totalBurnerTimeAddress, totalTimeBurnerOnSeconds);
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

  lastReadingUpdatingTime = millis();
  timeIntervalNextion = millis();
  timeMixerHitEndSwitch = millis();
  periodicStoreTimeTask = millis();
  heatMode = HeatMode::AUTO;
  mixMode = MixMode::MIX;

  maxSeedTemp = EEPROM.read(setMaxSeedTempAddress);
  maxTermTemp = EEPROM.read(setMaxTermTempAddress);
  mixerDelayTime = EEPROM.read(mixerDelayTimeAddress) * 0.5;
  minTermTemp = EEPROM.read(setMinTermTempAddress);

  ReadEEPROM();

  totalTimeBurnerLastSave = totalTimeBurnerOnSeconds;
  totalTimeDryingLastSave = totalTimeDryingSeconds;

  wdt_enable(WDTO_250MS);
}

void loop()
{
  // after start command is received, booting process begins
  if (operationMode == OperationMode::BOOTING)
    BootUpProcedure();

  // procedure after boot is completed
  else if (operationMode == OperationMode::DRYING)
    DryingProcess();

  // shutdown procedure
  else if (operationMode == OperationMode::SHUTTING_DOWN)
    StopDrying();

  if (moveButtonsPressed == true && mixerError == false)
    MoveMixer();

  PeriodicTasks();

  MixerEndSwitchCheck();
  CheckMixerState();
  WorkingHoursCalculation();
  myNex.NextionListen();
  wdt_reset();
}