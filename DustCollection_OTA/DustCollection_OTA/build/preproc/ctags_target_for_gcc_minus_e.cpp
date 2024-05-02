# 1 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
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
 *     4/27/2024  CDW   1.3.3 -- updated the gate pin to hard default to GPIO 5 or pin D1 on teh WEMOS D1 mini
 * 
*********************************************************************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */






//#define BLYNK_DEBUG



// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD

//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

# 75 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 2
# 76 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 2


WidgetTerminal statusUpdate(17); // terminal on the mobile app to update status

void BlynkWidgetWrite0 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // ERASE EEPROM clearing it from old configuration   1.3.0
  {
    if (param.asInt())
      {
        saveConfigEEPROM (1);
      }
  }

void BlynkWidgetWrite2 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // gate control pin from the blynk Config Screen
{
  blastGate.gatePin = param.asInt();
  dirtyConfigBit = true;
}

void BlynkWidgetWrite3 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // collector spin down delay, from Blynk config screen
{
  blastGate.collectorDelay = param.asInt();
  dirtyConfigBit = true;
}

void BlynkWidgetWrite4 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // outlet pin, if button is not set to digital, from Blynk config screen
{
  blastGate.outletPin = param.asInt();
  dirtyConfigBit = true;
}

void BlynkWidgetWrite6 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // gate name from the Blynk Config Screen
{
  strcpy(blastGate.gateName, param.asStr());
  dirtyConfigBit = true;
}

void BlynkWidgetWrite5 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // gate id from then Blynk Config Screen
{
  holderGateId = blastGate.gateID;
  blastGate.gateID = param.asInt();
  dirtyConfigBit = true;
  changeGate = true;
}

void BlynkWidgetWrite9 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) //  multi-selector to either open or close gate
  {
    if (param.asInt() == 0)
      openGate();
    else if (param.asInt() == 2)
      closeGate();
    Blynk.virtualWrite(9, 1);
  }

void BlynkWidgetWrite11 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // tool name from the Blynk Config Screen
{
  strcpy(blastGate.toolName, param.asStr());
  dirtyConfigBit = true;
}

void BlynkWidgetWrite12 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // node-red prefex - not implemented yet
  {
  strcpy(blastGate.nodeRedPrefix, param.asStr());
  dirtyConfigBit = true;
  }

void BlynkWidgetWrite13 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // gate map from the Blynk config Screen
{
  strcpy(blastGate.gateMap, param.asStr());
  //Serial.println(blastGate.gateMap);
  dirtyConfigBit = true;
}

void BlynkWidgetWrite14 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // Save to EEPROM button
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
              saveConfigEEPROM(0);
              dirtyConfigBit = false;
            }
          else
            {
              if (blastGate.gateID != holderGateId) // gate ID has changed, and need to remove the old gate from the parameter array
                {
                  jSonDoc["gate Id"] = blastGate.gateID;
                  jSonDoc["holder"] = holderGateId;
                  serializeJsonPretty (jSonDoc, mapToJson);

                  if (!mqttClient.connected())
                    mqttClient.connect(broker, port);
                  mqttClient.beginMessage ("Dust/Register Gate");
                  mqttClient.println (mapToJson);
                  mqttClient.endMessage();
                }
            }
        }
    }
}

void BlynkWidgetWrite15 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // Refresh from EEPROM
  {
  if (param.asInt() == 1 )
    if (getEEPROMConfig(15))
      setBlynkConfigUI();
  }

void BlynkWidgetWrite16 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param) // red button on screen to reset the board
  {
    if (param.asInt())
      ESP.restart();
  }

