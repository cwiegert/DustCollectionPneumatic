/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - *************************************************************/


/**************************************************************************************
 *  DustCollection_OTA 
 * 
 *  Author:   Cory D. Wiegert
 *  Date:     April, 7, 2024
 *  License:  all code open source and licensed under MIT license 
 *  
 * 
 * Background:
 *              New version of dust collection automation.   In previous versions, found here
 *              https://github.com/cwiegert/DustCollectionEEPROM all the gates were controled via
 *              software running on an Arduino Mega, with the configuration all managed in a single place.
 *              This version, is an IOT model, where all the code for each gate is managed on a NodeMCU
 *              and synched to a NODE-RED instance running on a raspbery PI.   The communications are 
 *              over the MQTT connecting to a mosquitto broker running on the same raspberry pi.
 *              The gates are managed with air actuators, and a single HIGH/LOW to the 12v relay to trigger the
 *              air solenoid.
 * 
 *              All activation of the NodeMCU's are done through Blink OTA, and upgrades destributed through the BLYNK update 
 *              package and the BLYNK_FIRMWARE_VERSION attribute below.   To updrage the NodeMCU's, simply increment the version
 *              and create the publication package on the BLYNK developer screen.   
 * Libraries:
 *          - Blynkedgenet.h                      incstalled via the Arduino Library Manager
 *          - BlynkSimpleShieldESP3266.h          installed via the Blynk library in Arduino Library Manager
 *          - ArduinoJson.h                       installed via the Arduino Library Manager
 *          - ArduinoMqttClient.h                 installed via the arduino Library Manager
 *          - DustCollectorGlobals.h              manually built and in the library directory of the git repo
 *        
 * version:
 *     4/07/2024  CDW   1.1.0 -- plugged into NODE-RED, skipping the outlet with a 0 as the outlet pin
 *     4/27/2024  CDW   1.3.0 -- implemented V0 to erase EEPROM, did null checks on BLYNK UI, set them to constants when null
 *     4/27/2024  CDW   1.3.3 -- updated the gate pin to hard default to GPIO 5 or pin D1 on teh WEMOS D1 
 *     5/6/2024   CDW   1.3.4 -- publish to GitHub under new repository
 *     5/17/2024  CDW   1.3.5 -- added the chip id as part of the blast gate structure.   Will use this in NodeRed for maintaining gate ID
 *     5/26/2024  CDW   1.3.6 -- changed the gate map for loop, instead of temp+1 < string, it's just temp < strlng
 *     5/26/2024  CDW   1.4.0 -- MAJOR change to the MqttClient.cpp library from the Arduino client library.   Changed the TX_PAYLOAD_BUFFER_SIZE 512 from 256
 *                               The Mqtt JSON messages were being truncated at 256 (default for the payload buffer).   Will need to package this library with the git, or 
 *                               make sure to call out in the readme.   Have highlighted in the CPP code
 * 
*********************************************************************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
#define BLYNK_TEMPLATE_ID "TMPL24bXLC68L"
#define BLYNK_TEMPLATE_NAME "Blynk Provision"

#define BLYNK_FIRMWARE_VERSION "1.4.0"  

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

#include "library/BlynkEdgent.h"
#include "library/DustCollecterGlobals.h"


WidgetTerminal  statusUpdate(V17);        // terminal on the mobile app to update status

BLYNK_WRITE (V0)  // ERASE EEPROM clearing it from old configuration   1.3.0
  {
    if (param.asInt())
      {
        saveConfigEEPROM (E_ERASE);
      }
  }

BLYNK_WRITE(V2)  // gate control pin from the blynk Config Screen
{
  blastGate.gatePin = param.asInt();
  dirtyConfigBit = true;
}

BLYNK_WRITE(V3)  // collector spin down delay, from Blynk config screen
{
  blastGate.collectorDelay = param.asInt();
  dirtyConfigBit = true;
}

BLYNK_WRITE(V4)  // outlet pin, if button is not set to digital, from Blynk config screen
{
  blastGate.outletPin = param.asInt();
  dirtyConfigBit = true;
}

BLYNK_WRITE(V6)  // gate name from the Blynk Config Screen
{
  strcpy(blastGate.gateName, param.asStr());
  dirtyConfigBit = true;
}

BLYNK_WRITE(V5)  // gate id from then Blynk Config Screen
{
  holderGateId = blastGate.gateID;
  blastGate.gateID = param.asInt();
  dirtyConfigBit = true;
  changeGate = true;
}

BLYNK_WRITE(V9)   //  multi-selector to either open or close gate
  {
    if (param.asInt() == 0)
      openGate();
    else if (param.asInt() == 2)
      closeGate();
    Blynk.virtualWrite(V9, 1);
  }

BLYNK_WRITE(V11)  // tool name from the Blynk Config Screen
{
  strcpy(blastGate.toolName, param.asStr());
  dirtyConfigBit = true;
}

BLYNK_WRITE(V12)  // node-red prefex - not implemented yet
  {
  strcpy(blastGate.nodeRedPrefix, param.asStr());
  dirtyConfigBit = true;
  }

BLYNK_WRITE(V13)  // gate map from the Blynk config Screen
{
  strcpy(blastGate.gateMap, param.asStr());
  //Serial.println(blastGate.gateMap);
  dirtyConfigBit = true;
}

BLYNK_WRITE(V14)  // Save to EEPROM button
{ 
  if (param.asInt() == 1)
    {    
      DynamicJsonDocument jSonDoc(JSON_SIZE);
      String mapToJson;
      statusUpdate.clear();
      if (dirtyConfigBit == true )
        {  
          if (changeGate == false)
            {
              saveConfigEEPROM(E_WRITE);
              dirtyConfigBit = false;
            }
          else
            {
              if (blastGate.gateID != holderGateId)     // gate ID has changed, and need to remove the old gate from the parameter array
                {       
                  jSonDoc["gate Id"] = blastGate.gateID;
                  jSonDoc["holder"] = holderGateId;
                  serializeJsonPretty (jSonDoc, mapToJson);
                  
                  if (!mqttClient.connected())
                    mqttClient.connect(broker, port);
                  mqttClient.beginMessage (REGISTER);       
                  mqttClient.println (mapToJson);
                  mqttClient.endMessage(); 
                } 
            }
        }
    }  
}

BLYNK_WRITE(V15)  // Refresh from EEPROM
  {
  if (param.asInt() == 1 )
    if (getEEPROMConfig(15))
      setBlynkConfigUI();
  }

BLYNK_WRITE(V16)  // red button on screen to reset the board
  {
    if (param.asInt())
      ESP.restart();
  }

BLYNK_WRITE (V17)
  {
    // if I want input from the Blynk screen I would write it here
  }

/************************************************************
 * void runOutletCheck()
 *    check to see if the pin being monitored for for a voltage changed
 *    has been bit flipped to HIGH.   If so, call the gate map map function
 *    set the JSON object with the gate map in each of the elements, which is 
 *    then broadcast to all the gates to set their appropriate state.
 * 
**********************************************************************/
void runOutletCheck() 
  {

    String mapToJson;
    DynamicJsonDocument jSonDoc(JSON_SIZE);

    if (blastGate.outletPin > 0)                  // skip the outlets that aren't connected to an outlet
      {
        jSonDoc["sent Gate"] = blastGate.gateID;
        jSonDoc["outlet"] = blastGate.outLetDigital;
        jSonDoc["spinDown"] = blastGate.collectorDelay;
        jSonDoc["machine"] = blastGate.toolName;

        JsonObject gateMap = jSonDoc.createNestedObject("gateMap");
        int temp = 0;
        do
        {
          gateMap[String(temp)] = blastGate.gateMap[temp] - '0';          // create the nested JSON object with the gate states set 1 per element ()  1.3.6
          temp++;
        } while (blastGate.gateMap[temp] != '\0' && temp < 32);
           
        serializeJsonPretty(jSonDoc, mapToJson);
        //serializeJsonPretty(jSonDoc, Serial);
        if (!mqttClient.connected())    // check for a disconnect and reconnect to make sure messages go through
          mqttClient.connect(broker, port);
     // Serial.println("toolIsOn value ==> " + String (toolIsOn) + " pn  ==> " +  String(blastGate.outletPin )+ "==> "+ String(digitalRead (blastGate.outletPin)));
        if (toolIsOn == false)
          {
            if (digitalRead (blastGate.outletPin) == LOW)      // current flowing through the INPUT_PULLUP, going to ground pulls to LOW
              {
                digitalWrite(blastGate.gatePin, HIGH);        // send send 5V to the gate pin, signal relay to open gate
                digitalWrite(1, HIGH);
               
                mqttClient.beginMessage(TOOL_ON);
                mqttClient.println(mapToJson);
                mqttClient.endMessage();                      // Call Node-Red and turn the collector on, Node-red will run gate maps as a TOPIC back to all gates
                toolIsOn = true;
              }
          }
        else 
          {
            if (digitalRead(blastGate.outletPin) == HIGH && toolIsOn == true)       // tool has been running and is now turned off INPUT_PULLUP hold HIGH when no outside current
              {
                //Serial.println("I tiggered the HIGH condition on readOutlets");
                     // set the 
                mqttClient.beginMessage(TOOL_OFF);            // Turn tool off if elapsed time > delay spin down
                mqttClient.println(mapToJson);
                mqttClient.endMessage();
              }
            else if (toolIsOn == true)
              {
                if (!mqttClient.connected())    // check for a disconnect and reconnect to make sure messages go through
                  mqttClient.connect(broker, port);
          
                mqttClient.beginMessage (TOOL_RESET);         // Need to reset the elapsed time timer, and don't want to run through the startup in NodeRed
                mqttClient.println(mapToJson);
                mqttClient.endMessage (); 
              }
          }
      }
  }

