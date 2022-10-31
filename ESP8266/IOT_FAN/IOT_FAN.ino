#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Wire.h>
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


const int FAN_INA = D6;
const int FAN_INB = D5;
float speed = 0;

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
  pinMode(0, INPUT_PULLUP);
  pinMode(FAN_INA, OUTPUT);
  pinMode(FAN_INB, OUTPUT);
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
    client.setCallback(callback);
    while (!client.connected()) 
    {
      Serial.println("Connecting to MQTT...");
      if (client.connect("IOT_FAN", mqttUser, mqttPassword )) 
      {
        Serial.println("connected");
        client.publish("fan/status", TOPIC);
        client.subscribe(TOPIC);
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
    startFan();
    delay(100);
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

  if (MDNS.begin("IOT_FAN")) {
   Serial.println("MDNS responder started");
  }
  
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("HTTP server started");  
}

void setup_captive() {    
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("IOT_FAN");
  
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

void callback(char* topic, byte* payload, unsigned int length) 
{
  char payloadArray[10] = {'\0',};
  String payloadString;
  float temperature;
  float fanSpeed = 0.0;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) 
  {
    payloadArray[i] = (char)payload[i];
  }
  
  payloadString = String(payloadArray);
  temperature = payloadString.toFloat();
  Serial.println(temperature);

//  fanSpeed = (temperature-20) * 20;

  if(temperature < 20) fanSpeed = 0;
  else if(temperature > 30) fanSpeed = 255;
  else fanSpeed = costomMap(temperature, 20, 30, 120, 255);

  if(fanSpeed < 0)
  {
    speed = 0;
  }
  else if(fanSpeed > 255)
  {
    speed = 255;
  }
  else
  {
    speed = fanSpeed;
  }
}

void startFan()
{
  analogWrite(FAN_INA, 0);
  analogWrite(FAN_INB, speed);
}

void stopFan()
{
  analogWrite(FAN_INA, LOW);
  analogWrite(FAN_INB, LOW);
}

float costomMap(int x, float in_min, float in_max, float out_min, float out_max) {
  return (float)(((float)x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}
