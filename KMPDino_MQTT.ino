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

#include "Config.h"
#include "Debug.h"
#include "Dino.h"

#include "PubSubClient.h"
#include "swRTC.h"
#include "TOTP.h"

//
// GLOBALS
//

// Timers
unsigned long timer1                 = 0;
unsigned long timer2                 = 0;
unsigned long timer3                 = 0;

const byte INTERVAL_NTP              = 10; // Minutes
const int INTERVAL_TOTP              = 1000; // Millis

// Ethernet
EthernetClient ethClient;
EthernetUDP udp;

// MQTT
char MQTT_CLIENT_ID[24];
const int MQTT_RECONNECT_DELAY       = 5000;

const char *TOPIC_CMD                = "CMD";
const char *TOPIC_RELAY              = "RELAY";
const char *TOPIC_OPTOIN             = "OPTOIN";
const char *TOPIC_AVAIL              = "AVAIL";

const byte TOPIC_LEN                 = 48;
const byte PAYLOAD_LEN               = 24;

PubSubClient mqttClient(ethClient);

// IO
bool optoin_status[OPTOIN_COUNT]     = {false};
bool relay_status[RELAY_COUNT]       = {false};
bool relay_available[RELAY_COUNT]    = {false};
bool relay_secure[RELAY_COUNT]       = {false};

const byte RELAY_CMD_COUNT            = 6;
const char *relay_cmd_names[]         = {"OFFLINE", "ONLINE", "OFF", "ON", "TOGGLE", "INSECURE", "SECURE"};

// NTP
const byte NTP_SEND_PACKETS          = 4;
const int NTP_RESPONSE_WAIT          = 1000;

// RTC
swRTC rtc;

// TOTP
TOTP totp                            = TOTP(hmacKey, 16);
const byte TOTP_COUNT                = 3;
const int totp_steps[TOTP_COUNT]     = {-60, -30, 0};
char totp_codes[TOTP_COUNT][7];

//
// TOTP
//