/***********************************************************
 *  void nodRedListenCallback (int _size)
 *      callback function for all the node red topic handling
 * ************************************************************/
void nodeRedListenCallback ( int _size)
  {
    int workingGate;
    int __gateState;
    char  payload[_size + 1];
    String topic;
    //int error = 0;
    
    //DynamicJsonDocument jSonDoc(JSON_SIZE);
   DynamicJsonDocument jSonDoc(512);
    topic = mqttClient.messageTopic();
      if (topic != GATE_ID_SEND && topic != CLOSE_ME)    // check the topic to make sure there is a json document
    {     
      mqttClient.readBytes (payload, _size);           // read the incoming message
      payload[_size+1] = '\0'; 
      DeserializationError error=deserializeJson (jSonDoc, payload);       //deserialize the incoming payload to a json document
      if (error) 
        {
          Serial.print(F("JSON parsing error: "));
          Serial.println(error.c_str());
          return;
        } 
    }

     if (topic == GATE_ID_SEND)           // if Node-red has asked this gate to publish the ID
      {
        String mapToJson;
        jSonDoc["gate chip id"] = blastGate.wemos_id;     // 1.3.5 added the wemos_id to use in the check for unique and clearing the array
        jSonDoc["gate_ID"] = blastGate.gateID;
        jSonDoc["gate Name"] = blastGate.gateName;
        serializeJsonPretty (jSonDoc, mapToJson);
    //    serializeJsonPretty (jSonDoc, Serial);
        mqttClient.beginMessage (PUBLISH_GATE_ID);
        mqttClient.println (mapToJson);
        mqttClient.endMessage();
      }
    else if (topic == RUN_GATES)          //  get the value from the gateMap, open or close gaet base on gateMap 
      {
          mqttClient.readBytes (payload, _size);
        
          payload[_size] = '\0';
          DynamicJsonDocument  mqttDoc (JSON_SIZE);
          
          deserializeJson (mqttDoc, payload);
        //   serializeJsonPretty (mqttDoc, Serial);
          workingGate = mqttDoc["sent Gate"];
          String bgString = String(workingGate);
          String _gate = String(blastGate.gateID);
          JsonObject gateMapObject = mqttDoc["gateMap"].as<JsonObject>();;
          
          if (workingGate != blastGate.gateID)    // so long as this code is not running on the instructing gate, execute gate action
            {
              __gateState = gateMapObject[_gate];
              if (__gateState == LOW)
                closeGate();    
              else 
                openGate();           
            }
      }
    else if (topic == CLEAR_NODES)       // Not Implemented yet
      {

      }
    else if (topic == NOT_UNIQUE)
      {
        blastGate.gateID = jSonDoc["gate Id"];
        if (saveConfigEEPROM(E_WRITE))
          {
            Blynk.virtualWrite (V5, blastGate.gateID);
            Blynk.syncVirtual (V5);
            holderGateId = -1;
            dirtyConfigBit = false;
            statusUpdate.clear();
            statusUpdate.print ("changed the gate Id to ==> " );
            statusUpdate.println (blastGate.gateID);
            statusUpdate.println (F("be sure to check the gate map for the appropriate mapping to the gate"));

          }
      }
    else if (topic == IS_UNIQUE)         //  implement the save to saveConfigEERPOM here, only if a unique value comes back from node-red
      {
        if (saveConfigEEPROM(E_WRITE))
          {
            holderGateId = -1;
            dirtyConfigBit = false;
          }
        
      }
 
    else if (topic == CONFIRM_OFF)         // resets the toolIsOn variable
      {
          toolIsOn = false;
          mqttClient.beginMessage(CLOSE_ALL);    // want to close all the gates, so the relays are not always energized
          mqttClient.endMessage ();

      }
    else if (topic == CONFIRM_ON)
      {
        mqttClient.readBytes (payload, _size);
        
          payload[_size] = '\0';
          DynamicJsonDocument  mqttDoc (JSON_SIZE);
          
          deserializeJson (mqttDoc, payload);
           //serializeJsonPretty (mqttDoc, Serial);
          workingGate = mqttDoc["sent Gate"];
   // Serial.println ("blastGate.gateId ==> " + String (blastGate.gateID) + " | workingGate ==> " + String (workingGate));
          if (workingGate == blastGate.gateID)    // only turn the tool on if this gate turned on the collector
            toolIsOn = true;
      }
    else if (topic == CLOSE_ME)
        {
           closeGate();                           // all gates will close, including the "was on " tool gate
        }
    else if (topic == TOOL_ON)          // Not Implemented yet
      {

      }       
    else if (topic = NEW_GATE_ID)       // Not implemented yet
          {
            // or do i put the logic for the calling of node-red here, and let the IS-UNIQUE handle the response from mqtt
          }     
  }

