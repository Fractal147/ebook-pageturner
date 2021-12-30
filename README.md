# ebook-pageturner
Code for a simple page turner device for my kobo libra h2o

## Hardware
The sketch expects something like an esp8266 or esp32.
The switches are expected to be connected with a pulldown resistor to gnd or another pin.

### Requirements:
- Some kind of ESP8266/ESP32 board installed in the Arduino IDE
- - e.g. https://arduino-esp8266.readthedocs.io/en/latest/index.html
- A telnet client library
- - e.g. https://github.com/alejho/Arduino-Telnet-Client
- - or https://github.com/videojedi/ESP8266-Telnet-Client


## General operation
The kobo libra at least (and likely others) have a developer mode.
This mode opens a telnet server on the kobo.
From that telnet prompt, one can record the touchscreen event (like a tap or a swipe) corresponding to page turns.
Then one can play them back on demand.

The wireless module logs in via telent and plays back the touchscreen event to the kobo input, which makes a pageturn happen.



## Kobo configuration
- Developer mode should be enabled.
- Keep wifi on should be enabled.
- (recommended to change the telnet password too!)

Log in via telnet, and record a page turn forwards and back in files `/mnt/onboard/fw1.input' and '/mnt/onboard/bw1.input`

### Recording pageturns:
(Note the input event number may be different - this is for a libra h2o)

```
At the Kobo telent root interface:
# cat /dev/input/event1 > /tmp/record.input
touch the screen to record the event then
kill the cat (ctrl-c)

check the result, should be a few hundred bytes..
# ls -l /tmp/record.input

To replay the recorded event:
# cat /tmp/record.input > /dev/input/event1
```



### LED configuration
- Generally LED is on when actively doing stuff
- E.g. On while connecting to wifi.
- e.g. On while sending pageturn.


### Future bits
It would be nicer to simulate button presses, given the kobo has hardware pageturn buttons.
	But I'm not sure how!

### Versions used to compile:
- Arduino 1.8.13 on mac os x 10.13.6
- esp8266 boards module 3.0.2