unsigned long get_ntp(UDP &udp, const char *timeServer) {
  if (!udp.begin(123)) {
    s_println(F("NTP > No socket available"));
    return 0;
  }

  for (byte x = 1; x <= NTP_SEND_PACKETS; x++) {
    const int NTP_PACKET_SIZE = 48;
    byte packetBuffer[NTP_PACKET_SIZE];

    // Clear packets
    while (udp.parsePacket() > 0)
      ;

    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    s_print(F("NTP > Send packet "));
    s_println(x);
    unsigned long t1 = millis();
    udp.beginPacket(timeServer, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    unsigned long timeout = millis() + NTP_RESPONSE_WAIT;
    while (millis() <= timeout) {
      int size = udp.parsePacket();

      if (size >= NTP_PACKET_SIZE) {
        s_print(F("NTP > Received response ("));
        s_print(millis() - t1);
        s_println(F(" ms)"));

        udp.read(packetBuffer, NTP_PACKET_SIZE);
        unsigned long secsSince1900;

        // Convert four bytes starting at location 40 to a long integer
        secsSince1900 = (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];

        udp.stop();

        // Convert to Unix TS (1970)
        return secsSince1900 - 2208988800UL;
      }
    }
  }

  udp.stop();
  return 0;
}

void set_rtc() {
  unsigned long now = get_ntp(udp, timeServer);

  if (now != 0) {
    rtc.stopRTC();
    rtc.setClockWithTimestamp(now);
    rtc.setDeltaT(rtc_deltaT);
    rtc.startRTC();
  } else {
    s_println(F("NTP > Failed, continue with current RTC"));
  }
}

void print_rtc() {
  s_print(F("RTC > "));
  s_print(rtc.getHours());
  s_print(F(":"));
  s_print(rtc.getMinutes());
  s_print(F(":"));
  s_print(rtc.getSeconds());

  s_print(F(" -- "));

  s_print(rtc.getDay());
  s_print(F("/"));
  s_print(rtc.getMonth());
  s_print(F("/"));
  s_println(rtc.getYear());
}

void generate_totp() {

  for (byte i = 0; i < TOTP_COUNT; i++) {
    unsigned long TS = rtc.getTimestamp() + totp_steps[i];
    char *code = totp.getCode(TS);
    
    if (strncmp(totp_codes[i], code, sizeof(totp_codes[i])) != 0) {
      strlcpy(totp_codes[i], code, sizeof(totp_codes[i]));
#ifdef SERIAL_PRINT_TOTP
      if (i == 0) print_rtc();
      s_print(F("TOTP > "));
      s_print(totp_codes[i]);

      s_print(F(" | "));
      s_println(TS);
#endif
    }
  }
}

bool validate_totp(const char *payload) {

  char code[7]; // 6 digits + null char
  byte code_len = (sizeof(code) - 1); // -1 = Subtract null char 
  byte payload_len = strlen(payload);
  
  if (code_len > payload_len) {
    s_println(F("TOTP > Payload too short"));
    return false;
  }
  
  // Assume that TOTP code is last 6 digits in the payload
  int start = payload_len - 1; // -1 = C array counts from 0
  int end   = payload_len - code_len;
    
  for (int x = start; x >= end; x--) {
    if (!isDigit(payload[x])) {
      s_println(F("TOTP > Failed parsing code"));
      return false;
    }

    int y = start - x;
    int z = end + y;
    code[y] = payload[z];
  }
  code[code_len] = '\0';

#ifdef SERIAL_PRINT_TOTP
    s_print(F("TOTP > Validating "));
    s_println(code);
#endif

  // Validate TOTP
  bool auth = false;

  for (byte i = 0; i < TOTP_COUNT; i++) {
    if (strncmp(totp_codes[i], code, sizeof(totp_codes[i])) == 0) auth = true;
  }
  
  if (auth) {
    s_println(F("TOTP > Code is valid"));
    return true;
  }

  s_println(F("TOTP > Code is invalid"));
  return false;
}

//
// MQTT
//

void mqtt_reconnect() {

  unsigned long t1 = millis();
  while (!mqttClient.connected()) {
    s_print(F("MQTT > Connecting... "));
    char topic[TOPIC_LEN];
    mqtt_build_topic(TOPIC_AVAIL, topic);
    
    bool state = false;
    if (strlen(MQTT_PASSWORD) > 0) {
      state = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, topic, 0, 1, available_names[0]);
    } else {
      state = mqttClient.connect(MQTT_CLIENT_ID, topic, 0, 1, available_names[0]);
    }

    if (state) {
      s_print(F("OK ("));
      s_print(millis() - t1);
      s_print(F(" ms) as "));
      s_println(MQTT_CLIENT_ID);

      mqtt_publish(TOPIC_AVAIL, available_names[1], true);
    } else {
      s_print(F("Failed, State "));
      s_print(mqttClient.state());
      s_println(F(", Retrying..."));
      delay(MQTT_RECONNECT_DELAY);
    }
  }
}

void mqtt_subscribe(const char *suffix) {

  char topic[TOPIC_LEN];
  mqtt_build_topic(suffix, topic);

  s_print(F("MQTT > Subscribe to "));
  s_print(topic);

  if (mqttClient.subscribe(topic)) {
    s_println(F(", Succeeded"));
  } else {
    s_println(F(", Failed"));
  }
}