void setMQttSubscriptions()                 // function to define which topics are subscribed, and setting the callback
  {
      mqttClient.subscribe (RUN_GATES); 
    //mqttClient.subscribe (CLEAR_NODES);
    mqttClient.subscribe (GATE_ID_SEND);
    //mqttClient.subscribe (PUBLISH_GATE_ID);
    mqttClient.subscribe (NOT_UNIQUE );
    mqttClient.subscribe (IS_UNIQUE );
    mqttClient.subscribe(CONFIRM_OFF);
    mqttClient.subscribe(CONFIRM_ON);
    mqttClient.subscribe(CLOSE_ME);
   // mqttClient.subscribe (NEW_GATE_ID);
    mqttClient.onMessage (nodeRedListenCallback);
   }

/*******************************************************************
 * bool saveConfigEEPROM()
 *    used from teh blynkUI to save any changes in the gate configuration 
 *    screen.   Will only write if the dirty_bit has been set to true
 *    returns false if not able to save to eeprom
**********************************************************************/
bool saveConfigEEPROM( int _RW)             // checks for WRITE or ERASE and saves blastGate structure to EEPROM or erases EEPROM
  {

    int remaining;
    remaining = EEPROM.length() - sizeof(configStore) - sizeof(blastGate);
    if (remaining > 0 && _RW == E_WRITE)
      EEPROM.put (configAddy, blastGate);
    else if (_RW == E_WRITE)
    {
      Serial.println(F(" the blast gate structure will not fit in EEPROM"));
      return false;
    }
  //Serial.println("dirtyConfig ==> " + String(dirtyConfigBit) + " | remaining ==> " + String (remaining) + " | __RW ==> " + String(_RW));
    if (dirtyConfigBit && remaining > 0 && _RW == E_WRITE) 
      {
        
        EEPROM.put(configAddy, blastGate);
        EEPROM.commit();
        Serial.println(F("WROTE blast gate structure to EEPROM"));
        dirtyConfigBit = false;
        statusUpdate.println (F("saved new settings to EEPROM"));
        return true;
      } 
    else if (_RW == E_ERASE)
      {   
        int  _address = configAddy;
        int __step = 0;
        statusUpdate.clear();
        statusUpdate.print("Clearing");
        while (__step < sizeof (blastGate))
          {
            EEPROM.write(_address, 0XFF);
            _address++;
            __step++;
            statusUpdate.print(".");           // 1.3.1
          }
        //statusUpdate.clear();
        statusUpdate.println(F("\nCleared EEPROM"));
        return true;
      }
    else
      return false;
  }

