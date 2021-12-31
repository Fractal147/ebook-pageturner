
/*
  this is dead simple hardware, with one or two buttons,
  each triggering a script.

  Button interfacing and debouncing..
  A press must last at least btn_min_press length


  Plan for power draw:
  V1 implementation: - constantly powered, connects to telnet on start.
  V2 implementation -  some form of light sleep, button triggers waking etc.
    Also detects and retries telnet on fail.
    Note telnet library needs tweaking to do anything other than print.
  V3 - some very fast wake??

  Plan for telneting:
  MVP: blind sending.
  V2 :  check for an 'ok' or something back.

  Plan for mDNS:
  Find the Kobo IP address!
  Or could use mac address?

  Logging into telnet - essentially it waits for ':' and then tries 'root'
  and then if it sees '#' it's working
  maybe it could 'echo test'


  //Note that delay() does yield and allow wifi/tcpip tasks to run.
  //https://arduino-esp8266.readthedocs.io/en/latest/reference.html?highlight=delay()#timing-and-delays


*/

#include "telnet_creds.h" //Do change local *_creds.h.EXAMPLE files to *_creds.h when filled in.
#include "wifi_creds.h"

#include <ESP8266TelnetClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h> //also used for OTA
#include <WiFiUdp.h> //also used for telnet?
#include <ArduinoOTA.h>


//Configuration switches - fill with 1/0 or true/false
#define CONFIG_OUTPUT_PULLDOWN_FOR_BUTTONS false //if a pin is being used to provide 'gnd' for the switches.
//#define CONFIG_HOLD_PRESS_FOR_REVERSE false //not currently implemented
#define CONFIG_SEPARATE_REVERSE_BUTTON true
#define CONFIG_SECOND_LED false
#define CONFIG_DEBUG_SERIAL true //this may break horribly if false



//Pin definitions.
const int fwButtonPin = D1; //AKA GPIO5
const int bwButtonPin = D2; //AKA GPIO4 //Used if CONFIG_SEPARATE_REVERSE_BUTTON is true
const int pullPin = D3; //AKA GPIO0 //Used if CONFIG_OUTPUT_PULLUP_FOR_BUTTONS is true
#define BTN_PRESS 0 //What a pressed button reads as - 1 or 0.


const int ledPin  = D4;//or try LED_BUILTIN! AKA GPIO2 - on esp12f blue, arduino 17, nodemcu D4, wemos mini d4
const int ledErrPin = D0;//AKA GPIO16 //Used if CONFIG_SECOND_LED is true

#define LED_ON 0 //set as appropriate for the led polarity
#define LED_OFF !LED_ON


//How to find the e-reader:
//Ideally would use mac address or something, or mdns...
IPAddress ebookIP (192, 168, 0, 45);
const uint16_t telnet_port = 23;


//Regular commands to send
//Note can easily replace with .sh files to run with less text transferred-->faster
//const char pagefw_cmd[] = "cat /mnt/onboard/fw1.input > /dev/input/event1 && echo ok";
//const char pagebw_cmd[] = "cat /mnt/onboard/bw1.input > /dev/input/event1 && echo ok";
const char pagefw_cmd[] = "./fw.sh";
const char pagebw_cmd[] = "./bw.sh";
const char success_string[] = "ok"; //What it looks for to know it has succeeded. //not currently implemented.
//const char extra_cmd[]=  "&& echo ok"



//Debouncing:
//The state is checked every loop- roughly 1-2 times per millisecond.
//To count as a press, the button needs to be held down sufficiently long
//Note that ideally it needs to discern between double presses, long presses, etc.

//Also it should ignore sufficiently short pulses of either state.
//It is up to coder to ensure that short/long/double presses are appropriately distinct.
//Note that having a double press must delay a single press being generated...until it's been off for long enough.

#define DEBOUNCE_IGNORE_SHORTER_THAN_MS 5 //also used at the end of a press.

#define DEBOUNCE_SHORT_PRESS_MIN_MS 100
#define DEBOUNCE_SHORT_PRESS_MAX_MS 1000

//#define DEBOUNCE_DOUBLE_PRESS_GAP_MIN_MS 100 //not implemented
//#define DEBOUNCE_DOUBLE_PRESS_GAP_MIN_MS 500 //not implemented

#define DEBOUNCE_LONG_PRESS_MIN_MS 1000
#define DEBOUNCE_LONG_PRESS_MAX_MS 3000


typedef enum b_o {
  BUTTON_NONE, //so it reads as FALSE
  BUTTON_SHORT_PRESS,
  BUTTON_LONG_PRESS,
  BUTTON_DOUBLE_PRESS //not currently implemented
} button_outputs_e;

typedef struct dB  {
  //ATM designed for single presses.
  unsigned long pressStartTime;
  unsigned long pressEndTime;

  unsigned long lastDeglitchedStateChangeTime;
  int last_state_deglitched;

} debouncedButton_s;



