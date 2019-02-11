# KMPDino MQTT

## Description

Arduino application for the [ProDiNo Ethernet V2](https://kmpelectronics.eu/products/prodino-ethernet-v2/) hardware. It allows controlling the relays and subscribe to optical input changes via MQTT.

### Features

* Set individual relay status to `offline` or `online` (remembered between reboots)
* Protect relay control with one-time-passwords (TOTP)

## Installation

* Copy `Config.example.h` to `Config.h` and configure settings.
* Variable `hmacKey` hexadecimal array is generated via the provided [TOTP Key Generator](TOTP%20Key%20Generator.html)

### Required libraries

* [KMP Prodino Ethernet 1.1.2](https://github.com/kmpelectronics/Arduino/tree/master/KMPDinoEthernet/src/Ethernet)
* [PubSubClient 2.6.0](https://github.com/knolleary/pubsubclient)
* [swRTC 1.2.8](https://github.com/leomil72/swRTC)
* [TOTP-Arduino 1.0.1](https://github.com/lucadentella/TOTP-Arduino)

## Usage

### MQTT Subscriptions

Topic | Payload | Usage
----- | ------- | -----
`KMPDINO/SERIAL/AVAIL` | `OFFLINE` or `ONLINE` | Prodino availability (Last Will message)
`KMPDINO/SERIAL/RAM` | `number of bytes` | RAM usage
`KMPDINO/SERIAL/RTC` | `Unix timestamp` | Current time
`KMPDINO/SERIAL/UPTIME` | `number of seconds` | Boot time
`KMPDINO/SERIAL/RELAY/<0-3>` | `OFF` or `ON` | State of relay
`KMPDINO/SERIAL/OPTOIN/<0-3>` | `OFF` or `ON` | State of optical in
`KMPDINO/SERIAL/RELAY/<0-3>/AVAIL` | `OFFLINE` or `ONLINE` | Availability of relay
`KMPDINO/SERIAL/RELAY/<0-3>/SECURE` | `INSECURE` or `SECURE` | Require TOTP code to control relay if set to secure

### MQTT Commands

Topic | Payload | Usage
----- | ------- | -----
`KMPDINO/SERIAL/CMD/RELAY/<0-3>` | `RELAY_CMD [TOTP]` | Set relay command by integer or string name with optional TOTP code
`KMPDINO/SERIAL/CMD/IO/STATUS` | | Publish status of all inputs & relays
`KMPDINO/SERIAL/CMD/INFO` | | Publishes RAM, RTC & UPTIME info
`KMPDINO/SERIAL/CMD/TIME/SYNC` | `TOTP` | Sync with NTP server
`KMPDINO/SERIAL/CMD/EEPROM/CLEAR` | `TOTP` | Clear EEPROM

#### Relay commands

Integer | Name
------- | ----
0 		| OFFLINE
1 		| ONLINE
2 		| OFF
3 		| ON
4 		| TOGGLE
5 		| INSECURE
6 		| SECURE