void BlynkWidgetWrite17 (BlynkReq __attribute__ ((__unused__)) &request, const BlynkParam __attribute__ ((__unused__)) &param)
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

    if (blastGate.outletPin > 0) // skip the outlets that aren't connected to an outlet
      {
        jSonDoc["sent Gate"] = blastGate.gateID;
        jSonDoc["outlet"] = blastGate.outLetDigital;
        jSonDoc["spinDown"] = blastGate.collectorDelay;
        jSonDoc["machine"] = blastGate.toolName;

        JsonObject gateMap = jSonDoc.createNestedObject("gateMap");
        for (int temp = 0; temp +1 < strlen(blastGate.gateMap); temp++) // create the nested JSON object with the gate states set 1 per element
            {
              gateMap[String(temp)] = blastGate.gateMap[temp] - '0';
            }

        serializeJsonPretty(jSonDoc, mapToJson);
        //serializeJsonPretty(jSonDoc, Serial);
        if (!mqttClient.connected()) // check for a disconnect and reconnect to make sure messages go through
          mqttClient.connect(broker, port);
     // Serial.println("toolIsOn value ==> " + String (toolIsOn) + " pn  ==> " +  String(blastGate.outletPin )+ "==> "+ String(digitalRead (blastGate.outletPin)));
        if (toolIsOn == false)
          {
            if (digitalRead (blastGate.outletPin) == 0x0) // current flowing through the INPUT_PULLUP, going to ground pulls to LOW
              {
                digitalWrite(blastGate.gatePin, 0x1); // send send 5V to the gate pin, signal relay to open gate
                digitalWrite(1, 0x1);

                mqttClient.beginMessage("Dust/Tool On");
                mqttClient.println(mapToJson);
                mqttClient.endMessage(); // Call Node-Red and turn the collector on, Node-red will run gate maps as a TOPIC back to all gates
                toolIsOn = true;
              }
          }
        else
          {
            if (digitalRead(blastGate.outletPin) == 0x1 && toolIsOn == true) // tool has been running and is now turned off INPUT_PULLUP hold HIGH when no outside current
              {
                //Serial.println("I tiggered the HIGH condition on readOutlets");
                     // set the 
                mqttClient.beginMessage("Dust/Tool Off"); // Turn tool off if elapsed time > delay spin down
                mqttClient.println(mapToJson);
                mqttClient.endMessage();
              }
            else if (toolIsOn == true)
              {
                if (!mqttClient.connected()) // check for a disconnect and reconnect to make sure messages go through
                  mqttClient.connect(broker, port);

                mqttClient.beginMessage ("Dust/Reset Tool Timer"); // Need to reset the elapsed time timer, and don't want to run through the startup in NodeRed
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
    char payload[_size + 1];
    String topic;
    //int error = 0;

    DynamicJsonDocument jSonDoc(JSON_SIZE);

    topic = mqttClient.messageTopic();
      if (topic != "Dust/Send Gate ID" && topic != "Dust/Close Me") // check the topic to make sure there is a json document
    {
      mqttClient.readBytes (payload, _size); // read the incoming message
      payload[_size+1] = '\0';
      DeserializationError error=deserializeJson (jSonDoc, payload); //deserialize the incoming payload to a json document
      if (error)
        {
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>(
# 289 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "289" "." "148" "\", \"aSM\", @progbits, 1 #"))) = (
# 289 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      "JSON parsing error: "
# 289 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      ); &__pstr__[0];}))
# 289 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      ))));
          Serial.println(error.c_str());
          return;
        }
    }

     if (topic == "Dust/Send Gate ID") // if Node-red has asked this gate to publish the ID
      {
        String mapToJson;
        jSonDoc["gate_ID"] = blastGate.gateID;
        jSonDoc["gate Name"] = blastGate.gateName;
        serializeJsonPretty (jSonDoc, mapToJson);
        //serializeJsonPretty (jSonDoc, Serial);
        mqttClient.beginMessage ("Dust/Gate ID");
        mqttClient.println (mapToJson);
        mqttClient.endMessage();
      }
    else if (topic == "Dust/Run Gates") //  get the value from the gateMap, open or close gaet base on gateMap 
      {
          mqttClient.readBytes (payload, _size);

          payload[_size] = '\0';
          DynamicJsonDocument mqttDoc (JSON_SIZE);

          deserializeJson (mqttDoc, payload);
        //   serializeJsonPretty (mqttDoc, Serial);
          workingGate = mqttDoc["sent Gate"];
          String bgString = String(workingGate);
          String _gate = String(blastGate.gateID);
          JsonObject gateMapObject = mqttDoc["gateMap"].as<JsonObject>();;

          if (workingGate != blastGate.gateID) // so long as this code is not running on the instructing gate, execute gate action
            {
              __gateState = gateMapObject[_gate];
              if (__gateState == 0x0)
                closeGate();
              else
                openGate();
            }
      }
    else if (topic == "Dust/Clear All Gates") // Not Implemented yet
      {

      }
    else if (topic == "Dust/Not Unique")
      {
        blastGate.gateID = jSonDoc["gate Id"];
        if (saveConfigEEPROM(0))
          {
            Blynk.virtualWrite (5, blastGate.gateID);
            Blynk.syncVirtual (5);
            holderGateId = -1;
            dirtyConfigBit = false;
            statusUpdate.clear();
            statusUpdate.print ("changed the gate Id to ==> " );
            statusUpdate.println (blastGate.gateID);
            statusUpdate.println (((reinterpret_cast<const __FlashStringHelper *>(
# 345 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                                 (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "345" "." "149" "\", \"aSM\", @progbits, 1 #"))) = (
# 345 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                                 "be sure to check the gate map for the appropriate mapping to the gate"
# 345 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                                 ); &__pstr__[0];}))
# 345 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                                 ))));

          }
      }
    else if (topic == "Dust/Is Unique") //  implement the save to saveConfigEERPOM here, only if a unique value comes back from node-red
      {
        if (saveConfigEEPROM(0))
          {
            holderGateId = -1;
            dirtyConfigBit = false;
          }

      }

    else if (topic == "Dust/OFF") // resets the toolIsOn variable
      {
          toolIsOn = false;
          mqttClient.beginMessage("Dust/Close All Gates"); // want to close all the gates, so the relays are not always energized
          mqttClient.endMessage ();

      }
    else if (topic == "Dust/ToolIsOn")
      {
        mqttClient.readBytes (payload, _size);

          payload[_size] = '\0';
          DynamicJsonDocument mqttDoc (JSON_SIZE);

          deserializeJson (mqttDoc, payload);
           //serializeJsonPretty (mqttDoc, Serial);
          workingGate = mqttDoc["sent Gate"];
   // Serial.println ("blastGate.gateId ==> " + String (blastGate.gateID) + " | workingGate ==> " + String (workingGate));
          if (workingGate == blastGate.gateID) // only turn the tool on if this gate turned on the collector
            toolIsOn = true;
      }
    else if (topic == "Dust/Close Me")
        {
           closeGate(); // all gates will close, including the "was on " tool gate
        }
    else if (topic == "Dust/Tool On") // Not Implemented yet
      {

      }
    else if (topic = "Dust/New Gate Id") // Not implemented yet
          {
            // or do i put the logic for the calling of node-red here, and let the IS-UNIQUE handle the response from mqtt
          }
  }

void setMQttSubscriptions() // function to define which topics are subscribed, and setting the callback
  {
      mqttClient.subscribe ("Dust/Run Gates");
    //mqttClient.subscribe (CLEAR_NODES);
    mqttClient.subscribe ("Dust/Send Gate ID");
    //mqttClient.subscribe (PUBLISH_GATE_ID);
    mqttClient.subscribe ("Dust/Not Unique" );
    mqttClient.subscribe ("Dust/Is Unique" );
    mqttClient.subscribe("Dust/OFF");
    mqttClient.subscribe("Dust/ToolIsOn");
    mqttClient.subscribe("Dust/Close Me");
   // mqttClient.subscribe (NEW_GATE_ID);
    mqttClient.onMessage (nodeRedListenCallback);
   }

/*******************************************************************
 * bool saveConfigEEPROM()
 *    used from teh blynkUI to save any changes in the gate configuration 
 *    screen.   Will only write if the dirty_bit has been set to true
 *    returns false if not able to save to eeprom
**********************************************************************/
bool saveConfigEEPROM( int _RW) // checks for WRITE or ERASE and saves blastGate structure to EEPROM or erases EEPROM
  {

    int remaining;
    remaining = EEPROM.length() - sizeof(configStore) - sizeof(blastGate);
    if (remaining > 0 && _RW == 0)
      EEPROM.put (configAddy, blastGate);
    else if (_RW == 0)
    {
      Serial.println(((reinterpret_cast<const __FlashStringHelper *>(
# 424 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "424" "." "150" "\", \"aSM\", @progbits, 1 #"))) = (
# 424 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    " the blast gate structure will not fit in EEPROM"
# 424 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    ); &__pstr__[0];}))
# 424 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    ))));
      return false;
    }
  //Serial.println("dirtyConfig ==> " + String(dirtyConfigBit) + " | remaining ==> " + String (remaining) + " | __RW ==> " + String(_RW));
    if (dirtyConfigBit && remaining > 0 && _RW == 0)
      {

        EEPROM.put(configAddy, blastGate);
        EEPROM.commit();
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>(
# 433 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "433" "." "151" "\", \"aSM\", @progbits, 1 #"))) = (
# 433 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      "WROTE blast gate structure to EEPROM"
# 433 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      ); &__pstr__[0];}))
# 433 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      ))));
        dirtyConfigBit = false;
        statusUpdate.println (((reinterpret_cast<const __FlashStringHelper *>(
# 435 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                             (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "435" "." "152" "\", \"aSM\", @progbits, 1 #"))) = (
# 435 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                             "saved new settings to EEPROM"
# 435 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                             ); &__pstr__[0];}))
# 435 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                             ))));
        return true;
      }
    else if (_RW == 1)
      {
        int _address = configAddy;
        int __step = 0;
        statusUpdate.clear();
        statusUpdate.print("Clearing");
        while (__step < sizeof (blastGate))
          {
            EEPROM.write(_address, 0XFF);
            _address++;
            __step++;
            statusUpdate.print("."); // 1.3.1
          }
        //statusUpdate.clear();
        statusUpdate.println(((reinterpret_cast<const __FlashStringHelper *>(
# 452 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                            (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "452" "." "153" "\", \"aSM\", @progbits, 1 #"))) = (
# 452 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                            "\nCleared EEPROM"
# 452 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                            ); &__pstr__[0];}))
# 452 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                            ))));
        return true;
      }
    else
      return false;
  }

bool getEEPROMConfig( int _where) // populates the blast gate structure from EEPROM
  {
    byte temp;
    temp = EEPROM.read (configAddy);
    EEPROM.get(configAddy, blastGate);
    if (blastGate.gateID != -1 )
      return true;
    else if (_where != 15)
      {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>(
# 468 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "468" "." "154" "\", \"aSM\", @progbits, 1 #"))) = (
# 468 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      "did not read EEPROM"
# 468 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                      ); &__pstr__[0];}))
# 468 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                      ))));
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
      blastGate.gatePin = 5 /* for a WEMOS with a relay shield, this needs to stay set to 5 or the relay will not trigger   1.3.3*/;
    pinMode(blastGate.gatePin, 0x01);
    if (blastGate.outletPin > 5 && blastGate.outletPin < 11) // on a WEMOS, can not use GPIO 6-11, they are used by the board directly
      blastGate.outletPin = 12 /* defaulting the outlet monitoring pin   */;
    pinMode(blastGate.outletPin, 0x02);
    digitalWrite (blastGate.gatePin, 0x0); // closing the gate on startup
    pinMode(15 /* using the GPIO pin # instead of the header number (D)*/, 0x01);
    digitalWrite(15 /* using the GPIO pin # instead of the header number (D)*/, 0x0);
  }

void setBlynkConfigUI()
  {
    if (int(blastGate.gateName[0]) == 255)
      strcpy(blastGate.gateName, "Undefined");
    Blynk.virtualWrite(6, blastGate.gateName);
    Blynk.virtualWrite(5, blastGate.gateID);
    if (blastGate.gatePin < 1) //1.3.0
      {
        blastGate.gatePin = 5 /* for a WEMOS with a relay shield, this needs to stay set to 5 or the relay will not trigger   1.3.3*/;
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(2, blastGate.gatePin);
    if (int(blastGate.toolName[0]) == 255) //1.3.0
      {
        strcpy(blastGate.toolName, "Undefined");
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(11, blastGate.toolName);
    if (int(blastGate.gateMap[0]) == 255) //1.3.0
      {
        strcpy(blastGate.gateMap, "00000000000");
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(13, blastGate.gateMap);
    if ( blastGate.outletPin < 1) // 1.3.0
      {
        blastGate.outletPin = 12 /* defaulting the outlet monitoring pin   */;
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(4, blastGate.outletPin);
    Blynk.virtualWrite(3, blastGate.collectorDelay);
    if (int(blastGate.nodeRedPrefix[0]) == 255) // 1.3.0
      {
        strcpy(blastGate.nodeRedPrefix, "Undefined");
        dirtyConfigBit = true;
      }
    Blynk.virtualWrite(12, blastGate.nodeRedPrefix);
    Blynk.syncAll();
  }

void openGate() {
  digitalWrite(blastGate.gatePin, 0x1);
}

void closeGate() {
  digitalWrite(blastGate.gatePin, 0x0);
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
    DynamicJsonDocument jSonDoc(JSON_SIZE);

    BlynkEdgent.begin();
    while (BlynkState::get() != MODE_RUNNING && count < 300) //  give time for the Blynk.edgent() sub functions to execute.   
      {
        count++;
      //  Serial.println("Runnint BlynkEdgent.run() to process Edgent connection   " + String(BlynkState::get()) + "  running mode ==> " + String(MODE_RUNNING));
        BlynkEdgent.run();
      }
//   removed the wifi.begin line here, it was error out, and unneccessary   1.3.0

    if (!mqttClient.connect(broker, port))
      {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>(
# 574 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "574" "." "155" "\", \"aSM\", @progbits, 1 #"))) = (
# 574 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    "MQTT connection failed! Error code = "
# 574 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    ); &__pstr__[0];}))
# 574 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    ))));
        Serial.println(mqttClient.connectError());

        while (1);
      }

    if (mqttClient.connected ())
      {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>(
# 582 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "582" "." "156" "\", \"aSM\", @progbits, 1 #"))) = (
# 582 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    "You're connected to the MQTT broker at IP Address and port ==> "
# 582 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                    ); &__pstr__[0];}))
# 582 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                    ))));
        Serial.println(String(broker) + " : " + String(port));

        setMQttSubscriptions ();
      }

    if (BlynkState::get() == MODE_RUNNING)
      {
        configAddy = 0 + sizeof(configStore) + 8;
        if (getEEPROMConfig(0))
          {
            setBlynkConfigUI(); // Set the Blynk config screen
            setPins(); // Set the appropriate digital pins
            String mapToJson; // Variable to hold the Json structure
            jSonDoc["gate_ID"] = blastGate.gateID;
            jSonDoc["gate Name"] = blastGate.gateName;
            serializeJsonPretty (jSonDoc, mapToJson);
            if (!mqttClient.connected())
              {
                Serial.println ("have to reconnect to mqqt");
                mqttClient.connect(broker, port);
              }

            if (mqttClient.connected())
            {
              mqttClient.beginMessage ("Dust/Gate ID"); // post the gateID to the NodeRed broker
              mqttClient.println (mapToJson);
              mqttClient.endMessage();
            }
          }
        else
          {
            blastGate.gatePin = 5 /* for a WEMOS with a relay shield, this needs to stay set to 5 or the relay will not trigger   1.3.3*/;
            blastGate.outletPin = 12 /* defaulting the outlet monitoring pin   */;
          }
      }
    statusUpdate.clear();
    statusUpdate.println(((reinterpret_cast<const __FlashStringHelper *>(
# 619 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                        (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "619" "." "157" "\", \"aSM\", @progbits, 1 #"))) = (
# 619 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                        "ready"
# 619 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                        ); &__pstr__[0];}))
# 619 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                        ))));
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>(
# 620 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                  (__extension__({static const char __pstr__[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "DustCollection_OTA.ino" "." "620" "." "158" "\", \"aSM\", @progbits, 1 #"))) = (
# 620 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                  "ready!!!!!"
# 620 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino" 3
                  ); &__pstr__[0];}))
# 620 "/Users/corywiegert/SynologyDrive/Automated Dust Collection/gitHub/DustCollection_OTA/DustCollection_OTA/DustCollection_OTA.ino"
                  ))));
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
