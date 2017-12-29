/**
 * Firefighter Flashlight Arduino script
 *
 * Copyright (C) 2016 by Michael Ikemann
 * 
 *  Hardware setup:
 *  - White bright 30 mA leds at pins 2,3,4,7 and 8
 *  - Blue leds at pins 5 and 6
 *  - Piezo at pin 9
 *  - Analogue button input chain at pin 4, resistor chain using 1k/2k/3k for button 0, 1, 2 and 10k ground pulldown : +5V -> 1k -> button 0 -> 1k -> button 1 -> 1k button 2 -> (analogue 4 and (10k -> GND)
 *  
 *  Flash light switches between different modes when mode button is pressed, increases and decreases brightness or flashing frequency using + and - buttons and plays Fireman sam theme or an alarm if + or - are pressed and blue light mode
 */


// -------------------------------------------------------------------------------------

#include "pitches.h"  // for melodies

// -------------------------------------------------------------------------------------

#define FLASH_LIGHT_PIN_COUNT 5   ///< Total count of white LEDs
#define FIREFIGHTER_PIN_COUNT 2   ///< Total count of blue LEDs
#define PIEZO_PIN 9

const int gFlashLightPins[FLASH_LIGHT_PIN_COUNT] = {2,3,4,7,8};   ///< White LED pins
const int gFireLightPins[FIREFIGHTER_PIN_COUNT] = {5,6};          ///< Blue LED pins

#define BUTTON_TOLLERANCE 10  ///< Button tollerance, relative to 1024
#define BUTTON_INPUT 4        ///< Analogue pin 4
#define BUTTON_DELAY 25       ///< Debouncing delay

#define BLINK_SPEED 250               ///< Blue light blink speed
#define LIGHT_INTENSITY_STEPPING 10   ///< Stepping size of light intensity changing (0 = min, 100 = max)

// -------------------------------------------------------------------------------------

/** @brief Enumeration of flashlight modes */
enum class LightMode
{
  All,              ///< All white leds enabled
  JustOne,          ///< Just center led
  Inner,            ///< Just inner leds
  Walking,          ///< Knight righter mode (left to right and back)
  Switching,        ///< Switching between inner and outer
  Flashing,         ///< Flashing
  Emergency,        ///< Blue leds static
  EmergencyFlashing,  ///< Blue leds flashing
  Count             ///< Mode count
};

// -------------------------------------------------------------------------------------

/** @brief Enumeration of hardware buttons */
enum class FlashLightButton
{
  LightMode,  ///< Light mode button
  Plus,       ///< Plus button
  Minus,      ///< Minus button
  Count,      ///< Total button count
  None = -1   ///< No button / undefined state
};

// -------------------------------------------------------------------------------------

/** @brief Flashlight logic handling class */
class AFlashLight
{
protected:
  LightMode         mLightMode = LightMode::All;    ///< The current flash light mode
  int               miLightIntensity = 100;              ///< The current light intensity / flash speed
  const int         maiButtonAnalogValues[(int)FlashLightButton::Count] = {787, 852, 930};  ///< The analog values which are received if any of the buttons ia pressed
  FlashLightButton  mCurButton = FlashLightButton::None;  ///< The currently pressed button
  long              mlLastButtonChange = 0;               ///< The last button change tick
  long              mlLastButtonTriggered = 0;            ///< The last button trigger tick

public:
  //! Setups the flashlight
  void Setup();

  //! Updates the light and blinking states
  void UpdateLightStates();

  //! Handles the analogue input and triggers a button press when detected
  void EvaluateButtons();

  //! Handles the press of given button type
  /** @param [in] Button The button being pressed, @see FlashLightButton */
  void HandleButton(const FlashLightButton Button);

  //! Plays the Fireman Sam melody
  void PlayFiremanSam();

  //! Plays one of two alarm sounds
  /** @param [in] Style The style, either 0 or 1 atm */
  void PlayAlarm(const int Style);
};

// -------------------------------------------------------------------------------------

AFlashLight gFlashLight;   ///< Global flashlight object

// -------------------------------------------------------------------------------------

// Startup callback
void setup() {
  Serial.begin(9600);
  gFlashLight.Setup();
}

// .....................................................................................

// Main loop handler callback
void loop() 
{
  gFlashLight.EvaluateButtons();
  gFlashLight.UpdateLightStates();
}

// .....................................................................................

