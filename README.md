# ebook-pageturner
Code for a simple page turner device for my kobo libra h2o

## Hardware
The sketch expects something like an esp8266 or esp32.
The switches are expected to be connected with a pulldown resistor to gnd or another pin.

## General operation
The kobo libra at least (and likely others) have a developer mode.
This mode opens a telnet server on the kobo.
From that telnet prompt, one can record the touchscreen event (like a swipe) corresponding to page turns.
Then one can play them back on demand.


## Kobo configuration
Developer mode should be enabled.
Keep wifi on should be enabled.
(reccomended to change the telnet password too!)
Log in via telnet, and record a page turn forwards and back in files `/mnt/onboard/tapfw.input' and '/mnt/onboard/tapbw.input`

### Recording pageturns:
```
#At the Kobo telent root interface:
# cat /dev/input/event1 > /tmp/record.input
touch the screen to record the event then
kill the cat (ctrl-c)

check the result, should be a few hundred bytes..
# ls -l /tmp/record.input

To replay the recorded event:
# cat /tmp/record.input > /dev/input/event1
```


### Future bits
It would be nicer to simulate button presses, given the kobo has hardware pageturn buttons.
	But I'm not sure how.

## Running the sketch
The sketch should compile and run - and allow arduinoOTA to work too.
It will need some telnet library - e.g. ESP8266TelnetClient.h
