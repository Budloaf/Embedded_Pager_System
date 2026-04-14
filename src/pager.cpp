/*************************************************************************************
 *     Program name: Lab07.cpp
 *          Version: 2.2
 *             Date: Feb 11, 2017 / Mar 29, 2024
 *           Author: Greg Nordstrom, John Hutton
 *         Modified: Hayden Berry
 *             Date: Apr 18, 2024
 *         Platform: ESP32
 *    Additional HW: Buzzer, 7-seg LED, 74HC595, rotary encoder w/ button, 220 ohm 
 *                   resistors for 7-seg LED.
 * Additional files: Lab07.h
 *  Req'd libraries: WiFi, PubSubClient, ArduinoJson, Bounce2, Rotary
 *
 * This program implements a simple communication network consisting of up to 16
 * Huzzah32 Feather with an ESP32 processors. Nodes communicate via an MQTT broker
 * (typically a RPi running Mosquitto) to send heartbeat messages as well as "ringring"
 * messages. If a ringring message is received from a valid node (0-16), the
 * receiver plays a brief dual-tone sound using a local buzzer. Nodes must also
 * respond to "topics" messages by sending a "registeredFor" message listing all
 * the topics for which it is registered.
 ************************************************************************************/
// included configuration file and support libraries
#include <Esp.h>            // Esp32 support
#include <WiFi.h>           // wi-fi support
#include <PubSubClient.h>   // MQTT client (by Nick O'Leary?? vTBD version)
#include <ArduinoJson.h>    // for encoding/decoding MQTT payloads in JSON format
                            // by Benoit Blanchon (OLD version vTBD version)
#include <Bounce2.h>        // debounce pushbutton (by Thomas O Frederics vTBD version)
#include <ESP32Encoder.h>   // New libary (in arduino libraries) for rotary encoder
                            // Slightly simpler to implement
                            // https://github.com/madhephaestus/ESP32Encoder
#include "Lab07.h"          // included in this project

WiFiClient wfClient;              // create a wifi client
PubSubClient psClient(wfClient);  // create a pub-sub object (must be
                                  // associated with a wifi client)

// Global Variables

// Rotary encoder variables.
Bounce2::Button knob_button = Bounce2::Button();
ESP32Encoder rotaryEncoder;
unsigned short knob;

// character arrays for processing strings and json payloads
char json_Buffer[200];
char json_msgBuffer[200];
char sbuf1[80];          
char sbuf2[80];

unsigned long mscurrent = millis();
unsigned long mslastMQTT = mscurrent;
unsigned long mslastHeartbeat = mscurrent;

int datArray[16] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 246, 238, 62, 156, 122, 158, 142};
// datArray is helpful for abstracting away the seven-segment I/O
// index N displays number N seven-segment (in hex 0-F)

/* Prototype functions */
void call_node(String,String);
void ringtone();
void callerID(String);
void updateRotary();
void rotary_button();
void registeredFor(String);
void processMQTTMessage(char*, byte*, unsigned int);
void sendHeartbeatMessage(String);
void registerForTopics();
void connectWifi();
void setupMQTT();
void connectMQTT();

void setup() {
  Serial.begin(115200);
  while(!Serial) {
    // wait for serial connection
    delay(1);
  }
  Serial.println("Serial ready!");

  // Pin and other setup
  pinMode(pdButton, INPUT_PULLUP); // pin 0 corresponds to GPIO0, which corresponds to Pin 19
  // finish button setup

  // initialize buzzer
  pinMode(buzzerPin, OUTPUT); // buzzer ("ringring" message enunciator)

  // 74HC595 pins
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // Rotary Setup
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  rotaryEncoder.attachHalfQuad(dtPin, clkPin);
  rotaryEncoder.setCount(0);
  
  // setup connections
  connectWifi();
  setupMQTT();
  connectMQTT();

  // Flash the on-board LED five times to let user know
  // that the Huzzah32 board has been initialized and ready to go.
  pinMode(LED_BUILTIN, OUTPUT);
  for(int i=0; i<5; i++) {
    digitalWrite(LED_BUILTIN, 0);  // active low
    delay(200);
    digitalWrite(LED_BUILTIN, 1);
    delay(150);
  }
  knob_button.attach(pdButton, INPUT_PULLUP);
  knob_button.interval(DEBOUNCE_INTERVAL);
  knob_button.setPressedState(LOW);
  // registeredFor(myNodeID);
}

