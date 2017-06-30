#include <Ticker.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// options
const char *ssid = "max-airrouter";
const char *password = "12345678";
const char *mqtt_server = "192.168.1.207";
const int mqtt_port = 1883;
const int led = LED_BUILTIN;
// set uuid & topics
int uuid = ESP.getChipId();

WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonBuffer<200> jsonBuffer;
Ticker blinker;


// generate topic names
char* genTopicName(char* suffix) {
  Serial.println("Function called");
  char concatenator[180] = {0};
  sprintf(concatenator, "leds/%i/%s", uuid, suffix);
  Serial.println(concatenator);
  return concatenator;
}

// set LED
bool LEDCurrent = false;
void setLED(bool stat)
{
  digitalWrite(led, stat);
  LEDCurrent = stat;
}
// blink LED
int blinkCounts = 0;
bool blinking = false;
bool blinkStatus = false;
int blinkDelay = 0;
// blink function for Ticker
void blinkTicker()
{
  if (blinkCounts == 0)
  {
    blinker.detach();
  }
  else
  {
    setLED(!blinkStatus);
    blinkStatus = !blinkStatus;
    blinkCounts--;
  }
}
// start blinker
void blink(int repeats, int switchDelay, bool initialStatus)
{
  // runs infinitely if repeats is set to below 0, with a ticker
  blinkCounts = 2 * repeats + 1;
  blinkStatus = !initialStatus;
  blinking = true;
  blinkDelay = switchDelay;
  blinker.attach_ms(switchDelay, blinkTicker);
}


void callback(char *Topic, byte *payload, unsigned int length)
{
  // log
  Serial.print("Message arrived on topic ");
  Serial.println(Topic);
  // parse json
  JsonObject &data = jsonBuffer.parseObject(payload);
  // set current status
  // act upon data

  // on/off/toggle/blink
  String state = data["state"];
  Serial.println("New State: " + state);
  if (state == "on") {
    blinker.detach();
    blinking = false;
    setLED(false);
  }
  else if (state == "off") {
    blinker.detach();
    blinking = false;
    setLED(true);
  }
  else if (state == "toggle") {
    blinker.detach();
    blinking = false;
    setLED(!LEDCurrent);
    if (LEDCurrent) {
    } else {
    }
  }
  else if (state == "blink") {
    if (data["infinite"] == "true")
    {
      blink(-1, data["delay"].as<int>(), false);
    } else {
      blink(data["blinkCounts"].as<int>(), data["delay"].as<int>(), false);
    }
  }
  jsonBuffer.clear();


  // create state object to send back
  JsonObject &newState = jsonBuffer.createObject();
  newState["state"] = !LEDCurrent ? "on" : "off";
  // if more advanced details are needed
  if (blinking)
  {
    newState["infinite"] = (blinkCounts < 0);
    newState["blinkCount"] = blinkCounts;
    newState["delay"] = blinkDelay;
  }
  // convert to json string
  char JSONMessageBuffer[100];
  newState.printTo(JSONMessageBuffer, sizeof(JSONMessageBuffer));
  // send
  jsonBuffer.clear();
  char* topic = genTopicName("state");
  Serial.println(JSONMessageBuffer);
  client.publish(topic, JSONMessageBuffer);
}
// subscribe to topics and announce existence
void subscribe()
{
  char* topic_1 = genTopicName("set_state_simple");
  Serial.println(topic_1);
  client.subscribe(topic_1);

  char* topic_2 = genTopicName("set_state");
  client.subscribe(topic_2);
  // announce existence:
  char* topic = "leds/manage/add";
  char concatenator[180];
  sprintf(concatenator, "{\"uuid\": %i, \"description\": \"\"}", uuid);
  client.publish(topic, concatenator);

}
// (re)connect to the server
void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    char uuid_string[10];
    dtostrf(uuid, 10, 0, uuid_string);
    if (client.connect(uuid_string))
    {
      Serial.println("connected");
      Serial.print(client.connected());
      subscribe();
    }
    else
    {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup()
{
  // set up LED:
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  // start serial for debugging
  Serial.begin(115200);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // WIFI setup
  WiFi.begin(ssid, password);
  // connect to wifi
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("MQTT client started");
  Serial.print("UUID is ");
  Serial.println(uuid);
}
void loop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}
