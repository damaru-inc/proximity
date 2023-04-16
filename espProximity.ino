//#include <ArduinoOTA.h>
//#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <WiFiUdp.h>
//#include <NTPClient.h>

const int triggerLedPin = D6;  //was D0, and D4 the built-in. The built-in still flashes.
const int flashLedPin = D5;
const int trigPin = D3;           //connects to the trigger pin on the distance sensor
const int echoPin = D1;           //connects to the echo pin on the distance sensor

// Get the chipid
uint32_t chipId = ESP.getChipId();

// distance in cm to detect motion
#define RANGE 70
// number of milliseconds detected before we send a message
#define TIME_THRESHOLD 300

//// Wifi Config
//const char* ssid = "Messaging 2.4";
//const char* password =  "Solnet1*";
const char* ssid = "changeme";
const char* password =  "changeme";

//// Solace Config
const char* mqttServer = "changeme.messaging.solace.cloud";
const int mqttPort = 1883;
const char* mqttUser = "solace-cloud-client";
const char* mqttPassword = "changeme";
char mqttClientId[80];


// NTP config
const long utcOffsetInSeconds = 0; //1.5 * 3600; // who knows why we're behind 90 minutes? Should be: // -4 * 3600; // 4 hours behind UTC

char dataTopic[80];
char controlTopic[80];
const char* willMessage = "disconnected";
char message[80];
unsigned long loopCounter = 0;
unsigned long lastPubSubReconnect = 0;
bool detected = false;
bool messageSent = false;
float distance = 0;
long firstDetected = 0;


WiFiClient espClient;
PubSubClient pubSubClient(espClient);
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "time.chu.nrc.ca", utcOffsetInSeconds);


void loop() {

  long now = millis();
  distance = getDistance();   //variable to store the distance measured by the sensor

  Serial.print(distance);     //print the distance that was measured
  Serial.println(" cm");      //print units after the distance

  if (distance <= RANGE) {                       //if the object is close
    if (!detected) {
      detected = true;
      digitalWrite(triggerLedPin, HIGH);
      firstDetected = now;
    }

    if (now - firstDetected > TIME_THRESHOLD) {
      if (!messageSent) {
        if (pubSubClient.connected()) {
          Serial.printf("distance: %4.2f heap: %d\n", distance, ESP.getFreeHeap());
          //  Serial.printf("sensorVal: %4d volts: %2.2f temp: %2.2f\n", sensorVal, volts, temp);
          //sprintf(message, "%lu|%d|%d", time, val, ESP.getFreeHeap());
          sprintf(message, "%d", (int) distance);
          pubSubClient.publish(dataTopic, message);
          messageSent = true;
        }
      }
    }
  } else {
//    if (messageSent) { // We were sending the 0 message for some android mqtt client. No longer needed.
//      pubSubClient.publish(dataTopic, "0");
//    }
    detected = false;
    messageSent = false;
    digitalWrite(triggerLedPin, LOW);
  }

  loopCounter++;

  if ((loopCounter % 5 == 0) && !pubSubClient.connected()) {
    if (now - lastPubSubReconnect > 5000) {
      lastPubSubReconnect = now;
      // Attempt to reconnect
      if (pubSubReconnect()) {
        lastPubSubReconnect = 0;
      } else {
        flashLed(500);
      }
    }
  } else {
    // Client connected
    pubSubClient.loop();
    if (loopCounter % 10 == 0) {
      flashLedTimes(2);
    }
    if (loopCounter % 80 == 0) {
      pubSubClient.publish(controlTopic, "ping");
    }
  }

  delay(300);
}

void flashLed(int mills) {
  digitalWrite(flashLedPin, HIGH);
  delay(mills);
  digitalWrite(flashLedPin, LOW);
}

void flashLedTimes(int times) {
  for (int i = 0; i < times; i++) {
    if (i > 0) {
      delay(100);
    }
    flashLed(100);
  }
}

//RETURNS THE DISTANCE MEASURED BY THE HC-SR04 DISTANCE SENSOR
float getDistance()
{
  float echoTime;                   //variable to store the time it takes for a ping to bounce off an object
  float calculatedDistance;         //variable to store the distance calculated from the echo time

  //send out an ultrasonic pulse that's 10ms long
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  echoTime = pulseIn(echoPin, HIGH);      //use the pulsein command to see how long it takes for the
  //Serial.printf("echoTime: %f\n", echoTime);
  //pulse to bounce back to the sensor

  //calculatedDistance = echoTime / 148.0;  //calculate the distance of the object that reflected the pulse (half the bounce time multiplied by the speed of sound)
  calculatedDistance = echoTime / 58.2;  // in cm

  return calculatedDistance;              //send back the distance that was calculated
}

void setup() {
  pinMode(triggerLedPin, OUTPUT);
  pinMode(flashLedPin, OUTPUT);
  pinMode(trigPin, OUTPUT);   //the trigger pin will output pulses of electricity
  pinMode(echoPin, INPUT);    //the echo pin will measure the duration of pulses coming back from the distance sensor
  Serial.begin(115200);
  Serial.printf("\nchipId: %ld     mem: %ld\n", chipId, ESP.getFreeHeap());
  flashLedTimes(5);
  setupWiFi();
  Serial.printf("After setupWifi   mem: %ld\n", ESP.getFreeHeap());
  //timeClient.begin();
  //Serial.printf("After timeClient. mem: %ld", ESP.getFreeHeap());
  setupPubSub();
  flashLedTimes(5);
}

void setupWiFi() {
  // Connect to WIFI
  Serial.println(" Connecting to wifi");
  WiFi.mode(WIFI_STA);

  WiFi.setAutoConnect (true);
  WiFi.setAutoReconnect (true);

  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void setupPubSub() {
  pubSubClient.setServer(mqttServer, mqttPort);
  //sprintf(dataTopic, "proximity/data/%lu", chipId);
  sprintf(dataTopic, "proximity/data");
  sprintf(controlTopic, "proximity/control/%lu", chipId);
  sprintf(mqttClientId, "NodeMcu-%lu", chipId);
}

boolean pubSubReconnect() {
  int willQos = 0;
  boolean willRetain = false;
  Serial.printf("Attempting to connect to %s %d\n", mqttServer, mqttPort);
  if (pubSubClient.connect(mqttClientId, mqttUser, mqttPassword, controlTopic, willQos, willRetain, willMessage )) {
    // Get the current time
    //timeClient.update();
    //unsigned long time = timeClient.getEpochTime();
    Serial.println("connected");
    //sprintf(message, "%lu|%s", time, "Connected.");
    pubSubClient.publish(controlTopic, "connected");
  } else {
    Serial.print("Not connected: ");
    Serial.println(pubSubClient.state());
  }

  return pubSubClient.connected();
}