void loop() {
  mscurrent = millis();
  // This is largely a reactive program, and as such only uses
  // the main loop to maintain the MQTT broker connection and
  // regularly call the psClient.loop() code to check for new
  // messsages
  psClient.loop();
  updateRotary(); // update knob value
  rotary_button(); // checks if rotary button was pressed and updates accordingly

  // reconnect to MQTT server if connection lost
  if ( (mscurrent - mslastMQTT) > MQTT_CONNECT_CHECK) {
    mslastMQTT = mscurrent;
    if (!psClient.connected()) {
      Serial.println("MQTT not connected! Trying reconnect.");
      connectMQTT();
    }
  }
  // send heartbeat (HEARTBEAT_INTERVAL = 0 means no heartbeat)
  if( ((mscurrent - mslastHeartbeat) > HEARTBEAT_INTERVAL) &&
      HEARTBEAT_INTERVAL != 0 ) {
      mslastHeartbeat = mscurrent;
      sendHeartbeatMessage(myNodeID);
    }
}
/**********************************************************
 * Helper functions
 *********************************************************/
void call_node(String srcNode, String dstNode)
{
  StaticJsonDocument<200> root;

  // fill tree with message data
  root["srcNode"] = srcNode;
  root["dstNode"] = dstNode;

  // Send message
  serializeJson(root,json_msgBuffer);
  String msgTopic = "ece/" + (String)dstNode + "/ringring";
  const char *msg = msgTopic.c_str();  // put into char array form (zero copy!!)
  psClient.publish(msg, json_msgBuffer);
  Serial.print("Calling ");Serial.println(msgTopic);
  Serial.println(msg);
}
void ringtone() { // ringtone is a quick trill between a base note its major third (note interval)
  int base = 2600; // base frequency (ratios are easier to deal with than frequencies)
  int dur = 50;
  for(int j = 0;j<8;j++) {
    tone(buzzerPin,base, dur);
    tone(buzzerPin,base*5/4, dur); // 4:5 is the ratio of a note and its major third
  }
}
void callerID(String senderNode) { // senderNode = "NodeNN" of sender
    int sender = senderNode.substring(4,6).toInt();
    // sender = NN of sender
    rotaryEncoder.setCount(sender);
}
void updateRotary() {
  // update rotary encoder object
  knob = rotaryEncoder.getCount(); // update knob value
  // Serial.println(knob);
  // write knob value to seven segment display
  digitalWrite(latchPin,LOW);
  shiftOut(dataPin,clockPin,MSBFIRST,datArray[knob%16]);
  digitalWrite(latchPin,HIGH);
}
void rotary_button() {
  knob_button.update();
  String buffer = (String)(knob%16);
  if (knob%16 < 10) {
    buffer = '0' + buffer;
  }
  if (knob_button.pressed()) {
    call_node(myNodeID,"node" + buffer);
  }
}
void registeredFor(String srcNode) {
  StaticJsonDocument<200> root;

  root["NodeName"] = srcNode;
  JsonArray nested = root.createNestedArray("topics");
  nested.add("ece/node08/ringring");
  nested.add("ece/node08/topics");
  // use nested.add() to add any other topics that are registered for if implemented in the future

  serializeJsonPretty(root,Serial);
  serializeJson(root,json_msgBuffer);
  String msgTopic = "ece/" + (String)srcNode + "/registeredFor";
  const char *msg = msgTopic.c_str();  // put into char array form (zero copy!!)
  psClient.publish(msg, json_msgBuffer);
  Serial.println(msgTopic);
  Serial.println(msg);
}
void processMQTTMessage(char* topic, byte* json_payload, unsigned int length) {
  // This code is called whenever a message previously registered for is
  // RECEIVED from the broker. Incoming messages are selected by topic,
  // then the payload is parsed and appropriate action taken. (NB: If only
  // a single message type has been registered for, then there's no need to
  // select on a given message type. In practice this may be rare though...)
  //
  // For info on sending MQTT messages inside the callback handler, see
  // https://github.com/bblanchon/ArduinoJson/wiki/Memory-model.

  Serial.print("Topic => ");
  Serial.println(topic);// Parse for topics here
  sprintf(sbuf1,"ece/%s/ringring", myNodeID);
  sprintf(sbuf2,"ece/%s/topics",myNodeID);
  if(strcmp(topic, sbuf1) == 0) {
    // received "ledCommand" message, so parse payload into an object tree
    // example payload: {"senderID":"btnNode14","cmd":"on"}
    StaticJsonDocument<200> jsonDoc;
    auto error = deserializeJson(jsonDoc, (char*)json_payload);
    Serial.println("Parse message packet is ...");
    serializeJsonPretty(jsonDoc, Serial);
    if(!error) {
      // JSON name-value pairs have been parsed into the parse tree, so
      // extract values associated with the names "senderID" and "cmd"
      // and store values in string variables
      String ece = jsonDoc["ece"];
      String msg = jsonDoc["msg"];
      Serial.println(); Serial.print("msg = "); Serial.println(msg);
      ringtone();
      callerID(jsonDoc["srcNode"]);
    }
    else {
      // parse failed so print a console message and return to caller
      sprintf(sbuf1,"failed to parse JSON payload (topic: %s)\r\n", topic);
      Serial.print(sbuf1);
      return;
    }
  }
  else if (strcmp(topic,sbuf2) == 0) {
    registeredFor(myNodeID);
  }
  else {
      // topic was registered with broker, but no processing code in place... :(
      sprintf(sbuf1,"Topic: \"%s\" unhandled\r\n", topic);
      Serial.print(sbuf1);
  }
}
void sendHeartbeatMessage(String srcNode) {

  // message topic: "ece/node01/heartbeat"
  // payload: NodeName, NodeType
  StaticJsonDocument<200> root;

  // fill tree with message data
  root["NodeName"] = srcNode;
  root["NodeType"] = "ESP32";

  // Send message
  serializeJson(root,json_msgBuffer);
  String msgTopic = "ece/" + (String)srcNode + "/heartbeat";
  const char *msg = msgTopic.c_str();  // put into char array form (zero copy!!)
  psClient.publish(msg, json_msgBuffer);
  // heartbeat message here
  Serial.println("Sent heartbeat.");
}
void registerForTopics() { // register with MQTT broker for topics of interest to this node
  // register for ece/nodeNN/ringring
  Serial.print("Registering for topics...");
  sprintf(sbuf1,"ece/%s/ringring", myNodeID);
  psClient.subscribe(sbuf1);

  // register for ece/nodeNN/topics
  sprintf(sbuf1,"ece/%s/topics", myNodeID);
  psClient.subscribe(sbuf1);

  Serial.println("Registration done.");
}

