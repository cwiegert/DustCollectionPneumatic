#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

//define the topics used in the various mqtt messages
#define RUN_GATES          "Dust/Run Gates"
#define CLEAR_NODES        "Dust/Clear All Gates"
#define PUBLISH_GATE_ID    "Dust/Gate ID"
#define GATE_ID_SEND       "Dust/Send Gate ID"
#define TOOL_ON            "Dust/Tool On"
#define TOOL_OFF           "Dust/Tool Off"
#define NOT_UNIQUE         "Dust/Not Unique"
#define IS_UNIQUE          "Dust/Is Unique"
#define CLEAR_GATE         "Dust/Clear Gate"
#define COLLECTOR_ON       "Dust/Collector On"
#define COLLECTOR_OFF      "Dust/Collector Off"
#define REGISTER           "Dust/Register Gate"
#define NEW_GATE_ID        "Dust/New Gate Id"
#define CLOSE_ALL          "Dust/Close All Gates"
#define CLOSE_ME           "Dust/Close Me"
#define CONFIRM_OFF        "Dust/OFF"
#define CONFIRM_ON         "Dust/ToolIsOn"
#define TOOL_RESET         "Dust/Reset Tool Timer"

#define EXTERNAL_LED    15     // using the GPIO pin # instead of the header number (D)
#define INTERNAL_LED    2     // using the GPIO pin # instead of the header number (D4)

#define OUTLET_PIN      12    // defaulting the outlet monitoring pin   
#define GATE_PIN         5    // for a WEMOS with a relay shield, this needs to stay set to 5 or the relay will not trigger   1.3.3
#define NO_VALUE        "Undefined"
#define E_ERASE           1
#define E_WRITE         0
 


/***********************************************
   *   Structure for the blast gate definition.  each instance
   *   variable is read and filled from the config file
   *  5/26/2024 -- CDW   1.3.7      Modified gateMap to not have a default length or value
   ***********************************************/
      struct    gateDefinition {
         uint32_t    wemos_id = 0;                          // unique ID generated off the chip of the wemos
         int         gateID = -1;
         char        gateName[32] = {"Need a Name"};        // name of gate
         bool        openClose;                             // is the gate open or closed? 
         char        gateMap [32] = {"000000000000"};       // string containing instructions on which gates to open and close  0 based array, Gate numbering typically starts with 1
         int         collectorDelay = 0;                    //  number of seconds to wait until turning off collector
         int         gatePin = GATE_PIN;                    // pint on the ESP8266 to open/close the gate
         char        toolName[32] = {"Give me a Name"};     // if the gate is the primary gate for the tool, put the tool name here
         int         outletPin = OUTLET_PIN;                // digital pin to monitor the outlet, default to 0, which is ignored in monitoring
         char        nodeRedPrefix[16] = {"Dust/NODE?"};    // key for the topic to be sent to Node-Red  (Not implemented yet)
         bool        outLetDigital;                         // is the outlet monitor hard wired to a digital pin, or code managed through virtual pin
         bool        gateDigital;                           // gate switch controled through virtual pin logic, or hard wired to digital pin
        }   blastGate;

int storeVirtualPin;
//const int INTERNAL_LED = LED_BUILTIN;         // Internal LED pin

int brightness = 0;
bool increasingBrightness = true;


WiFiClient    wifiClient;
MqttClient    mqttClient(wifiClient);

//const char broker[] = "192.168.0.73";         // Server address for MQtt broker  currently, the annapurnaNas
const char     broker[] = "192.168.0.91";       // Server address for MQtt broker  currently, the raspberry pi in the dining room
int            port     = 1883;                 // Port for the mosquito MQTT broker 

const long     interval = 100;                 // delay for sending messages 0.1 seconds
unsigned long  previousMillis = 0;

const size_t   JSON_SIZE = 512;
int            count = 0;
int            holderGateId;
bool           changeGate = false;
bool           toolIsOn =  false;            // flag to know when the dust collector is turned on

bool           isUnique = true;              // used to assess if the gateID's are unique on the system  

bool           dirtyConfigBit = false;       // write dirty bit for the Blynk controls.  Assesses when to allow save to EEPROM
int            configAddy;                         // EEPROM addy to start the Blynk Config screen write    
