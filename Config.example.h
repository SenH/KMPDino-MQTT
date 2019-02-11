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

#include "Ethernet.h"

//
// Debug
//

// #define SERIAL_BLOCK_SETUP
#define SERIAL_PRINT
// #define SERIAL_PRINT_VERBOSE
// #define SERIAL_PRINT_TOTP
// #define SERIAL_PRINT_RAM

//
// Ethernet
//

// Wiznet prefix 00:08:DC
byte mac[]                           = {0x00, 0x08, 0xDC, 0x00, 0x00, 0x00};
IPAddress host(192, 0, 2, 1);

//
// MQTT
//

const char *MQTT_SERVER              = "MQTT_SERVER";
const int MQTT_PORT                  = 1883;
const char *MQTT_USER                = "MQTT_USER";
const char *MQTT_PASSWORD            = "MQTT_PASSWORD";

//
// RTC
//

const char *timeServer               = "NTP_SERVER";
const int rtc_deltaT                 = 3250; // tenths of seconds per day

//
// TOTP
//

// Generate via TOTP Key Generator.html
uint8_t hmacKey[]                    = {};