bool getEEPROMConfig( int _where)            // populates the blast gate structure from EEPROM
  {
    byte temp;
    temp = EEPROM.read (configAddy);
    EEPROM.get(configAddy, blastGate);
    if (blastGate.gateID != -1 )
      return true;
    else if (_where != 15)
      {
        Serial.println(F("did not read EEPROM"));
        return false;
      }
    else
      return true;
    
  }

/**********************************************
void setPins ()
  used to set the hardware pins to the appropriate mode.   For the NODE MCU 8266
  DO NOT USE pins 6 - 11, as htey are used by the flash memory.   Since this device
  is running only a single blast gate, there shouldn't be an issue
******************************************************/

/************************************************
 * void setPins()
 *    setting the reading and the writing pins.   
 *    The outlet pin is an INPUT_PULLUP as we will be monitoring for 
 *    a change from LOW to HIGH, and the gate pin is a straight forward
 *    digital output to trigger the air solenoid
****************************************************/
void setPins()            
  {
  
    if (blastGate.gatePin < 1)
      blastGate.gatePin = GATE_PIN;
    pinMode(blastGate.gatePin, OUTPUT);
    if (blastGate.outletPin > 5 && blastGate.outletPin < 11)                    // on a WEMOS, can not use GPIO 6-11, they are used by the board directly
      blastGate.outletPin = OUTLET_PIN;  
    pinMode(blastGate.outletPin, INPUT_PULLUP);
    digitalWrite (blastGate.gatePin, LOW);          // closing the gate on startup
    pinMode(EXTERNAL_LED, OUTPUT);
    digitalWrite(EXTERNAL_LED, LOW);
  }