button_outputs_e check_button(int btn_pin, debouncedButton_s * btn_s) {
  //Normalise everything so that 1 = pressed, and 0 = unpressed.
  int current_state  = (digitalRead(btn_pin) == BTN_PRESS);

  unsigned long current_time = millis();


  //If there is a state change:
  if (btn_s->last_state_deglitched != current_state) {
    
    //then if it's long enough, it becomes the new deglitched state.
    if ((current_time - btn_s->lastDeglitchedStateChangeTime) >= DEBOUNCE_IGNORE_SHORTER_THAN_MS) {

      btn_s->last_state_deglitched = current_state;
      btn_s->lastDeglitchedStateChangeTime = current_time;

      if (current_state) {
        //just been pressed ON
        btn_s->pressStartTime = btn_s->lastDeglitchedStateChangeTime;
      } else {
        //just been switched OFF
        btn_s->pressEndTime = btn_s->lastDeglitchedStateChangeTime;
      }
    }
  }
  


  //if the press has finished, decide if it's long or short
  if (btn_s->last_state_deglitched == 0 && btn_s->pressEndTime > 0) {
    unsigned long duration = (btn_s->pressEndTime - btn_s->pressStartTime);

    if (duration >= DEBOUNCE_SHORT_PRESS_MIN_MS && duration < DEBOUNCE_SHORT_PRESS_MAX_MS) {
      //Tidy up...
      btn_s->pressStartTime = 0;
      btn_s->pressEndTime = 0;
      return BUTTON_SHORT_PRESS;
    }
    if (duration >= DEBOUNCE_LONG_PRESS_MIN_MS && duration < DEBOUNCE_LONG_PRESS_MAX_MS) {
      //Tidy up..
      btn_s->pressStartTime = 0;
      btn_s->pressEndTime = 0;
      return BUTTON_LONG_PRESS;
    }
  }
  //otherwise end
  return BUTTON_NONE;
}

//Globals:
bool telnet_connected = false; //global for tracking if connected
WiFiClient client;
ESP8266telnetClient tc(client);



//Function that wraps telnet sends to only do it when connected.
//ideally also returns whatever it returns!!!!
int telnet_send(const char * outstring) {
  //wraps to safely call when deb - maybe in future do some timeouts?
  digitalWrite(ledPin, LED_ON);
  int retval = 1;
  if (telnet_connected) {
    tc.sendCommand(outstring);
    retval = 0;
  }
  else {
    Serial.print("Not");
  }
  Serial.print("Sent: ");
  Serial.println(outstring);
  digitalWrite(ledPin, LED_OFF);
  return retval;
}


//Wifi setups
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;



void arduinoOTASetup() {

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}



void setup() {


  //LEDs
  pinMode(ledPin, OUTPUT);
#if CONFIG_SECOND_LED
  pinMode(ledErrPin, OUTPUT);
#endif

  //Turn on LED state:
  digitalWrite(ledPin, LED_ON);

  //Button configurations
  //If BTN_PRESS = 0, then the button pins should be pulled up.
#if !BTN_PRESS
#define BTN_INPUT_MODE INPUT_PULLUP
#else
#define BTN_INPUT_MODE INPUT
#endif

  pinMode(fwButtonPin, BTN_INPUT_MODE);

#if CONFIG_SEPARATE_REVERSE_BUTTON
  pinMode(bwButtonPin, BTN_INPUT_MODE);
#endif



#if CONFIG_OUTPUT_PULLDOWN_FOR_BUTTONS
  pinMode(pullPin, OUTPUT);
  digitalWrite(pullPin, BTN_PRESS); //set the pull pin to match button press. (i.e. pulldown)
#endif

  //debug serial
#if CONFIG_DEBUG_SERIAL
  Serial.begin(115200);
#endif
  Serial.println("");



  // connect to wifi network:
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");

  //run ota stuff
  arduinoOTASetup();



  //readyish
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  //setup telnet
  tc.setPromptChar('#');


  // try and telnet in until it gets something back
  boolean success_bool = 0;
  int fail_count = 0;
  Serial.println("Trying to telnet into device");
  do {
    digitalWrite(ledPin, LED_ON);
    success_bool = tc.login(ebookIP, TELNET_USER, TELNET_PASSWORD, telnet_port);
    digitalWrite(ledPin, LED_OFF);
    if (!(success_bool)) {
      tc.disconnect();
      fail_count++;
      Serial.print(".");
      delay(500);
    }
  }
  while (success_bool == 0 && fail_count < 2);

  if (!success_bool) {
    Serial.println("Failed to log in, any serial to not restart");
    delay(1000);
    if (!(Serial.available())) {
      Serial.println("Restarting");
      ESP.restart();
    }
  }
  else {
    telnet_connected = true;
    Serial.println("Logged in");
  }


 

  // if it's "kobo login:", then send user, and if it exists, password
  // if it's #, then ready - pass into loop //not implemented.
  telnet_send("echo start");
  telnet_send("cd /mnt/onboard");


  //  set up the interrupts that listen for button presses.
  //  attachInterrupt(digitalPinToInterrupt(fwButtonPin), interrupt_fw, FALLING)
  //  #if CONFIG_SEPARATE_REVERSE_BUTTON
  //  attachInterrupt(digitalPinToInterrupt(bwButtonPin), interrupt_bw, FALLING)
  //  #endif

  digitalWrite(ledPin, LED_OFF);

}


void loop() {

  static button_outputs_e fw_pressed = BUTTON_NONE;
  static button_outputs_e bw_pressed = BUTTON_NONE;
  static debouncedButton_s fwButton_s; //Initied to zero by default
  static debouncedButton_s bwButton_s; //Initied to zero by default

  ArduinoOTA.handle();
  

  fw_pressed = check_button(fwButtonPin, &fwButton_s);
  if (fw_pressed==BUTTON_SHORT_PRESS) {
    Serial.println("FW pressed");
    telnet_send(pagefw_cmd);
    //fw_pressed = 0;
  }

#if CONFIG_SEPARATE_REVERSE_BUTTON
  bw_pressed = check_button(bwButtonPin, &bwButton_s);
  if (bw_pressed==BUTTON_SHORT_PRESS) {
    Serial.println("BW pressed");
    telnet_send(pagebw_cmd);
    //bw_pressed = 0;
  }
#endif

  yield(); //called automatically at the end of loop anyway, probably.
}
