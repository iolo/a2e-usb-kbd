#include <hidboot.h>
#include <usbhub.h>
#include <SPI.h>

// open jumper to log with Serial(PB0&PB1 will not work)
#undef _LOG_ENABLED
#ifdef _LOG_ENABLED
#define LOG_INIT(baud)    { Serial.begin(baud); } //while(!Serial); }
#define LOG(message)      (Serial.print(message))
#define LOG_HEX(hex)      (Serial.print(hex, HEX))
#define LOG_LINE(message) (Serial.println(message))
#else
#define LOG_INIT(baud)
#define LOG(message)
#define LOG_HEX(hex)
#define LOG_LINE(message)
#endif

#undef _LED_ENABLED
#ifdef _LED_ENABLED
#define LED_INIT()        (pinMode(LED_BUILTIN, OUTPUT))
#define LED_ON()          (digitalWrite(LED_BUILTIN, HIGH))
#define LED_OFF()         (digitalWrite(LED_BUILTIN, LOW))
#else
#define LED_INIT()
#define LED_ON()
#define LED_OFF()
#endif

// ### Apple //e  keyboard socket(26pin IDC)
//          ^
//          | rear(slot)
//        -----
//    X6 |26 25| Y7
// /SHFT |24 23| Y6
//    Y9 |22 21| X4
//    X3 |20 19| X5
//    X1 |18 17| X7
//    X2 |16 15| /RESET
//    X0 |14 13||GND
//    Y8 |12 11| /CTRL
//    Y5 |10  9| /CAPS
//    Y4 |8   7| PB0/OAPL/Cmd
//    Y3 |6   5| PB1/CAPL/Opt
//    Y2 |4   3| 5V
//    Y1 |2   1| Y0
//        -----
//          | front(keyboard)
//          v

// ### Arduino Nano
//                USB
//                   ---|  |---
//  SCK UHS <-- SCK |16      15| D12 MISO --> UHS MISO
//      UHS <-- 3V3 |17      14| D11 MOSI --> UHS MOSI
//              REF |18      13| D10 SS --> UHS SS
//           <-- A0 |19      12| D9 --> UHS INT
//  INH 4067 <-- A1 |20      11| D8 --> 4051 S2 (X b2)
//  SHFT A2E <-- A2 |21      10| D7 --> 4051 S1 (X b1)
// RESET A2E <-- A3 |22       9| D6 --> 4051 S0 (X b0)
//      CAPS <-- A4 |23       8| D5 --> 4067 S3 (Y b3)
//      CTRL <-- A5 |24       7| D4 --> 4067 S2 (Y b2)
//           x-- A6 |25       6| D3 --> 4067 S1 (Y b1)
//           x-- A7 |26       5| D2 --> 4067 S0 (Y b0)
//       UHS <-- 5V |27       4| GND
//      UHS <-- RST |28       3| RST
//              GND |29 x x x 2| RX0 --> JUMPER -> A2E PB0
//              VIN |30 x x x 1| TX1 --> JUMPER -> A2E PB1
//                   ----------

// ### USB Host Shield
//                ---|    |---
//   Nano <-- SS |o   VBUS o o| INT --> Nano
// Nano <-- MOSI |o    INT x x|
// Nano <-- MISO |o    GPX x x|
//  Nano <-- SCK |o    RST o x|
//               |x        x x|
//               |x          x|
//               |x          x|
//               |x          x|
// Nano <-- 3.3V |o x x  x x x| GND
//               |x x x  x x x| RST --> Nano
//           GND |x x x  x x x|
//               |x x x  x x x|
//                -------------
//
// ### 4067 16ch mux for Y (rows)
//
// ### 4051  8ch mux for X (cols)
//

#define MATRIX_ROWS 10
#define MATRIX_COLS 8