void setBlynkConfigUI() 
  {
    if (int(blastGate.gateName[0]) == 255)
      strcpy(blastGate.gateName, NO_VALUE);
    Blynk.virtualWrite(V6, blastGate.gateName);
    Blynk.virtualWrite(V5, blastGate.gateID);
    if (blastGate.gatePin < 1)                  //1.3.0
      {
        blastGate.gatePin = GATE_PIN;
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(V2, blastGate.gatePin);
    if (int(blastGate.toolName[0]) == 255)      //1.3.0
      {
        strcpy(blastGate.toolName, NO_VALUE);
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(V11, blastGate.toolName);
    if (int(blastGate.gateMap[0]) == 255)       //1.3.0
      {
        strcpy(blastGate.gateMap, "00000000000");
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(V13, blastGate.gateMap);
    if ( blastGate.outletPin < 1)               // 1.3.0
      {
        blastGate.outletPin = OUTLET_PIN;
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(V4, blastGate.outletPin);
    Blynk.virtualWrite(V3, blastGate.collectorDelay);
    if (int(blastGate.nodeRedPrefix[0]) == 255)       // 1.3.0
      {
        strcpy(blastGate.nodeRedPrefix, NO_VALUE);
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(V12, blastGate.nodeRedPrefix);
    Blynk.syncAll();
  }

void openGate() {
  digitalWrite(blastGate.gatePin, HIGH);
}

void closeGate() {
  digitalWrite(blastGate.gatePin, LOW);
}

/*******************************************************************************************************
 * *****************************************************************************************************
********************************************************************************************************/
void setup() 
  {
    int count = 0;
    Serial.begin(115200);
    delay(100);
    EEPROM.begin (2048);
   //DynamicJsonDocument jSonDoc(JSON_SIZE);
    DynamicJsonDocument jSonDoc(512);
    BlynkEdgent.begin();
    while (BlynkState::get() != MODE_RUNNING && count < 300)     //  give time for the Blynk.edgent() sub functions to execute.   
      {
        count++;
      //  Serial.println("Runnint BlynkEdgent.run() to process Edgent connection   " + String(BlynkState::get()) + "  running mode ==> " + String(MODE_RUNNING));
        BlynkEdgent.run();
      }
//   removed the wifi.begin line here, it was error out, and unneccessary   1.3.0

    if (!mqttClient.connect(broker, port)) 
      {
        Serial.print(F("MQTT connection failed! Error code = "));
        Serial.println(mqttClient.connectError());

        while (1);
      }

    if (mqttClient.connected ())
      {
        Serial.print(F("You're connected to the MQTT broker at IP Address and port ==> "));
        Serial.println(String(broker) + " : " + String(port));
        
        setMQttSubscriptions ();
      }

    if (BlynkState::get() == MODE_RUNNING) 
      {
        configAddy = EEPROM_CONFIG_START + sizeof(configStore) + 8;
        if (getEEPROMConfig(0)) 
          {
            setBlynkConfigUI();                   // Set the Blynk config screen
            setPins();                            // Set the appropriate digital pins
            blastGate.wemos_id = ESP.getChipId();
            String mapToJson;                           // Variable to hold the Json structure
            jSonDoc["gate chip id"] = blastGate.wemos_id;
            jSonDoc["gate_ID"] = blastGate.gateID;
            jSonDoc["gate Name"] = blastGate.gateName;
            serializeJsonPretty (jSonDoc, mapToJson);
            //serializeJsonPretty (jSonDoc, Serial);
            if (!mqttClient.connected())
              {
                Serial.println ("have to reconnect to mqqt");
                mqttClient.connect(broker, port);
              }
              
            if (mqttClient.connected())
            {
              mqttClient.beginMessage (PUBLISH_GATE_ID);     // post the gateID to the NodeRed broker
              mqttClient.println (mapToJson);
              mqttClient.endMessage();
            }  
          }
        else
          {
            blastGate.gatePin = GATE_PIN;
            blastGate.outletPin = OUTLET_PIN;
          }
      }
    statusUpdate.clear();
    statusUpdate.println(F("ready"));
    Serial.println(F("ready!!!!!"));
    Serial.println("wemos id ==> " + String (blastGate.wemos_id));
  }

void loop() 
  {
  
    // to avoid having delays in loop, we'll use the strategy from BlinkWithoutDelay
    // see: File -> Examples -> 02.Digital -> BlinkWithoutDelay for more info
    unsigned long currentMillis = millis();
   
    if (currentMillis - previousMillis >= interval) 
      {
          // save the last time you blinked the LED
          previousMillis = currentMillis;
          BlynkEdgent.run();
          if (!mqttClient.connected())
            mqttClient.connect(broker, port);
          mqttClient.poll();
          runOutletCheck(); 
        }
  }

/*********************************************************************************************************
 * *******************************************************************************************************
**********************************************************************************************************/


