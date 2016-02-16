#include <FS.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <Button.h> 
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>


#define ECOSWITCH 1
#define NODEMCU 0

#if ECOSWITCH
#define BUTTON_PIN 13
#define LED_PIN 15
#endif

#if NODEMCU
#define BUTTON_PIN 0
#define LED_PIN 16
#endif

#define PULLUP true
#define INVERT true
#define DEBOUNCE_MS 100

Button myBtn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button

#define OFF 1
#define ON 0


MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiManager wifi;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//define your default values here, if there are different values in config.json, they are overwritten.
char device_name[40] = "espSwitch";
char mqtt_topic[120];
char mqtt_server[40];
char mqtt_port[6] = "1883";

int state = OFF;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


const char* index_html =
R"(<html><body><form method='POST' action='/' enctype='multipart/form-data'>
                  <fieldset>
                    <legend>General:</legend>
                    Switch Name: <input type='text' name='switchname'>
                  </fieldset>
                  <fieldset>
                    <legend>MQTT:</legend>
                    MQTT Server: <input type='text' name='mqttserver'>
                    MQTT Port: <input type='text' name='mqttport' value='1883'>
                    <input type='submit' value='Submit'>
                  </fieldset>
               </form></body></html>)";

void handleNotFound()
{
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ )
  {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}

	server.send ( 404, "text/plain", message );
}

void handle_root()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html",index_html);
}

void handle_post()
{
  if (server.args() == 3)
  {
    Serial.print("Name: ");
    Serial.println(server.arg(0));
    Serial.print("MQTT IP: ");
    Serial.println(server.arg(1));
    Serial.print("MQTT Port: ");
    Serial.println(server.arg(2));

    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["device_name"] = server.arg(0);
    json["mqtt_server"] = server.arg(1);
    json["mqtt_port"] = server.arg(2);

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
  else
  {
    Serial.println("Error");
  }
  server.send(200, "text/plain", "ok");
}

void handle_on()
{
  Serial.println("State: ON");
  state = ON;
}

void handle_off()
{
  Serial.println("State: OFF");
  state = OFF;
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //ON
  if ((char)payload[1] == 'N')
  {
    handle_on();
  }
  else
  {
    handle_off();
  }
}

long lastReconnect = 0;

void mqttReconnect()
{
  long now = millis();
  if (now - lastReconnect > 5000)
  {
    lastReconnect = now;
    Serial.print("Attempting MQTT connection as:");
    Serial.println(device_name);
    if (mqttClient.connect(device_name))
    {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          Serial.println("\nparsed json");
          strcpy(device_name, json["device_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_device_name("devicename", "device name", device_name, 40);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);

   //set config save notify callback
  wifi.setSaveConfigCallback(saveConfigCallback);
  
  //add all your parameters here
  wifi.addParameter(&custom_device_name);
  wifi.addParameter(&custom_mqtt_server);
  wifi.addParameter(&custom_mqtt_port);

  //read updated parameters
  strcpy(device_name, custom_device_name.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  wifi.autoConnect(device_name);
  
  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["device_name"] = device_name;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
  Serial.print("mDNS Name: ");
  Serial.println(device_name);

  if (!mdns.begin(device_name, WiFi.localIP()))
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
  Serial.print("Sketch size: ");
  Serial.println(ESP.getSketchSize());
  Serial.print("Free size: ");
  Serial.println(ESP.getFreeSketchSpace());
  
  server.on("/", HTTP_GET, handle_root);
  server.on("/", HTTP_POST, handle_post);
  server.on("/on", handle_on);
  server.on("/off", handle_off);
  server.onNotFound ( handleNotFound );

  httpUpdater.setup(&server);

  state = OFF;
  Serial.println("State: OFF");
  pinMode(LED_PIN, OUTPUT);

  Serial.print("local ip:");
  Serial.println(WiFi.localIP());

  Serial.print("mqtt server Name: ");
  Serial.println(mqtt_server);
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
  sprintf(mqtt_topic, "/%s/switch", device_name);
}

void loop()
{
  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  server.handleClient();
  mqttClient.loop();
  myBtn.read();

  if (myBtn.wasPressed())
  {
    if (state == ON)
    {
      Serial.println("State: OFF");
      mqttClient.publish(mqtt_topic, "OFF");
      state = OFF;
    }
    else
    {
      Serial.println("State: ON");
      mqttClient.publish(mqtt_topic, "ON");
      state = ON;
    }
  }
  if (myBtn.pressedFor(10000))
  {
    Serial.println("Factory Reset");
    SPIFFS.format();
    wifi.resetSettings();
    ESP.restart();
  }
  digitalWrite(LED_PIN, state);
}

