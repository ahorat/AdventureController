#include <Arduino.h>
#include <Bounce2.h>
#include <bluefruit.h>

#define DEBUG

#define BUTTON_PRESSED LOW
#define BUTTON_RELEASED HIGH
#define BOUNCE_INTERVAL 5

#define NUM_BUTTONS 4
#define NUMBER_OF_PROFILES 2
#define ID_OF_CONSUMER_PROFILE 1

#define PROFILE_SWITCH_BUTTON_1 0
#define PROFILE_SWITCH_BUTTON_2 3
#define PROFILE_SWITCH_INTERVAL 5000

#define MULTIPRESS_INTERVAL 500

BLEDis bledis;
BLEHidAdafruit blehid;

Bounce bounces[NUM_BUTTONS];
uint32_t nextKeyStrikeTimeMs[NUM_BUTTONS];

const int BUTTON_PINS[NUM_BUTTONS] = {PIN_031, PIN_029, PIN_002, PIN_115};

const uint16_t KEY_CODES_PROFILE_1[NUM_BUTTONS] = {'=', '-', 'r', 'c'};
const uint16_t KEY_CODES_PROFILE_2[NUM_BUTTONS] = {HID_USAGE_CONSUMER_SCAN_NEXT, HID_USAGE_CONSUMER_SCAN_PREVIOUS,
                                                   HID_USAGE_CONSUMER_VOLUME_DECREMENT, HID_USAGE_CONSUMER_VOLUME_INCREMENT};
const uint16_t *KEY_CODES[NUMBER_OF_PROFILES] = {KEY_CODES_PROFILE_1, KEY_CODES_PROFILE_2};

uint32_t currentProfile;
bool profileChanged;

void fireKey(uint32_t buttonId);

void setup()
{
  currentProfile = 0;
  profileChanged = false;

  // Init Input Pins
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    bounces[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
    bounces[i].interval(BOUNCE_INTERVAL);
    nextKeyStrikeTimeMs[i] = 0;
  }

#ifdef DEBUG
  Serial.begin(115200);
  while (!Serial)
    delay(10);
#endif

  Bluefruit.begin();
  Bluefruit.autoConnLed(false);
  Bluefruit.setName("AdvCtrl V0.1 P1");
  Bluefruit.setTxPower(4);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather 52");
  bledis.begin();

  /* Start BLE HID */
  blehid.begin();

  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);

  // Include BLE HID service
  Bluefruit.Advertising.addService(blehid);

  // There is enough room for the dev name in the advertising packet
  Bluefruit.Advertising.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
  Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

void loop()
{
  delay(5);
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    // Update the Bounces instance
    bounces[i].update();
  }
  uint32_t currentDuration = 0;
  // --------------- Profile Change Logic ---------------------
  if (!profileChanged && bounces[PROFILE_SWITCH_BUTTON_1].read() == BUTTON_PRESSED && bounces[PROFILE_SWITCH_BUTTON_2].read() == BUTTON_PRESSED)
  {
    if (bounces[PROFILE_SWITCH_BUTTON_1].currentDuration() > PROFILE_SWITCH_INTERVAL &&
        bounces[PROFILE_SWITCH_BUTTON_2].currentDuration() > PROFILE_SWITCH_INTERVAL)
    {
      currentProfile = (currentProfile + 1) % NUMBER_OF_PROFILES;
      profileChanged = true;
#ifdef DEBUG
      Serial.println("Switched to Profile: " + (String)currentProfile);
#endif
    }
    return;
  }
  else if (profileChanged && (bounces[PROFILE_SWITCH_BUTTON_1].read() == BUTTON_RELEASED && bounces[PROFILE_SWITCH_BUTTON_2].read() == BUTTON_RELEASED))
  {
    profileChanged = false;
  }

  // --------------- Single Press Logic ---------------------
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    if (bounces[i].changed())
    {
      // If value changed, get new state and fire key if button is released.
      if (bounces[i].read() == BUTTON_RELEASED)
      {
        fireKey(i);
        nextKeyStrikeTimeMs[i] = 0;
      }
    }

    // --------------- Long Press Logic ---------------------
    else if (!profileChanged && bounces[i].read() == BUTTON_PRESSED)
    {
      // If button is pressed for longer time, repeated firing of keys
      currentDuration = bounces[i].currentDuration();
      if (currentDuration > 1000 && currentDuration > nextKeyStrikeTimeMs[i])
      {
        fireKey(i);
        nextKeyStrikeTimeMs[i] = currentDuration + MULTIPRESS_INTERVAL;
      }
    }
  }
  // Serial.println(bounces[0].read());
}

void fireKey(uint32_t button_id)
{
#ifdef DEBUG
  Serial.println(KEY_CODES[currentProfile][button_id]);
#endif
  if (currentProfile == ID_OF_CONSUMER_PROFILE)
  {
    blehid.consumerKeyPress(KEY_CODES[currentProfile][button_id]);
    blehid.consumerKeyRelease();
  }
  else
  {
    blehid.keyPress(KEY_CODES[currentProfile][button_id]);
    blehid.keyRelease();
  }
}
