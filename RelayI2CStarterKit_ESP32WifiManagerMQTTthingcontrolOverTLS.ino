/*#################################An example to connect thingcontro.io MQTT over TLS1.2###############################

*/
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>

#include <Wire.h>
#include "Adafruit_MCP23008.h"

// Modbus
#include <ModbusMaster.h>
#include "REG_CONFIG.h"
#include <HardwareSerial.h>

HardwareSerial modbus(2);

#define WIFI_AP ""
#define WIFI_PASSWORD ""

String deviceToken = "mtdI6pDUu6WwkJRcJBTu";  //Sripratum@thingcontrio.io
//String deviceToken = "tOkvPadbQqLFsmc0sCON";
char thingsboardServer[] = "mqtt.thingcontrol.io";

String json = "";



ModbusMaster node;

//static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77"; // need to update every 3 months
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
const unsigned long periodCallBack = 1000;  //the value is a number of milliseconds
const unsigned long periodSendTelemetry = 10000;  //the value is a number of milliseconds


WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

// Basic toggle test for i/o expansion. flips pin #0 of a MCP23008 i2c
// pin expander up and down. Public domain

// Connect pin #1 of the expander to Analog 5 (i2c clock)
// Connect pin #2 of the expander to Analog 4 (i2c data)
// Connect pins #3, 4 and 5 of the expander to ground (address selection)
// Connect pin #6 and 18 of the expander to 5V (power and reset disable)
// Connect pin #9 of the expander to ground (common ground)

// Output #0 is on pin 10 so connect an LED or whatever from that to ground

Adafruit_MCP23008 mcp;


int status = WL_IDLE_STATUS;
String downlink = "";
char *bString;
int PORT = 8883;
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

// Modbus
struct Meter
{
  String temp;
  String hum;

};

Meter meter[10] ;
//signal meta ;

void setup() {

  Serial.begin(115200);

  modbus.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println(F("Starting... SHT20 TEMP/HUM_RS485 Monitor"));
  // communicate with Modbus slave ID 1 over Serial (port 2)
  node.begin(ID_meter, modbus);

  Serial.println();
  Serial.println(F("***********************************"));

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("@Thingcontrol_AP")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    //    ESP.reset();
    delay(1000);
  }

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );
  client.setCallback(callback);
  reconnectMqtt();

  Serial.print("Start..");
  startMillis = millis();  //initial start time
  mcp.begin();      // use default address 0

  mcp.pinMode(0, OUTPUT);
  mcp.pinMode(1, OUTPUT);
  mcp.pinMode(2, OUTPUT);

  mcp.pinMode(3, OUTPUT);

}

void loop()
{
  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }

  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)


  //check call back to connect Switch
  if (currentMillis - startMillis >= periodCallBack)  //test whether the period has elapsed
  {
    processCalled();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }

  //send telemetry
  if (currentMillis - starSendTeletMillis >= periodSendTelemetry)  //test whether the period has elapsed
  {

    sendtelemetry();
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
}


void getMac()
{
  Serial.println("OK");
  Serial.print("+deviceToken: ");
  Serial.println(WiFi.macAddress());
}

void viewActive()
{
  Serial.println("OK");
  Serial.print("+:WiFi, ");
  Serial.println(WiFi.status());
  if (client.state() == 0)
  {
    Serial.print("+:MQTT, [CONNECT] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
  else
  {
    Serial.print("+:MQTT, [FAILED] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
}

void setWiFi()
{
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("ThingControlCommand"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //    ESP.reset();
    delay(5000);
  }

  Serial.println("OK");
  Serial.println("+:WiFi Connect");

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );  // secure port over TLS 1.2
  client.setCallback(callback);
  reconnectMqtt();
}

void processCalled()
{
  Serial.println("OK");
  Serial.print("+:");
  Serial.println(downlink);
}



void processAtt(char jsonAtt[])
{

  char *aString = jsonAtt;

  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/attributes , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/attributes", aString);
}

void processTele(char jsonTele[])
{

  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/telemetry , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
}

void processToken()
{
  char *aString;

  //  aString = cmdHdl.readStringArg();

  Serial.println("OK");
  Serial.print("+:deviceToken , ");
  Serial.println(aString);
  deviceToken = aString;

  reconnectMqtt();
}

void unrecognized(const char *command)
{
  Serial.println("ERROR");
}

void callback(char* topic, byte* payload, unsigned int length)
{

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';
  Serial.println(json);
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  String methodName = String((const char*)data["method"]);
  Serial.println(methodName);

  if (methodName.startsWith("setValue"))
  {
    char json[length + 1];
    strncpy (json, (char*)payload, length);
    json[length] = '\0';

    downlink = json;
    String param = String((const char*)data["params"]);
    String meth =  String((const char*)data["method"]);
    Serial.print("param:"); Serial.println(param);

    Serial.println(meth);

    int relayId = meth.substring(meth.length() - 1, meth.length()).toInt();

    Serial.println(relayId);
    String aString = "{\"value";

    aString.concat(relayId);
    aString.concat("\":");
    if (param.equals("true") ) {

      mcp.digitalWrite(relayId, HIGH);

      Serial.println("on");
      aString.concat("1");
    } else if (param.equals("false")) {

      mcp.digitalWrite(relayId, LOW);
      Serial.println("off");
      aString.concat("0");

    }
    aString.concat("}");
    Serial.println("OK");
    Serial.print(F("+:topic v1/devices/me/attributes , "));
    Serial.println(aString);

    client.publish( "v1/devices/me/attributes", aString.c_str());

  }
  else
  {
    downlink = "";
  }
}

void reconnectMqtt()
{
  if ( client.connect("Thingcontrol_AT", deviceToken.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }

}


void read_Modbus(uint16_t  REG)
{

  static uint32_t i;
  uint8_t j, result;
  uint16_t data[2];
  uint16_t dat[2];
  uint32_t value = 0;


  // slave: read (6) 16-bit registers starting at register 2 to RX buffer
  result = node.readInputRegisters(REG, 2);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 2; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
  }
  for (int a = 0; a < 2; a++)
  {
    Serial.print(data[a]);
    Serial.print("\t");
  }
  Serial.println("");
  meter[0].temp = data[0];
  meter[0].hum =  data[1];
  Serial.println("----------------------");
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"temp\":");
  json.concat("33");
  //  json.concat(meter[0].temp);
  json.concat(",\"hum\":");
  json.concat("33");
  //  json.concat(meter[0].hum);
  json.concat("}");
  Serial.println(json);

  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);


  processTele(char_array);
  //}
}
