#include <Arduino.h>
#include <IbusTrx.h>
#include <Keyboard.h>
#include "IbusMessages.h"

// RPI Control GPIO
// Currently not implemented as the GPIO for RPI is 3.3V
const uint8_t RPI_GPIO_PIN = 2;

// Switch 12V output to RPI
const uint8_t SW_12V_PIN = 3;

// Switch RGB input
const uint8_t SW_RGB_PIN = 4;

// Switch ON/OFF MCP2025
const uint8_t SW_WAKE_PIN = 5;

IbusTrx ibusTrx;

// Timeout when no IBUS activity
static unsigned long lastIbusActivityMs = 0;
static bool slept = false;

// Timeout set to 10 seconds of inactivity
const unsigned long IBUS_INACTIVITY_TIMEOUT_MS = 10000;

// SELECT button toggles between Arrow UP and Arrow DOWN
static bool selectSendsUp = true;

// Only send USB keystrokes when SW_RGB_PIN is LOW (RPI Analog RGB enabled)
static inline bool rpiAnalogRgbEnabled() {
  return digitalRead(SW_RGB_PIN) == LOW;
}

static void sendKey(uint8_t key, uint8_t times = 1) {
  if (!rpiAnalogRgbEnabled()) return;
  for (uint8_t i = 0; i < times; i++) {
    Keyboard.write(key);
  }
}

static void switchRpiGpio(uint8_t pinState) {
  digitalWrite(RPI_GPIO_PIN, pinState);
}

static void switchPowerRpi(uint8_t pinState) {
  digitalWrite(SW_12V_PIN, pinState);
}

static void switchAnalogRgbInput(uint8_t pinState) {
  digitalWrite(SW_RGB_PIN, pinState);
}

static void switchLinIC(uint8_t pinState) {
  digitalWrite(SW_WAKE_PIN, pinState);
}

static void SleepMode() {
  // Stop UART so TXD is LOW
  Serial1.end();

  // Force TXD low (pin 1 on Arduino Pro Micro)
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);

  delay(50); // Ensure TXD is stable low

  // Sleep mode on MCP2025 with WAKE pin LOW
  switchLinIC(LOW);

  delay(50);
}

void setup() {
  // RPI GPIO set to LOW always to prevent 5V into GPIO of RPI
  pinMode(RPI_GPIO_PIN, OUTPUT);
  switchRpiGpio(LOW);

  // 12V to RPI is OFF wehn ardunio setup (wait for IGN to start)
  pinMode(SW_12V_PIN, OUTPUT);
  switchPowerRpi(LOW);

  // Analog RGB input from NAV module on startup
  pinMode(SW_RGB_PIN, OUTPUT);
  switchAnalogRgbInput(HIGH);

  // Make sure the MCP2025 is in normal mode
  pinMode(SW_WAKE_PIN, OUTPUT);
  switchLinIC(HIGH);

  delay(50);

  // IBUS is 9600 8E1 on BMW
  Serial1.begin(9600, SERIAL_8E1);
  ibusTrx.begin(Serial1);
  lastIbusActivityMs = millis();
  slept = false;

  // USB HID init    
  Keyboard.begin();
  delay(500);

}

void loop() {
  // Read complete IBUS messages via IbusTrx
  if (ibusTrx.available()) {
    IbusMessage msg = ibusTrx.readMessage();

    lastIbusActivityMs = millis();

    // IGN handling
    if (ibusIsIgnPos2(msg)) {
      switchPowerRpi(HIGH);  // Enable 12V only when IGN is POS2
    }
    if (ibusIsIgnOff(msg)) {
      switchPowerRpi(LOW); // Turn of 12v to RPI when IGN is OFF
    }

    if (ibusIsTelephonePressed(msg)) {
      switchAnalogRgbInput(digitalRead(SW_RGB_PIN) == HIGH ? LOW : HIGH);
    }

    // Force back to NAV RGB input (disable RPI analog RGB) when the buttons below are pressed
    if (ibusIsAmPressed(msg) || ibusIsFmPressed(msg) ||
              ibusIsTonePressed(msg) || ibusIsMenuPressed(msg) ||
              ibusIsModePressed(msg) || ibusIsInfoPressed(msg) ||
              ibusIsRadioBmPressed(msg)) {
      switchAnalogRgbInput(HIGH);
    }

    // Keyboard strokes for each radio buttons presses
    if (ibusIsPreset1Pressed(msg)) {
      sendKey('h');
    } else if (ibusIsPreset2Pressed(msg)) {
      sendKey(KEY_BACKSPACE);
    } else if (ibusIsPreset3Pressed(msg)) {
      sendKey('p');
    } else if (ibusIsPreset4Pressed(msg)) {
      sendKey('s');
    } else if (ibusIsPreset5Pressed(msg)) {
      sendKey(KEY_UP_ARROW);
    } else if (ibusIsPreset6Pressed(msg)) {
      sendKey(KEY_DOWN_ARROW);

    } else if (ibusIsMflSendEndPressed(msg)) {
      sendKey('v');
    } else if (ibusIsMflModeTogglePressed(msg)) {
      sendKey('a');
    }

    if (ibusIsSearchUpPressed(msg)) {
      sendKey('n');
    } else if (ibusIsSearchDownPressed(msg)) {
      sendKey('b');
    } else if (ibusIsSearchLeftPressed(msg)) {
      sendKey('b');
    } else if (ibusIsSearchRightPressed(msg)) {
      sendKey('n');
    } else if (ibusIsSelectPressed(msg)) {
      sendKey(selectSendsUp ? KEY_UP_ARROW : KEY_DOWN_ARROW);
      selectSendsUp = !selectSendsUp;
    } else if (ibusIsBmKnobPressed(msg)) {
      sendKey(KEY_RETURN);
    } else if (ibusIsBmKnobRight(msg, 1)) {
      sendKey(KEY_RIGHT_ARROW, 1);
    } else if (ibusIsBmKnobRight(msg, 2)) {
      sendKey(KEY_RIGHT_ARROW, 2);
    } else if (ibusIsBmKnobRight(msg, 3)) {
      sendKey(KEY_RIGHT_ARROW, 3);
    } else if (ibusIsBmKnobRight(msg, 4)) {
      sendKey(KEY_RIGHT_ARROW, 4);
    } else if (ibusIsBmKnobLeft(msg, 1)) {
      sendKey(KEY_LEFT_ARROW, 1);
    } else if (ibusIsBmKnobLeft(msg, 2)) {
      sendKey(KEY_LEFT_ARROW, 2);
    } else if (ibusIsBmKnobLeft(msg, 3)) {
      sendKey(KEY_LEFT_ARROW, 3);
    } else if (ibusIsBmKnobLeft(msg, 4)) {
      sendKey(KEY_LEFT_ARROW, 4);
    }
  }
  // Sleep mode for the MCP2025 when no IBUS activity detected for 10 seconds (Car is off and closed)
  if (!slept && (millis() - lastIbusActivityMs > IBUS_INACTIVITY_TIMEOUT_MS)) {
    SleepMode();
    slept = true;   // prevent calling SleepMode() repeatedly
  }

}