// Setups the flashlight
void AFlashLight::Setup()
{
  for(int i=0; i<FLASH_LIGHT_PIN_COUNT; ++i)
  {
    pinMode(gFlashLightPins[i], OUTPUT);
  }

  for(int i=0; i<FIREFIGHTER_PIN_COUNT; ++i)
  {
    pinMode(gFireLightPins[i], OUTPUT);
  }  

  noTone(PIEZO_PIN); // stopt das Spielen des Tons
}

// .....................................................................................

// Updates the light and blinking states
void AFlashLight::UpdateLightStates()
{  
  int lightInt;

  if(mLightMode==LightMode::Flashing) // Use intensity for frequency in flashing mode
  {
    int interval = miLightIntensity*5;
    lightInt = millis()%(interval*2)/interval ? HIGH : LOW;
  }
  else
  {
    lightInt = micros()%10000>=(100-miLightIntensity)*100;
  }
  
  int off = millis()%1600/200;
  if(off>4)
    off = 4-(off-4);

  if(mLightMode==LightMode::Walking)
  {
    switch(off)
    {
      case 0: off = 3; break;
      case 1: off = 2; break;
      case 2: off = 1; break;
      case 3: off = 0; break;
      case 4: off = 4; break;
    }
  }

  // white center led
  digitalWrite(gFlashLightPins[0], mLightMode==LightMode::Flashing || mLightMode==LightMode::Inner || (mLightMode==LightMode::Walking && off==0) || (mLightMode==LightMode::Switching && millis()%(BLINK_SPEED*2)/BLINK_SPEED==0) ||mLightMode==LightMode::All ? lightInt : LOW);
  // left inner
  digitalWrite(gFlashLightPins[1], mLightMode==LightMode::Flashing || mLightMode<=LightMode::Inner || (mLightMode==LightMode::Walking && off==1) ||(mLightMode==LightMode::Switching && millis()%(BLINK_SPEED*2)/BLINK_SPEED==0) ||mLightMode==LightMode::All ? lightInt : LOW);
  // right inner
  digitalWrite(gFlashLightPins[2], mLightMode==LightMode::Flashing || mLightMode==LightMode::Inner || (mLightMode==LightMode::Walking && off==2) ||(mLightMode==LightMode::Switching && millis()%(BLINK_SPEED*2)/BLINK_SPEED==0) || mLightMode==LightMode::All ? lightInt : LOW);
  // left outer
  digitalWrite(gFlashLightPins[3], mLightMode==LightMode::Flashing || mLightMode==LightMode::All || (mLightMode==LightMode::Walking && off==3) ||(mLightMode==LightMode::Switching && millis()%(BLINK_SPEED*2)/BLINK_SPEED==1) ? lightInt : LOW);
  // right outer
  digitalWrite(gFlashLightPins[4], mLightMode==LightMode::Flashing || mLightMode==LightMode::All || (mLightMode==LightMode::Walking && off==4) ||(mLightMode==LightMode::Switching && millis()%(BLINK_SPEED*2)/BLINK_SPEED==1) ? lightInt : LOW);

  // blue leds
  digitalWrite(gFireLightPins[0], mLightMode==LightMode::Emergency || (mLightMode==LightMode::EmergencyFlashing && millis()%(BLINK_SPEED*2)/BLINK_SPEED==1)  ? HIGH : LOW);
  digitalWrite(gFireLightPins[1], mLightMode==LightMode::Emergency || (mLightMode==LightMode::EmergencyFlashing && millis()%(BLINK_SPEED*2)/BLINK_SPEED==0) ? HIGH : LOW);
}

// .....................................................................................

// Handles the analogue input and triggers a button press when detected
void AFlashLight::EvaluateButtons()
{
  int buttonState = analogRead(BUTTON_INPUT);
  FlashLightButton newButton = FlashLightButton::None;

  if(buttonState>BUTTON_TOLLERANCE)
  {
    for(int i=0; i<(int)FlashLightButton::Count; ++i)
    {
      if(abs(buttonState-maiButtonAnalogValues[i])<BUTTON_TOLLERANCE)
      {
        newButton = (FlashLightButton)i;
      }
    }
  }

  if(mCurButton!=newButton)
  {
    mCurButton = newButton;
    mlLastButtonChange = millis();
    mlLastButtonTriggered = 0;
  }

   if(mCurButton!=FlashLightButton::None && mlLastButtonTriggered==0 && millis()-mlLastButtonChange>BUTTON_DELAY)
   {
    HandleButton(mCurButton);
   }
}

