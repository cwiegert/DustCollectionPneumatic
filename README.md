# Dust Collection Automation using Pneumatic Actuators
##  This is a next evolution of the Dust Collection EEPROM repo, with IoT and air driven gates


**IMPORTANT UPDATE V. 1.4.0 - changes to an arduino library**
Because the arduino standard install for the ArduinoMqttClient uses the MqttClient.h library, there is a 256 byte limitation on the size of messages sent to the mqtt broker.  To get around this limitation, you can modify the MQttClient.CPP found in the src directory under your standard location for the ArduinoMqttClient library install.   (typically /~/documents/arduino/library)     This repo does not contain the modified file, only the instructuions here on how to modify the buffer size for a larger topic message to the broker.   

If there is a bufffer over run, the arduino library will truncate your Json document to the size of the buffer, and NodeRed will throw an error "unexpected end of JSON file"

To modify to buffer size, open the MqttClient.cpp and chnage the parameter TX_PAYLOAD_BUFFER_SIZE

```  #ifndef TX_PAYLOAD_BUFFER_SIZE
  #ifdef __AVR__
    #define TX_PAYLOAD_BUFFER_SIZE 128
  #else
 //   #define TX_PAYLOAD_BUFFER_SIZE 256            old code which comes with the arduino library.   
    #define TX_PAYLOAD_BUFFER_SIZE 512          //  CDW 5/26/2024 because JSON messages were being truncated at 256 bytes
  #endif
#endif
```


