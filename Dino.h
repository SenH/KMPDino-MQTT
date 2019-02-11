// MIT License

// Copyright (c) 2018 Sen Haerens

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Debug.h"

#include "Arduino.h"
#include "EEPROM.h"
#include "Ethernet.h"

//
// GLOBALS
//

const char *P_NAME                 = "KMPDINO";
const float P_SRAM                 = 2560.0;
char P_SERIAL[7];

const byte RELAY_COUNT             = 4;
const byte OPTOIN_COUNT            = 4;

const char *status_names[]         = {"OFF", "ON", "TOGGLE"};
const char *available_names[]      = {"OFFLINE", "ONLINE"};
const char *secure_names[]         = {"INSECURE", "SECURE"};

const byte RELAY_AVAIL_OFFSET      = 0;
const byte RELAY_SECURE_OFFSET     = RELAY_COUNT;
// const byte RELAY_SYNC_OFFSET       = RELAY_SECURE_OFFSET + RELAY_COUNT;

const int relay_pins[RELAY_COUNT]   = {0x04, 0x0C, 0x0B, 0x07};
const int optoin_pins[OPTOIN_COUNT] = {A2, A3, A4, A5};

const int StatusLedPin             = 0x0D;
const int W5200PowerPin            = 0x09;
const int W5200ResetPin            = 0x0A;

//
// IO
//

void init_dino() {

  // Set relays pins.
  for (byte i = 0; i < RELAY_COUNT; i++) {
    pinMode(relay_pins[i], OUTPUT);
  }

  // Set optoin pins
  for (byte i = 0; i < OPTOIN_COUNT; i++) {
    pinMode(optoin_pins[i], INPUT);
  }

  // Set status led pin.
  pinMode(StatusLedPin, OUTPUT);
  digitalWrite(StatusLedPin, LOW);

  // Init W5200
  pinMode(W5200PowerPin, OUTPUT);
  pinMode(W5200ResetPin, OUTPUT);

  // Stand by -> Power UP.
  digitalWrite(W5200PowerPin, LOW);

  // Reset -> Operating mode.
  digitalWrite(W5200ResetPin, HIGH);
}

static void set_relay(byte relay, bool value, const char *names[], byte offset) {

  s_print(F("IO > Set relay "));
  s_print(relay);
  s_print(F(" to "));
  s_println(names[value]);

  EEPROM.update((relay + offset), value);
}

static bool is_relay(byte relay, const char *names[], byte offset) {

  bool value;
  EEPROM.get((relay + offset), value);

#ifdef SERIAL_PRINT_VERBOSE
  s_print(F("IO > Relay "));
  s_print(relay);
  s_print(F(" is "));
  s_println(names[value]);
#endif
  return value;
}

void set_relay_available(byte relay, bool available) {

  set_relay(relay, available, available_names, RELAY_AVAIL_OFFSET);
}

void set_relay_secure(byte relay, bool secure) { 
  
  set_relay(relay, secure, secure_names, RELAY_SECURE_OFFSET); 
}

bool is_relay_available(byte relay) {

  return is_relay(relay, available_names, RELAY_AVAIL_OFFSET);
}

bool is_relay_secure(byte relay) {

  return is_relay(relay, secure_names, RELAY_SECURE_OFFSET);
}

void set_relay_state(byte relay, byte state) {

  if (!is_relay_available(relay)) {
    return;
  }

  s_print(F("IO > Set relay "));
  s_print(relay);
  s_print(F(" to "));
  s_println(status_names[state]);
  digitalWrite(relay_pins[relay], state);
}

void toggle_relay(byte relay) {

  bool state;

  if (!is_relay_available(relay)) {
    return;
  }

  s_print(F("IO > Toggle relay "));
  s_print(relay);
  state = 1;

  if (digitalRead(relay_pins[relay])) state = 0;

  digitalWrite(relay_pins[relay], state);
  s_print(F(" to "));
  s_println(status_names[state]);
}

//
// SYS
//

void get_product_serial(byte *mac) { 

  snprintf(P_SERIAL, sizeof(P_SERIAL), "%02X%02X%02X", mac[3], mac[4], mac[5]); 
}

int get_free_ram() {

  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void print_free_ram() {

  int freeRam = get_free_ram();

  s_print(F("SYS > Free RAM  = "));
  s_print((freeRam / P_SRAM) * 100);
  s_print(F("% ("));
  s_print(freeRam);
  s_println(F(" bytes)"));
}

void clear_eeprom() {

  s_print(F("SYS > Clearing EEPROM ("));
  s_print(EEPROM.length());
  s_println(F(" bytes)"));

  for (unsigned int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }

  s_println(F("SYS > EEPROM cleared!"));
}

//
// UTIL
//

bool str_starts_with(const char *pre, const char *str) { return strncmp(pre, str, strlen(pre)) == 0; }