void mqtt_subscription_router(char *topic, byte *payload, unsigned int length) {

#ifdef SERIAL_PRINT
  mqtt_subscription_print(topic, payload, length);
#endif

  // Get topic pointer (without <P_NAME>/<P_SERIAL>/)
  char *topic_s = topic + strlen(P_NAME) + 1 + strlen(P_SERIAL) + 1;
  char prefix[TOPIC_LEN];
  
  // Convert payload to string
  char payload_s[PAYLOAD_LEN];
  if (length > (PAYLOAD_LEN - 1)) {
    s_println(F("CMD > Max payload length exceeded"));
    return;
  }
  
  for (byte i = 0; i < length; i++) {
    payload_s[i] = (char)payload[i];
  }
  payload_s[length] = '\0';

  // Route topics & validate payload length
  // https://github.com/knolleary/pubsubclient/issues/318

  // CMD/RELAY/
  snprintf(prefix, TOPIC_LEN, "%s/%s", TOPIC_CMD, TOPIC_RELAY);
  if (str_starts_with(prefix, topic_s) && length > 0) {
    validate_cmd_relay(topic, payload_s);
    return;
  }
  
  // CMD/IO/STATUS
  snprintf(prefix, TOPIC_LEN, "%s/IO/STATUS", TOPIC_CMD);
  if (str_starts_with(prefix, topic_s)) {
    publish_io(true);
    return;
  }
  
  // CMD/INFO
  snprintf(prefix, TOPIC_LEN, "%s/INFO", TOPIC_CMD);
  if (str_starts_with(prefix, topic_s)) {
    publish_rtc();
    publish_uptime();
    publish_free_ram();
    return;
  }
  
  // CMD/TIME/SYNC
  snprintf(prefix, TOPIC_LEN, "%s/TIME/SYNC", TOPIC_CMD);
  if (str_starts_with(prefix, topic_s) && validate_totp(payload_s)) {
    set_rtc();
    print_rtc();
    publish_rtc();
    return;
  }

  // CMD/EEPROM/CLEAR
  snprintf(prefix, TOPIC_LEN, "%s/EEPROM/CLEAR", TOPIC_CMD);
  if (str_starts_with(prefix, topic_s) && validate_totp(payload_s)) {
    clear_eeprom();
    publish_io(true);
    return;
  }
}

void mqtt_subscription_print(char *topic, byte *payload, unsigned int length) {

  // default max. packet size is 128 bytes (MQTT_MAX_PACKET_SIZE)
  s_print(F("MQTT IN  < "));
  s_print(topic);
  s_print(F(" -- "));
  for (unsigned int i = 0; i < length; i++) {
    s_print((char)payload[i]);
  }
  s_print(F(" -- "));
  s_print(length);
  s_println();
}

void mqtt_publish(const char *suffix, const char *payload) { mqtt_publish(suffix, payload, false); }

void mqtt_publish(const char *suffix, const char *payload, bool retained) {

  char topic[TOPIC_LEN];
  mqtt_build_topic(suffix, topic);

  s_print(F("MQTT OUT > "));
  s_print(topic);
  s_print(F(" -- "));
  s_println(payload);

  if (!mqttClient.publish(topic, payload, retained)) {
    s_print(F("MQTT > Publish failed"));
  }
}

void mqtt_build_topic(const char *suffix, char *topic) {

  snprintf(topic, TOPIC_LEN,"%s/%s", P_NAME, P_SERIAL);

  if (strlen(suffix) > 0) {
    snprintf(topic, TOPIC_LEN, "%s/%s", topic, suffix);
  }
}

//
// CMD
//

void validate_cmd_relay(const char *topic, const char *payload) {
  
  // Validate relay (last digit in topic)
  byte last = strlen(topic)-1;
  byte relay = topic[last] - '0';
  if (!isDigit(topic[last]) || relay > (RELAY_COUNT - 1)) {
    s_print(F("CMD > Invalid relay "));
    s_println(relay);
    return;
  }

  // Validate TOTP
  if (is_relay_secure(relay) && !validate_totp(payload)) {
    return;
  }
  
  // Validate numeric cmd (first digit in payload)
  byte cmd = payload[0] - '0';
  if (isDigit(payload[0]) && cmd <= RELAY_CMD_COUNT) {
    return do_cmd_relay(relay, cmd);
  }

  // Validate cmd name
  for (byte i = 0; i <= RELAY_CMD_COUNT; i++) {
    if (str_starts_with(relay_cmd_names[i], payload)) {
      return do_cmd_relay(relay, i);
    }
  }
}

void do_cmd_relay(byte relay, byte cmd) {
  
  switch (cmd) {
    case 0:
    set_relay_available(relay, false);
    break;
    
    case 1:
    set_relay_available(relay, true);
    break;
    
    case 2:
    set_relay_state(relay, false);
    break;

    case 3:
    set_relay_state(relay, true);
    break;

    case 4:
    toggle_relay(relay);
    break;
    
    case 5:
    set_relay_secure(relay, false);
    break;

    case 6:
    set_relay_secure(relay, true);
    break;
  }
}

void publish_io() {

  publish_io(false);
}

