// This file accompanies the Lab07.cpp program
// A simple ESP32 messaging system using MQTT

// system defines
  #define DEBOUNCE_INTERVAL       5 // 5mS works well for circuit-mount PBs
  #define HEARTBEAT_INTERVAL  10000 // heartbeat interval (0=no heartbeat)
  #define MQTT_CONNECT_CHECK  30000 // Connection check
  #define PB_UPDATE_TIME          8 // number of mS between button status checks
  #define SWITCH_OPEN             0
  #define SWITCH_CLOSED           1

  #define MIN_NODE_NUMBER         0
  #define MAX_NODE_NUMBER        15
  #define BLANK_7SEG_NUMBER      16

// Define PINs
  // buzzer pins
    #define buzzerPin 25   // buzzer for individual sender tones

  // shift register pins
    #define dataPin    4   // DS pin of 74HC595
    #define latchPin   5   // ST_CP pin of 74HC595 (RCLK)
    #define clockPin  18   // SH_CP pin of 74HC595

  // rotary encoder pins
    #define pdButton  16   // Encoders PB button
    #define dtPin     17   // the dt (encoder "B") pin
    #define clkPin    21   // the clk (encoder "A") pin

// led activation values (active high)
  #define LED_ON   1
  #define LED_OFF  0

// Wifi credentials
#define LIPSCOMB
#ifdef LIPSCOMB
  const char* ssid = "LipscombGuest";  // no PW needed for Lipscomb guest wifi
#else
  const char* ssid = "<home-internet>";  // ssid of brokerX's network
  const char* password = "<home-internet-password"; // password for brokerX's network
#endif  	

// MQTT broker credentials
#ifdef LIPSCOMB
  const char* mqttBroker = "10.51.97.101";  // ECE mosquitto server
  //const char* mqttBroker = "10.149.97.101"; // Old hardwared address
#else
  const char* mqttBroker = "<home-internet-IPaddress>";   // Home mosquitto server
#endif
const int mqttPort = 1883;

// get ID of this node's ESP32 (undocumented: last three bytes of MAC address)
// this can be useful when making unique client ID values
//const uint32_t ID = ESP.getChipId();

// ECE node name and type
#define myNodeID "node08"
const char* nodeName = myNodeID;
const char* nodeType = "ESP32";

// message topics to register for
// Using string literal tricks.  "txt1" "-txt2" = "txt1-txt2"
const char* topics[] = {"ece/" myNodeID "/ringring",
                        "ece/" myNodeID "/topics"  };