// usb hid scan code -> apple //e matrix x & y
uint8_t matrix[MATRIX_ROWS][MATRIX_COLS] = {
  // X0    X1    X2    X3    X4    X5    X6    X6
  // ESC  TAB   Aa    Zz
  { 0x29, 0x2b, 0x04, 0x1d, 0x00, 0x00, 0x00, 0x00 }, // Y0
  // 1!   Qq    Dd    Xx
  { 0x1e, 0x14, 0x07, 0x1b, 0x00, 0x00, 0x00, 0x00 }, // Y1
  // 2@   Ww    Ss    Cc
  { 0x1f, 0x1a, 0x16, 0x06, 0x00, 0x00, 0x00, 0x00 }, // Y2
  // 3#   Ee    Hh    Vv
  { 0x20, 0x08, 0x0b, 0x19, 0x00, 0x00, 0x00, 0x00 }, // Y3
  // 4$   Rr    Ff    Bb
  { 0x21, 0x15, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00 }, // Y4
  // 6^   Yy    Gg    Nn
  { 0x23, 0x1c, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00 }, // Y5
  // 5%   Tt    Jj    Mm    \|    `~    CR    BS
  { 0x22, 0x17, 0x0d, 0x10, 0x31, 0x35, 0x28, 0x2a }, // Y6
  // 7&   Uu    Kk    ,<    =+    Pp    UP    DOWN
  { 0x24, 0x18, 0x0e, 0x36, 0x2e, 0x13, 0x52, 0x51 }, // Y7
  // 8*   Ii    ;:    .>    0)    [{    SPC   LEFT
  { 0x25, 0x0c, 0x33, 0x37, 0x27, 0x2f, 0x2c, 0x50 }, // Y8
  // 9(   Oo    Ll    /?    -_    ]}    '"    RIGHT
  { 0x26, 0x12, 0x0f, 0x38, 0x2d, 0x30, 0x34, 0x4f }, // Y9
};

// usb hid scan code
#define HID_CAPS 0x39
#define HID_F12 0x45 // apple2 reset

//
// nano pins
//

// A2E KBD

#define PIN_SHIFT A2
#define PIN_RESET A3 // F12
#define PIN_CTRL  A4
#define PIN_CAPS  A5
// use RX(0), TX(1)
// NOTE: open jumper before upload firmware
#define PIN_PB1   0 // CAPL / LGUI / RALT
#define PIN_PB0   1 // OAPL / LALT
// XXX: nano A6-A7 is analog input only!
//#define PIN_PB1   A7 // CAPL / LGUI / RALT
//#define PIN_PB0   A6 // OAPL / LALT

// 4067

#define PIN_Y_S0  2
#define PIN_Y_S1  3
#define PIN_Y_S2  4
#define PIN_Y_S3  5
#define PIN_Y_INH A1

// 4051

#define PIN_X_S0  6
#define PIN_X_S1  7
#define PIN_X_S2  8

// USB host shield

#define PIN_INT   9
#define PIN_SS    10
#define PIN_MOSI  11
#define PIN_MISO  12
#define PIN_SCK   13

class KbdRptParser : public KeyboardReportParser {
    void PrintKey(uint8_t mod, uint8_t key);
    void OnControlKeysChanged(uint8_t before, uint8_t after);
    void OnKeyDown(uint8_t mod, uint8_t key);
    void OnKeyUp(uint8_t mod, uint8_t key);
};

void KbdRptParser::PrintKey(uint8_t m, uint8_t key)
{
  LOG_HEX(key);
  LOG(' ');
  char c = (char)OemToAscii(m, key);
  if (c) {
    LOG(c);
    LOG(' ');
  }
}

void KbdRptParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  MODIFIERKEYS mod;
  *((uint8_t*)&mod) = after;
  LOG(mod.bmLeftCtrl ? "LCTRL " : " ");
  LOG(mod.bmLeftShift ? "LSHFT " : " ");
  LOG(mod.bmLeftAlt ? "LALT " : " ");
  LOG(mod.bmLeftGUI ? "LGUI " : " ");
  LOG(mod.bmRightCtrl ? "RCTRL " : " ");
  LOG(mod.bmRightShift ? "RSHFT " : " ");
  LOG(mod.bmRightAlt ? "RALT " : " ");
  LOG(mod.bmRightGUI ? "RGUI " : " ");
  LOG_LINE();

  if (mod.bmLeftCtrl || mod.bmRightCtrl) {
    digitalWrite(PIN_CTRL, LOW);
    LOG_LINE("CTRL DN");
  } else {
    digitalWrite(PIN_CTRL, HIGH);
    LOG_LINE("CTRL UP");
  }
  if (mod.bmLeftShift || mod.bmRightShift) {
    digitalWrite(PIN_SHIFT, LOW);
    LOG_LINE("SHIFT DN");
  } else {
    digitalWrite(PIN_SHIFT, HIGH);
    LOG_LINE("SHIFT UP");
  }
  if (mod.bmLeftAlt) {
    digitalWrite(PIN_PB0, HIGH);
    LOG_LINE("OAPL(PB0) DN");
  } else {
    digitalWrite(PIN_PB0, LOW);
    LOG_LINE("OAPL(PB0) UP");
  }
  if (mod.bmLeftGUI || mod.bmRightAlt) {
    digitalWrite(PIN_PB1, HIGH);
    LOG_LINE("CAPL(PB1) DN");
  } else {
    digitalWrite(PIN_PB1, LOW);
    LOG_LINE("CAPL(PB1) UP");
  }
}