**Why make the change from the servo driven dust gate automation?**
  
  To start with, the code base for management and administration I was looking for on the dust collection automation was getting too large to run on a single arduino.   Even with the Mega and the 8M of RAM to load programs, I had hit a limit on stability and amount of flexibility necessary for where I wanted to go with the automation.  Instead of trying to get more efficient with the 1 large code set, it was easier to go down the Internet of Things path, and have each gate be an autonomous node, but have it all wired together through NodeRed.   

  The flexibility provided by the NodeRed workflow management gave me the global orchestration necessary to configure all the gate controllers to act independently, but instruct the other nodes in the system how to operate.    The code base was going to be simpler, and there only need be a messaging system to do the instructions for system to know when to open and close a gate.   In this workflow, we also added the delay to allow for start and stop a machine (i.e. current on, off, on, off ) like a chop saw without continuously cycling the dust collector.    The *reset timer* and *store delay* functions on the Dust Collector panel control when the GPIO on the Raspberry Pi is set to off.

  Secondly, with the simplification of the nodes and code, I could run the NodeRed and the Mosquitto MQTT broker on a Raspberry Pi.   By adding the Raspberri Pi to the system and running the dust collector off a GPIO pin on the Pi, there was no need to write any code to control the logic of the collector.   Simply, there would be a NodeRed workflow which set the GPIO to high or low, all based on the logic from the outlet monitor on the gate.  

  Finally, the servo's kept burning up!   Because the gates are not precise with the slides and dust was getting into the gate slide, the servo was not powerful enough to drive the gate open/closed when there was dust in the slide, or if the slide was a little out of center.   Even with 35kg servo's, the gate would bind.  Because there is no feeback loop from the servo, it will continue to try to run to the position instructed until it either gets to that position or, the servo burns it's windings.

  Enter youtube!    I need to give an acknowledgement and shout out to Rings Workshop and this video he put out [Rings Workshop](https://www.youtube.com/watch?v=1p1USQXDALg)    YES!!!!  that is what I was looking for.   The code base is pretty simple on his set up, and follows the same pattern as on the EEPROM version from my repository -but the 3D printed brackets and air driven actuators is what was new for this set up.

**Configuring the System, with all the moving parts.   A simple overview, with details below**

  This system setup is driven by BLYNK OTA and all devices are activated off the auth code given when BLYNK activates a device.   The big mindset shift is that each time you activate a device, you are instantiating the Blynk Template.    When the wemos boots up, it is in activation mode, with only the factory firmware on it, and can be activated by adding a new device on the BLYNK mobile app.   Once the device is acitve, it will ping the BLYNK server for a new shipment, if 1 is found, it will be installed as the latest firmware on the device and reboot.    Whala, you have a new gate configured for the IoT system.

  **NodeRed** - is a IoT workflow manager that controls all of the inter-gate communication by use of MQTT messaging.   There is a screen shot below that outlines some of the flows, and a JSON package is included in the repo to import all the flows for the sytem.   Comments are included in NODERed to outline what each function does.   Connecting to the MQTT broker is set as the Server property within an MQTT message.  
<img width="400" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/9d159e8f-81fe-4000-a55c-98225a663af7"> <img width="400" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/58427aa8-104c-4274-83d6-4ba27a6a302b">



  **Raspberry Pi** -- NodeRed and the Mosquitto MQTT broker are installed on a Raspberry Pi running the latest Raspbian OS.   There are plenty of sites outlining how to get each installed and running.    IT's important to note the IP address and port for the Mosquitto MQTT installation.   That variables *broker* and *port*  must be set in the *DustCollectorGlobals.h*  for any connectivity to the mqtt broker to work.   Also, the GPIO pin configured on the Dust Collector page, Node --> Collector Power, should have a wire connected to the 5V activator on the dust collector heavy duty relay switch.   (Same as it was in the EEPROM model, but here we are coming off the Pi instead of an arduino)
  <img width="300" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/e9f78e02-6d38-43e2-8a1f-7dd58b5663c3">


  **Voltaage Sensor** -- simplifying the systsem was 1 of the goals of this project.    Instead of having to monitor and sense a specific voltage change, I used a 5V relay switch sensor to simply send 5 volts to a INPUT_PULLUP pin and bring that pin to ground.   No, it's a matter of reading LOW on that pin, and not having to debounce and calculate the delta to remove noise from the system.   I used these from Amazon [Voltage Sensor](https://www.amazon.com/dp/B0B7VT46KG?psc=1&ref=ppx_yo2ov_dt_b_product_details)  as the sensors.  K1 and K2 are simply a switch relay which is normally open, and when current is applied, the NO switch is closed.   [Relay](https://www.amazon.com/gp/product/B01MCWO35P/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1) 
<img width="200" alt="image" src="https://m.media-amazon.com/images/I/61duwk7MCIL._AC_SL1100_.jpg">


**12V external power supply** -- used to power the solenoid through the Wemos Relay shield.   Of course, if you are using heavier duty solenoid (24V) you will need to match the power supply to the voltage of the solenoid, and have a corresponding relay that can handle the voltage.   I typically use the MDR-60-12 by MeanWell, as I have used MeanWell on all my automation projects and have never had any issues.

**5V LED's colored** -- used to provide external indication of what is happening with the lights of the Wemos and Relay.   Have Red for when the relay is activated and voltage through NO, and Blue which is synched up to the Wemos onboard LED flashing light.   These come with an inline resistor --> [5V Colored LED](https://www.amazon.com/dp/B07TPMRLVR?psc=1&ref=ppx_yo2ov_dt_b_product_details)

**Buck Converter** -- added this to the system to manage the all the power.   The 12V line will run from teh power source, either in parallel or branched from gate to gate along a single trunk.   [Buck Converter](https://www.amazon.com/dp/B081N6WWJS?psc=1&ref=ppx_yo2ov_dt_b_product_details) the 12v, will be dropped down to 5V to run the wemos, and a line will branch off the [Splitter](https://www.amazon.com/dp/B087B6FQLB?psc=1&ref=ppx_yo2ov_dt_b_product_details) to run the solenoid (12V) and another 12V branch to power the next gate in line.


  **Soldering the Wires, and configuring the pins**
1. GPIO 12 (D6) on the Wemos  -----------  K1 on Voltage Sensor
2. K2 on voltage sensor       -----------  Ground on Wemos
3. D1 on Wemos                -----------  Default driver for the Wemos Relay (gatePin)
4. D4 on Wemos                -----------  Positive leg of Blue LED (external flasher of the onboard LED)
5. Negative leg of LED        -----------  Ground on Wemos
6. D1 on Wemos                -----------  Positive leg of Red LED (external light to show relay activated)
7. V+ on Solenoid             -----------  NO on Wemos Relay
8. V- on Solenoid             -----------  V- on 12V power supply
9. V+ on 12V power            -----------  COM on Wemos relay shield
10. Negative Leg of LED       -----------  Grown on Wemos
11. 11. USB-C power pigtail   -----------  Powers Wemos
            

## How this set up works ##

1.  Everything is run on a WEMOS D1 Mini with a Relay shield attached.   I prefer to use the new WEMOS, becuase the USB-C power plug is on the top of the board, and gives a better set in the box.    There are so many on Amanzon, I found [WEMOS](https://www.amazon.com/gp/product/B0BKSKV54X/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&th=1) to be easiest and most consitent <img width="200" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/5a1aaa89-df41-45b8-9afb-834c7c121351">

Along with the [Relay Shields]( https://www.amazon.com/dp/B093LC9DNJ?psc=1&ref=ppx_yo2ov_dt_b_product_details)  Each gate has the logic controller and the switching mechanism to drive the air actuator.  <img width="200" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/d3e406d0-5df1-4f13-a2be-f3915baf8e0a">

2.  The Air actuator is the key to this.   It is activated by the NO pole on the relay, which in turn is driven by pin **D1** on the WeMOS.  When soldering the headers to the Wemos and the relay, be sure to have the D1 and Power pins aligned or none of this system will work.     Simply, 12V power is driven to the **COM** on the relay, 1 wire from the actuator solenoid is connected to **NO** and the other solenoid wire connected to V- on the 12V power source.    The 5V trigger for the relay is pin D1, which is GPIO 5, and defined in DustCollectorGlobals.h.    I used 12V solenoids, as there isn't a need to have massive 24V drivers on these gates.... the gates are light.   I found these [Actuators]( https://www.amazon.com/dp/B00YCWL9I4?psc=1&ref=ppx_yo2ov_dt_b_product_details) to fit the hangers best.
<img width="200" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/9b7bac00-f1e0-492e-890c-a999b8c3a4f5">

3.  The solenoids drive the air piston.   They are fairly inexpensive and can be found again on Amazon [Solenoids]( https://www.amazon.com/dp/B07DYPM7XB?ref=ppx_yo2ov_dt_b_product_details&th=1)    The air piston is connected to the gate via the 3D printed connecting arm.   The links to all the 3D printed parts can be found at the bottom of the readme, and all the .STL's and fusion360 files are in the repo.

4.  It's really simple.    The wemos starts up and activates itself, waiting for a configuration.   Start the "add new device" from the Blynk App and discover the new wemos.   The firmware is updated and the wemos reboots.   Upon reboot, the 2.4Ghz network you set during the activation will be used to reconnect to you network.  NO credentials are stored in discoverable format.   Once up and running, the wemos will send it's gate ID to the NodeRed, which will add the id to the global array, or if not unique, will hand you back a gate id which is then stored in EEPROM on the wemos.
   
5.  Configure the gate map on the BlynkApp.    **IMPORTANT** The gateMap array is a 0 based array, and typically a system is set up with the first gate in being gateID #1.   Remember to put a 0 placeholder in all your gateMaps, or the array for each gate will be out of synch.  So if you have 6 gates, you will need 7 elements in the array "0110001".   If your tool is on gate 6, the map will open gates 1, 2 and 6 will be the activation gate.   The leading 0 is not used, but is necessary.  This will be a critical step if you have gates to open between your machine driven gate and the dust collector.   It's very simple, each position in the 0-1 map corresponds to an index of a gate.   If there is a 1, the gate should be opened, if closed, a 0.   Save the new name, tool name, gate map to EEPROM, and restart the device.    You're ready to go.

6.  The code is really really simple.   The readOutlet function is run in the Loop.   That function simply checks the outletPin for a pull down to LOW.   If low, it tells NodeRed a tool has been turned on, NodeRed then tells all gates to run the gate map it sends as part of the JSON message.  Every gate, except the 1 originating the message, will either open or close itself.

7.  If a gatePin is read LOW, and the "isOn" flag is set, the spin down timer is reset to 0, and the next outlet is checked.
  
9.  If there is no machine running, the spin down value will be applied, a message to close all gates and spin down the dust collector is issued...

**Assembly of 3D printed parts**
For simplicity sakes, there have been heated threaded inserts added to this model.   In previous versions, there were threaded holes in the PETG, but the risk of stripping the treads and the screws cross threading made the assembly of the parts difficut.     The buck converter box is connected to the hanger with M2 insters, while the air solenoid and the wire connector/splitter use M3.    To install, use the provided soldering iron tip, heat the soldering iron to 300 C, slip the insert onto the tip and melt it into the pre-printed holes.    

Parts are found in the links section below.

M2  <img width="200" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/a0d9fe45-202a-4e98-929a-1fd551209326">

M3
<img width="200" alt="image" src="https://github.com/cwiegert/DustCollectionPneumatic/assets/33184701/999271e5-d6ef-42e8-bd36-09f3cf51c99b">


______________________________________________________________________________________________________
**Links**

[3D files](https://www.thingiverse.com/thing:6603995)    other than in this git repo

[Solenoids](https://www.amazon.com/dp/B07DYPM7XB?ref=ppx_yo2ov_dt_b_product_details&th=1)

[Actuators]( https://www.amazon.com/dp/B00YCWL9I4?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[WEMOS D1 Mini](https://www.amazon.com/gp/product/B0BKSKV54X/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&th=1)

[WEMOS Relay 12V, 5V activation]( https://www.amazon.com/dp/B093LC9DNJ?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[LEDs](https://www.amazon.com/dp/B07TPMRLVR?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[USB Pigtails](https://www.amazon.com/dp/B0CMQ2SYF3?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[MiniPlug pigtail](https://www.amazon.com/dp/B0B4C2VSHV?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Current Sensing Relay](https://www.amazon.com/dp/B0B7VT46KG?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Long screw for box top](https://www.amazon.com/dp/B09LYKYV2S?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Screw for box top](https://www.amazon.com/dp/B07H14FRRB?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[M3 screws for holding solenoid](https://www.amazon.com/dp/B0CMDHGNJF?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[CF-PETG](https://www.amazon.com/dp/B0CCVKJJQC?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Air fittings](https://www.amazon.com/dp/B07ZHG8Y1Y?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Air mufflers](https://www.amazon.com/dp/B08K6T9H2K?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Air Controller](https://www.amazon.com/dp/B01ILJFJ4I?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[12V - 5V Buck Convert](https://www.amazon.com/dp/B081N6WWJS?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[Wire Connector and Splitter](https://www.amazon.com/dp/B087B6FQLB?psc=1&ref=ppx_yo2ov_dt_b_product_details)

[M3 heated insert](https://www.amazon.com/dp/B0BBSLL6G7?ref=ppx_yo2ov_dt_b_product_details&th=1)

[M2 heated insert](https://www.amazon.com/dp/B0CS6XJSSL?ref=ppx_yo2ov_dt_b_product_details&th=1)