// .....................................................................................

// Handles the press of given button type
void AFlashLight::HandleButton(const FlashLightButton Button)
{
  mlLastButtonTriggered = millis();
  Serial.println((int)mCurButton);

  if(Button==FlashLightButton::LightMode)
  {
    mLightMode = (LightMode)(((int)mLightMode+1)%(int)LightMode::Count);
    miLightIntensity = 100;
  }

  if((mLightMode==LightMode::Emergency || mLightMode==LightMode::EmergencyFlashing) && (Button==FlashLightButton::Plus || Button==FlashLightButton::Minus))
  {
    if(Button==FlashLightButton::Plus)
    {
    PlayFiremanSam();  
    }
    else
    {
      PlayAlarm(mLightMode==LightMode::Emergency ? 1 : 0);
    }
  }
  else
  {
    if(Button==FlashLightButton::Plus)
    {
      miLightIntensity += LIGHT_INTENSITY_STEPPING;
      if(miLightIntensity>100)
      {
        miLightIntensity = 100;
      }
    }
    if(mCurButton==FlashLightButton::Minus)
    {
      miLightIntensity -= LIGHT_INTENSITY_STEPPING;
      if(miLightIntensity<LIGHT_INTENSITY_STEPPING)
        miLightIntensity = LIGHT_INTENSITY_STEPPING;
    }
  }
}

// .....................................................................................

// Plays the Fireman Sam melody
void AFlashLight::PlayFiremanSam()
{
  // notes in the melody:
  int melody[] = {
    NOTE_F3, NOTE_C4, NOTE_F3, NOTE_C4, NOTE_F3, 
    NOTE_C4, NOTE_F3, NOTE_A3, NOTE_C4, NOTE_D4,
    NOTE_AS3, NOTE_A3, NOTE_C4, NOTE_D4,
    NOTE_A4, NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4,
    NOTE_D4, NOTE_G4, NOTE_E4,
    NOTE_A4, NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4,
    NOTE_D4, NOTE_G4, NOTE_F4, NOTE_E4, NOTE_D4,
    NOTE_E4, NOTE_F4  
  };

  // note durations: 4 = quarter note, 8 = eighth note, etc.:
  int noteDurations[] = {
    2, 2, 2, 2, 2, 
    2, 1, 2, 2, 2, 
    2, 2, 2, 1,
    2, 4, 4, 4, 1,
    2, 2, 1,
    2, 4, 4, 4, 1,
    2, 4, 4, 4, 4,
    2, 1
  };

  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < sizeof(melody)/sizeof(int); thisNote++) {

    // to calculate the note duration, take one second
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(PIEZO_PIN, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    for(int i=0; i<8; ++i)  // split pause to be able to update lights
    {
      delay(pauseBetweenNotes/8);  
      UpdateLightStates();    
    }
    // stop the tone playing:
    noTone(PIEZO_PIN); 
    delay(10);
  }
}

// .....................................................................................

// Plays one of two alarm sounds
void AFlashLight::PlayAlarm(const int Style)
{

  switch(Style)
  {
    case 0:
    {
      int delayTime = 400;
      int repetitions = 4;
    
      for(int i=0; i<4; ++i)
      {
        tone(PIEZO_PIN, 440*3, delayTime);

        for(int turn=0; turn<8; ++turn)
        {
          delay(delayTime/8);
          UpdateLightStates();
        }
        
        noTone(PIEZO_PIN);
        
        tone(PIEZO_PIN, 523*4, delayTime);

        for(int turn=0; turn<8; ++turn)
        {
          delay(delayTime/8);
          UpdateLightStates();
        }

        noTone(PIEZO_PIN);

        UpdateLightStates();
      }
    } break;
    
    case 1:
    {
     int speed = 10;
    
      for(int rep=0; rep<3; ++rep)
      {
        for (int x = 0; x < 100; x++)
        { 
          tone(PIEZO_PIN, 300 + x*60, speed);
          for(int turn=0; turn<8; ++turn)
          {
            delay(speed/8);
            UpdateLightStates();
          }        
          noTone(PIEZO_PIN);
        }   
        for (int x = 99; x>=0; --x)
        { 
          tone(PIEZO_PIN, 300 + x*60, speed);
          for(int turn=0; turn<8; ++turn)
          {
            delay(speed/8);
            UpdateLightStates();
          }        
          noTone(PIEZO_PIN);
        }
      }   
    } break;
  }
}