void connectWifi() {
  // attempt to connect to the WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(" network");
  delay(10);
  #ifdef LIPSCOMB
    WiFi.begin(ssid);            // Lipscomb WiFi does NOT require a password
  #elif defined(ETHERNET)
    WiFi.begin(ssid);
  #else
    WiFi.begin(ssid, password);  // For WiFi networks that DO require a password
  #endif

  // advance a "dot-dot-dot" indicator until connected to WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // report to console that WiFi is connected and print IP address
  Serial.print("MAC address = "); Serial.print(WiFi.macAddress());
  Serial.print(", connected as "); Serial.print(WiFi.localIP());
  Serial.println(".");
}
void setupMQTT () {
  // specify MQTT broker's domain name (or IP address) and port number
  Serial.print("Initalizing MQTT object with broker="); Serial.print(mqttBroker);
  Serial.print(" and port="); Serial.print(mqttPort);
  Serial.print("..");
  psClient.setServer(mqttBroker, mqttPort);

  // Specify callback function to process messages from the broker.
  psClient.setCallback(processMQTTMessage);
  Serial.println(".done");
}
void connectMQTT()
{
  // Ping the server before trying to reconnect
  int WiFistatus = WiFi.status();
  if (WiFistatus != WL_CONNECTED) {
    Serial.print("WiFi check failed!  Debug the Wireless connection...");
  } else {
    //Serial.println("passed!");
    // Try to connect to the MQTT broker (let loop() take care of retries)
    Serial.print("Connecting to MQTT with nodeName=");
    Serial.print(nodeName);
    Serial.print(" ... ");
    if (psClient.connect(nodeName)) {
      Serial.println("connected.");
      // clientID (<nodename>) MUST BE UNIQUE for all connected clients
      // can also include username, password if broker requires it
      // (e.g. psClient.connect(clientID, username, password)
      // once connected, register for topics of interest
      registerForTopics();
      sprintf(sbuf1,"MQTT initialization complete\r\nReady!\r\n\r\n");
      Serial.print(sbuf1);
    }
    else {
      // reconnect failed so print a console message, wait, and try again
      Serial.println(" failed!");
      Serial.print("MQTT client state=");
      Serial.println(psClient.state());
      Serial.print("(Is processor whitelisted?  ");
      Serial.print("MAC=");
      Serial.print(WiFi.macAddress());
      Serial.println(")");
    }
  }
}
