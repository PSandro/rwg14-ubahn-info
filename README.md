RWG14 U-Bahn info lcd
==

This is a quick project to display the u-bahn departures on a tiny LCD-Display drived by an ESP-8266.

The code is messy and buggy, but it works most of the times.

Feel free to contribute.


## Setup

copy `include/wifi-creds.h.example` to `include/wifi-creds.h` and edit the credentials accordingly.

build the project using platformio


Connect the esp8266, the build and upload using
```bash
pio run -t upload
```