void publish_io(bool force) {

  char topic[TOPIC_LEN];

  for (byte i = 0; i < OPTOIN_COUNT; i++) {
    bool state = digitalRead(optoin_pins[i]);

    // OPTOIN/<0-3> STATUS
    if (optoin_status[i] != state || force) {
      optoin_status[i] = state;

      snprintf(topic, TOPIC_LEN, "%s/%d", TOPIC_OPTOIN, i);
      mqtt_publish(topic, status_names[state], true);
    }
  }

  for (byte i = 0; i < RELAY_COUNT; i++) {
    bool state = digitalRead(relay_pins[i]);

    // RELAY/<0-3> STATUS
    if (relay_status[i] != state || force) {
      relay_status[i] = state;

      snprintf(topic, TOPIC_LEN, "%s/%d", TOPIC_RELAY, i);
      mqtt_publish(topic, status_names[state], true);
    }

    // RELAY/<0-3>/AVAIL
    bool available;
    EEPROM.get((i + RELAY_AVAIL_OFFSET), available);

    if (relay_available[i] != available || force) {
      relay_available[i] = available;

      snprintf(topic, TOPIC_LEN, "%s/%d/%s", TOPIC_RELAY, i, TOPIC_AVAIL);
      mqtt_publish(topic, available_names[available], true);
    }
    
    // RELAY/<0-3>/SECURE
    bool secure;
    EEPROM.get((i + RELAY_SECURE_OFFSET), secure);

    if (relay_secure[i] != secure || force) {
      relay_secure[i] = secure;

      snprintf(topic, TOPIC_LEN, "%s/%d/SECURE", TOPIC_RELAY, i);
      mqtt_publish(topic, secure_names[secure], true);
    }
  }
}

void publish_rtc() {

  char payload[PAYLOAD_LEN];
  snprintf(payload, PAYLOAD_LEN, "%lu", rtc.getTimestamp());
  mqtt_publish("RTC", payload);
}

void publish_uptime() {

  char payload[PAYLOAD_LEN];
  snprintf(payload, PAYLOAD_LEN, "%lu seconds", (millis() / 1000));
  mqtt_publish("UPTIME", payload);
}

void publish_free_ram() {

  char payload[PAYLOAD_LEN];
  snprintf(payload, PAYLOAD_LEN, "%d bytes", get_free_ram());
  mqtt_publish("RAM", payload);
}

//
// MAIN
//

void setup() {

#ifdef SERIAL_PRINT
  Serial.begin(9600);
#ifdef SERIAL_BLOCK_SETUP
  while (!Serial)
    ; // Wait for serial port to open (Leonardo only)
#endif
#endif

  // Init Prodino
  init_dino();
  get_product_serial(mac);
  // clear_eeprom();

  // Init Ethernet
  Ethernet.begin(mac, host);
  s_println(F("ETH > IP, GW, SUBNET, DNS"));
  s_println(Ethernet.localIP());
  s_println(Ethernet.gatewayIP());
  s_println(Ethernet.subnetMask());
  s_println(Ethernet.dnsServerIP());

  // Init MQTT
  snprintf(MQTT_CLIENT_ID, sizeof(MQTT_CLIENT_ID), "%s%s", P_NAME, P_SERIAL);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqtt_subscription_router);

  // Init RTC
  set_rtc();
  print_rtc();
}

void loop() {

  unsigned long now = millis();

  if (!mqttClient.connected()) {
    mqtt_reconnect();
    
    // Subscribe to <P_NAME>/<P_SERIAL>/CMD/#
    char suffix[TOPIC_LEN];
    snprintf(suffix, TOPIC_LEN, "%s/#", TOPIC_CMD);
    mqtt_subscribe(suffix);
    
    // Force publish IO on reconnect
    publish_io(true);
  }

  mqttClient.loop();

  // Publish IO states
  publish_io();

  // Sync NTP
  if (rtc.getMinutes() % INTERVAL_NTP == 0 && rtc.getSeconds() == 0 && (now - timer1) > 120000) {
    timer1 = now;
    set_rtc();
    print_rtc();
    publish_rtc();
  }

  // Generate codes
  if (now - timer2 >= INTERVAL_TOTP) {
    timer2 = now;
    generate_totp();
  }

#ifdef SERIAL_PRINT_RAM
  if (now - timer3 >= 5000) {
    timer3 = now;
    print_free_ram();
  }
#endif
}
