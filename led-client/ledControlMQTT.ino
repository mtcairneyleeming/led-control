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

class Blinker
{
public:
  bool isBlinking = false;
  int blinkCounts = -1;
  bool initialStatus = false;
  int blinkDelay = 500;

  Ticker blinker;

  void start()
  {
    self.blinker.attach_ms(switchDelay, self.blinkTicker);
  }
  void stop()
  {
    self.blinker.detach();
    // send state message
    publishState(jsonBuffer)
  }
  void set(bool initialState, int blinkDelay, int blinkCount = -1)
  {
    // runs infinitely if repeats is set to below 0, with a ticker
    self.blinkCounts = 2 * blinkCount + 1;
    self.initialStatus = !initialState;
    self.isBlinking = true;
    self.blinkDelay = blinkDelay;
  }
  // blink function for Ticker
  void blinkFunction()
  {
    if (blinkCounts == 0)
    {
      self.stop();
    }
    else
    {
      led.set(!blinkStatus);
      self.blinkStatus = !self.blinkStatus;
      self.blinkCounts--;
    }
  }
}
// LED class - control and state
class LED
{
private:
  int pin = LED_BUILTIN;

public:
  bool on = false;
  void set(bool state)
  {
    digitalWrite(self.pin, state);
    self.on = state;
  }
  void setup()
  {
    pinMode(led, OUTPUT);
    self.set(false);
  }
}

WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonBuffer<200> jsonBuffer;
Blinker blinker;
LED led;

// generate topic names
char *genTopicName(char *suffix)
{
  char concatenator[80] = {0};
  sprintf(concatenator, "leds/%i/%s", uuid, suffix);
  return concatenator;
}

void publishState(StaticJsonBuffer buffer)
{
  buffer.clear();
  // create state object to send back
  JsonObject &state = buffer.createObject();
  if (blinker.isBlinking)
  {
    // advanced state
    state["state"] = "blinking";
    state["blinkCount"] = blinker.blinkCounts;
    state["delay"] = blinker.blinkDelay;
    state["infinite"] = (blinker.blinkCounts < 0);
  }
  else
  {
    //basic state
    state['state'] = led.on ? "on" : "off";
  }
  char stateBuffer[100] = {0};
  state.printTo(stateBuffer.sizeof(stateBuffer));
  client.publish(genTopicName("state"), stateBuffer)
}

void callback(char *Topic, byte *payload, unsigned int length)
{
  JsonObject &data = jsonBuffer.parseObject(payload);
  String state = data["state"];
  blinker.stop();
  switch (state)
  {
  case "on":
    led.set(false);
    break;
  case "off":
    led.set(true);
    break;
  case "toggle":
    led.set(!led.on);
    break;
  case "blink":
    if (data["infinite"] == "true")
    {
      blinker.set(true, data["delay"].as<int>(), -1);
    }
    else
    {
      blinker.set(true, data["delay"].as<int>(), data["blinkCounts"].as<int>());
    }
  }
  publishState(jsonBuffer);
}
// subscribe to topics and announce existence
void MQTTBegin()
{
  client.subscribe(genTopicName("set_state_simple"));
  client.subscribe(genTopicName("set_state"));
  publishState(jsonBuffer);

  // announce existence:
  char *topic = "leds/manage/add";
  char concatenator[180];
  sprintf(concatenator, "{\"uuid\": %i, \"description\": \"\"}", uuid);
  client.publish(topic, concatenator);
}
// (re)connect to the server
void connectMQTT()
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
      MQTTBegin();
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
void connectWifi()
{
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
}
void setup()
{
  // set up LED:
  led.setup();
  // start serial for debugging
  Serial.begin(115200);
  // set up MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.print("UUID is ");
  Serial.println(uuid);
}
void loop()
{
  if (!client.connected())
  {
    connectMQTT();
  }
  client.loop();
}
