# ebook-pageturner
Code for a simple page turner device for my kobo libra h2o

## Hardware
The sketch expects something like an esp8266 or esp32.
By default it will use the internal pullups on the esp8266 for the switches.
Then the common pin of the switches can be connected to gnd or another pin (driven low).

This makes the switch pins low when pressed. All parts of the above are configurable.


### Requirements:
- Some kind of ESP8266/ESP32 board installed in the Arduino IDE
- - e.g. https://arduino-esp8266.readthedocs.io/en/latest/index.html
- A telnet client library
- - e.g. https://github.com/alejho/Arduino-Telnet-Client
- - or https://github.com/videojedi/ESP8266-Telnet-Client


## General operation
The kobo libra at least (and likely others) have a developer mode.
This mode opens a telnet server on the kobo.

From that telnet prompt it is possible to:
- Record the touchscreen event (like a tap or a swipe) corresponding to page turns.
- Record to keypress event (like physical pageturn buttons) corresponding to page turns.

Then one can play them back to the kobo /dev/input/eventX on demand.

The wireless module logs in via telent and runs a shell script which makes a pageturn happen.
- (fw.sh and bw.sh, which rely on an .input file)


## ebook-pageturner configuration
- Take the *_creds.h.EXAMPLE files, update, and rename to .h files
- Install or copy the telnet library
- Open the .ino file in Arduino IDE
- Edit the configuration defines and variables to suit
- Build!
- Can use arduinoOTA to upload without serial cable.


## Kobo configuration
- Developer mode should be enabled.(devmodeon in search bar)
- Keep wifi on should be enabled. (in dev options)
- (recommended to change the telnet password too!) (via telnet on a computer)
- Either copy the example files in this repository to the usb device root...
- Or use telnet and make own scripts

Log in via telnet, and record a page turn forwards and back in files `/mnt/onboard/fw1_ev0.input' and '/mnt/onboard/bw1_ev0.input`
Create or copy the turnfw.sh and turnbw.sh scripts on the /mnt/onboard directory (i.e. where the books are)
Check execution via telnet on computer.



### Recording pageturns:
(Note the input event number may be different - this is for a libra h2o)

```
At the Kobo telnet root interface:
To record from touchscreen (/dev/input/event1)
# cat /dev/input/event1 > /mnt/onboard/fw1_ev1.input
touch the screen to record the event then
kill the cat (ctrl-c)

check the result, should be a few hundred bytes..
# ls -l /mnt/onboard/fw1_ev1.input

To replay the recorded event:
# cat /mnt/onboard/fw1_ev1.input > /dev/input/event1

Alternate for /dev/input/event0 - physical keys.
Note this kills nickel, so reboot is needed afterwards
# fuser -k /dev/input/event0 && cat /dev/input/event0 >> /mnt/onboard/fw1_ev0.input
(press and release the page turn button)
(kill the cat (ctrl-c))
(record the other direction)
# reboot

```



### LED configuration
- Generally LED is on when actively doing stuff
- E.g. On while connecting to wifi.
- e.g. On while sending pageturn.


### Future Plans
- Find kobo IP by mac address
- Support hosting a wifi network on the esp8266 - inc. dhcp.
- Sleep the espdevice between presses




### Versions used to compile:
- Arduino 1.8.13 on mac os x 10.13.6
- esp8266 boards module 3.0.2

