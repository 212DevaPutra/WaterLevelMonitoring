#define trig_pin 27
#define echo_pin 26
#define GreenLed 19
#define RedLed 15
#define relayIn 5

#include <ArduinoJson.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "PubSubClient.h"

char ssid[] = "nyrua";
char pass[] = "12345678";
BlynkTimer timer;

int interval = 0;

const char* mqtt_server = "192.168.84.132";
long lastMsg = 0;
char msg[50];
int value = 0;

WiFiClient espClient;
PubSubClient client(espClient);

//Set Water Level Distance in CM
float waterDistance() {
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);

  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);

  // Determine distance from duration
  // Use 343 metres per second as speed of sound
  // Divide by 1000 as we want millimeters
  long duration = pulseIn(echo_pin, HIGH);
  float distance = ((duration / 2) * 0.343) / 10;
  return distance;
}

void waterStatus() {
  float distance = waterDistance();
    if (distance <= 10) {
      Serial.println("Air Penuh");
      digitalWrite(relayIn, HIGH);
      digitalWrite(RedLed, LOW);
      stopTone();
    } else if (distance >= 75) {
      Serial.println("Air habis");
      digitalWrite(relayIn, LOW);
      digitalWrite(RedLed, HIGH);
      playTone();
    } else {
      Serial.println("Level Normal");
      stopTone();
    }  
}

void playTone() {
  ledcWriteTone(0, 1000);
  delay(1000);
  ledcWriteTone(0, 500);
}

void stopTone() {
  ledcWriteTone(0, 0);
}

void setupWiFi() {
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "control/all") {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, messageTemp);
    int state_relay = doc["state_relay"];
      if (state_relay == 1) {
        digitalWrite(relayIn, LOW);
        digitalWrite(RedLed, HIGH);
      } else {
        digitalWrite(relayIn, HIGH);
        digitalWrite(RedLed, LOW);
      }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("sensors/all");
      client.subscribe("control/all");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupWiFi();
  timer.setInterval(1L, waterStatus);
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);
  pinMode(GreenLed, OUTPUT);
  pinMode(RedLed, OUTPUT);
  pinMode(relayIn, OUTPUT);
  ledcSetup(0, 1E5, 12);
  ledcAttachPin(4, 0);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float distance = waterDistance();
  Serial.print("Jarak Air :");
  Serial.println(distance);
  digitalWrite(GreenLed, HIGH);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Koneksi WiFi terputus, mencoba kembali terhubung...");
    setupWiFi();
    digitalWrite(GreenLed, LOW);
  }
  timer.run();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    int waterLevelPer = map((int)distance, 100, 0, 0, 100);
    DynamicJsonDocument doc(1024);

    doc["distance"] = distance;
    doc["waterLevelPer"] = waterLevelPer;

    char JSONmessageBuffer[100];
    serializeJson(doc, JSONmessageBuffer, sizeof(JSONmessageBuffer));
    client.publish("sensors/all", JSONmessageBuffer);
  }
}