void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  PrintKey(mod, key);
  LOG_LINE(" DN");
  LED_ON();

  if (key == HID_CAPS) {
    LOG_LINE("CAPS DOWN");
    return;
    // caps lock handled in OnKeyUp
    //if (kbdLockingKeys.kbdLeds.bmCapsLock) {
    //  digitalWrite(PIN_CAPS, LOW);
    //  LOG_LINE("CAPS ON");
    //} else {
    //  digitalWrite(PIN_CAPS, HIGH);
    //  LOG_LINE("CAPS OFF");
    //}
  } else if (key == HID_F12) {
    digitalWrite(PIN_RESET, LOW);
    LOG_LINE("RESET DOWN");
  } else {
    for (int y = 0; y < MATRIX_ROWS; y += 1) {
      for (int x = 0; x < MATRIX_COLS; x += 1) {
        if (key == matrix[y][x]) {
          LOG("X:");
          LOG(x);
          LOG(",Y:");
          LOG(y);
          LOG_LINE();
          // set x
          digitalWrite(PIN_X_S0, x & 0b001 ? HIGH : LOW);
          digitalWrite(PIN_X_S1, x & 0b010 ? HIGH : LOW);
          digitalWrite(PIN_X_S2, x & 0b100 ? HIGH : LOW);
          // set y
          digitalWrite(PIN_Y_S0, y & 0b0001 ? HIGH : LOW);
          digitalWrite(PIN_Y_S1, y & 0b0010 ? HIGH : LOW);
          digitalWrite(PIN_Y_S2, y & 0b0100 ? HIGH : LOW);
          digitalWrite(PIN_Y_S3, y & 0b1000 ? HIGH : LOW);
          // enable: on x -> y
          digitalWrite(PIN_Y_INH, LOW);
          return;
        }
      }
    }
  }
}

void KbdRptParser::OnKeyUp(uint8_t mod, uint8_t key)
{
  PrintKey(mod, key);
  LOG_LINE(" UP");
  LED_OFF();

  if (key == HID_CAPS) {
    LOG_LINE("CAPS UP");
    if (kbdLockingKeys.kbdLeds.bmCapsLock) {
      digitalWrite(PIN_CAPS, LOW);
      LOG_LINE("CAPS ON");
    } else {
      digitalWrite(PIN_CAPS, HIGH);
      LOG_LINE("CAPS OFF");
    }
  } else if (key == HID_F12) {
    digitalWrite(PIN_RESET, HIGH);
    LOG_LINE("RESET UP");
  } else {
    // disable: off x -> y
    digitalWrite(PIN_Y_INH, HIGH);
  }
}

//
//
//

USB Usb;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD> HidKeyboard(&Usb);

KbdRptParser Prs;

//
//
//

void setup() {
  pinMode(PIN_SHIFT, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_CTRL, OUTPUT);
  pinMode(PIN_CAPS, OUTPUT);
  pinMode(PIN_PB0, OUTPUT);
  pinMode(PIN_PB1, OUTPUT);
  pinMode(PIN_Y_S0, OUTPUT);
  pinMode(PIN_Y_S1, OUTPUT);
  pinMode(PIN_Y_S2, OUTPUT);
  pinMode(PIN_Y_S3, OUTPUT);
  pinMode(PIN_Y_INH, OUTPUT);
  pinMode(PIN_X_S0, OUTPUT);
  pinMode(PIN_X_S1, OUTPUT);
  pinMode(PIN_X_S2, OUTPUT);

  digitalWrite(PIN_SHIFT, HIGH);
  digitalWrite(PIN_RESET, HIGH);
  digitalWrite(PIN_CTRL, HIGH);
  digitalWrite(PIN_CAPS, HIGH);
  digitalWrite(PIN_PB1, LOW);
  digitalWrite(PIN_PB0, LOW);
  digitalWrite(PIN_Y_INH, HIGH);

  LOG_INIT(19200);
  LED_INIT();

  if (Usb.Init() == -1) {
    LOG_LINE("USB Host Error!");
    while (1) {
      LED_ON();
      delay(100);
      LED_OFF();
      delay(100);
    }
  }

  HidKeyboard.SetReportParser(0, &Prs);

  LOG_LINE("HELLO!");
}

void loop() {
  Usb.Task();
}
