#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <Button.h> 

#define DEVICE_NAME "esp8266Switch"
#if 1
#define BUTTON_PIN 13
#define LED_PIN 15
#else
#define BUTTON_PIN 5
#define LED_PIN 6

#endif
#define PULLUP true
#define INVERT true
#define DEBOUNCE_MS 100

Button myBtn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button

#define OFF 0
#define ON 1


int state = 0;


String webString="";
MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void handle_root()
{
  webString="Switch state: "+String((int)state);   // Arduino has a hard time with float to string
  server.send(200, "text/plain", webString);            // send to someones browser when asked
  server.send(200, "text/plain", "open /on or /off to control outlet");
}

void handle_on()
{
  state = ON;
}

void handle_off()
{
  state = OFF;
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  if (digitalRead(BUTTON_PIN) == 0)
  {
    Serial.println("Factory Reset");
  }

  WiFiManager wifi;
  wifi.autoConnect(DEVICE_NAME);

  if (!mdns.begin(DEVICE_NAME, WiFi.localIP()))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println("mDNS responder started");
  }

  // Start TCP (HTTP) server
  server.begin();
  Serial.println("TCP server started");

  Serial.println("Listening on port 8266");

  Serial.print("Sketch size: ");
  Serial.println(ESP.getSketchSize());
  Serial.print("Free size: ");
  Serial.println(ESP.getFreeSketchSpace());
  
  server.on("/", handle_root);
  server.on("/on", handle_on);
  server.on("/off", handle_off);

  httpUpdater.setup(&server);

  state = OFF;
  Serial.println("State: OFF");
  pinMode(LED_PIN, OUTPUT);
}

void loop()
{
  server.handleClient();

  myBtn.read();

  if (myBtn.wasPressed())
  {
    if (state == ON)
    {
      state = OFF;
    }
    else
    {
      state = ON;
    }
  }
  digitalWrite(LED_PIN, state);

}

