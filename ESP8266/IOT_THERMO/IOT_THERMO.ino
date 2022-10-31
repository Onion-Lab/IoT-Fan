#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Wire.h>

#include <OneWire.h>
#include <DallasTemperature.h>


// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS D2

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

#define EEPROM_LENGTH 1024
#define TOPIC "FAN_221011/temperature"

const char*   mqttServer = "test.mosquitto.org";
const int     mqttPort = 1883;
const char*   mqttUser = "chan";
const char*   mqttPassword = "chan";


char eRead[30];
byte len;
char ssid[30];
char password[30];

bool captive = true;

WiFiClient espClient;
PubSubClient client(espClient);
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String responseHTML = ""
    "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Wi-Fi Setting Page</title></head><body><center>"
    "<p>Wi-Fi Setting Page</p>"
    "<form action='/button'>"
    "<p><input type='text' name='ssid' placeholder='SSID' onblur='this.value=removeSpaces(this.value);'></p>"
    "<p><input type='text' name='password' placeholder='WLAN Password'></p>"
    "<p><input type='submit' value='Submit'></p></form>"
    "<p>Wi-Fi Setting Page</p></center></body>"
    "<script>function removeSpaces(string) {"
    "   return string.split(' ').join('');"
    "}</script></html>";


void setup() 
{
  Serial.begin(9600);
  EEPROM.begin(EEPROM_LENGTH);
  sensors.begin();  // Start up the library
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(0, initDevice, FALLING);

  ReadString(0, 30);
  if (!strcmp(eRead, ""))
    setup_captive();
  else 
  {
    captive = false;
    strcpy(ssid, eRead);
    ReadString(30, 30);
    strcpy(password, eRead);

    Serial.println(TOPIC);

    setup_runtime();  
    client.setServer(mqttServer, mqttPort);
    while (!client.connected()) 
    {
      Serial.println("Connecting to MQTT...");
      if (client.connect("IOT_THERMO", mqttUser, mqttPassword )) 
      {
        Serial.println("connected");
        //client.publish("fan/status", TOPIC);
      } 
      else 
      {
        Serial.print("failed with state "); Serial.println(client.state());
        ESP.restart();
      }
    }
  }
}

void loop() {
  if (captive)
  {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  else
  {
    char buf[10];
    sprintf(buf, "%.2lf", getTemperature());
    client.publish(TOPIC, buf);
//    delay(500);
  }
  client.loop();
}


void setup_runtime() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.print(".");
    yield;
    if(i++ > 20) 
    {
      ESP.restart();
      return; 
    }
  }
  Serial.println("");
  Serial.print("Connected to "); Serial.println(ssid);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  if (MDNS.begin("IOT_THERMO")) {
   Serial.println("MDNS responder started");
  }
  
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("HTTP server started");  
}

void setup_captive() {    
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("IOT_THERMO");
  
  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/button", button);

  webServer.onNotFound([]() {
    webServer.send(200, "text/html", responseHTML);
  });
  webServer.begin();
  Serial.println("Captive Portal Started");
}

void button()
{
  Serial.println("button pressed");
  Serial.println(webServer.arg("ssid"));
  SaveString(0, (webServer.arg("ssid")).c_str());
  SaveString(30, (webServer.arg("password")).c_str());
  webServer.send(200, "text/plain", "OK");
  ESP.restart();
}

void SaveString(int startAt, const char* id) 
{ 
  for (byte i = 0; i <= strlen(id); i++)
  {
    EEPROM.write(i + startAt, (uint8_t) id[i]);
  }
  EEPROM.commit();
}

void ReadString(byte startAt, byte bufor) 
{
  for (byte i = 0; i <= bufor; i++) {
    eRead[i] = (char)EEPROM.read(i + startAt);
  }
  len = bufor;
}

void handleNotFound()
{
  String message = "File Not Found\n";
  webServer.send(404, "text/plain", message);
}

ICACHE_RAM_ATTR void initDevice() {
    Serial.println("Flushing EEPROM....");
    SaveString(0, "");
    ESP.restart();
}

float getTemperature()
{
  float ret;
  sensors.requestTemperatures();
  ret = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(ret);
  Serial.print("C\n");
  return ret;
}
