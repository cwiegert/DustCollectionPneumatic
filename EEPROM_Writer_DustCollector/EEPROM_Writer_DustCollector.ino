// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID "TMPL4y633j20"
#define BLYNK_DEVICE_NAME "DustCollector EEPROM"
#define BLYNK_AUTH_TOKEN "iLO3VC0Vq6XRdTAjKTb7Y3LT6ifV8E-r"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
/**************************************************************************************************
 * Test program to figure out if we can use EEPROM to store variables for setup and managmement
 * Would like to have a tool that allows us to set EEPROM for each stepper motor, so we don't have to 
 * use the SD cared to store config settings.   EEPROM, once written, does not reset until it is over written or erased by 
 * writing to EEPROM again.   It may be a good way to set configuration on automation tools, and would use this program to set them up
 * 
 * once they are set up, EEPROM.get and EEPROM.read can be used to set the config necessary during run time, or doing lookups
 * 
 * mega 2560 has 4096 bytes
 * uno  has 
 * nano has 512 bytes
 * 
 * set the boardMemory variable to the appropriate size , and the memory will be cleared then written 
 * May need to modify code for the right program and initializing - but the concpet is here, 
 * 
 * reading memory is based on sizeof(type)... that will advance the reading point
 * can write to various points of EEPROM by using put, instead of write.   Can create structures and write entire structure
 *    instead of writing variable by variable
 *    use last read address += sizeof(variable type) to advance to te next read
 *    
 *    
 * version  History
 *       Cory D. Wiegert    v6.0.10.10.2021     initial version ready to be tested on live system
 *                                              built to go with DustCollectionAutomation v.6.0.10.10.2021
 *                                              the dust collection system will use EEPROM to stores the configuration settings for the 
 *                                              gates, outlets, and wifi.  There is an export function here to archive a version of the config
 *                                              Also modified the Blynk app to support changes to EEPROM during config
 *        Cory D. Wiegert   v6.1.10.18.2021     Modified the outlet config section to adjust for floats in the gate structures
 *                                              tested the save to file, import to file and have continuity
 **********************************************************************************/

#include <EEPROM.h>
#include "SdFat.h"
#include "DustCollectorGlobals.h"
#include <AESLib.h>
//#define USE_NODE_MCU_BOARD
//#include "BlynkEdgent.h"



//SdFat   sdCard;                        //  pointer to the SD card reader.   Used in storing config and memory files
//SdBaseFile  fConfig;
  int     eeAddress =0;
  
/*****************************************************
 *  function to erase everything from EEPROM.  
 *  will write a 255 into every byte of EEPROM
 *  ready's the board for the new new configuration
 * ****************************************************/
 void clearEEProm ( int boardSize)
  {
    int lastx = 0;
    Serial.println(F("clearing the EEPROM"));
    for (int x = 0; x< EEPROM.length(); x++)
      {
        if (x == lastx + 80)
          {
            Serial.println(".");
            lastx = x;
          }
        else
          Serial.print(".");
        EEPROM.write(x, 255);
      }
     Serial.println("");
     Serial.println(F("EEPROM Cleared"));
  }
  
 /***********************************************************************
  *    void   readConfig (SdBaseFile fconfig, char sectDelim, char* delim)
  *    
  *    function to read the initial setup line in the config file.   sets the 
  *    global variables as functioning in the switch statement.
  ************************************************************************/
  void readConfig (char configDelim, char delim[])
    {
      char          servoString[160] = {'\0'};
      char          memFile[30] = {'\0'};
      char          *token;
      int           index;
      int           bytesRead;
      char          serGet;
      char          checker;
      
      bytesRead = fConfig.fgets(servoString, sizeof(servoString));
      while ((servoString[0] != configDelim) && fConfig.available())                         // test to see if this is a comments section
        bytesRead = fConfig.fgets(servoString, sizeof(servoString));
      token = strtok(servoString, delim);  
      if (servoString[0] != '#')              //  checking the commented gages
        { 
          for (int index = 0; index < 9; index++)      
          {
            token = strtok(NULL, delim);
            switch (index)
              {
                case 0:
                  dust.servoCount = atoi(token);
                  break;
                case 1:
                  dust.NUMBER_OF_TOOLS = atoi(token);
                  break;
                case 2:
                  dust.NUMBER_OF_GATES = atoi(token);
                  break;
                case 3:
                  dust.DC_spindown = atoi(token);
                  break;
                case 4:
                  dust.dustCollectionRelayPin = atoi(token);
                  break;
                case 5:
                  dust.manualSwitchPin = atoi(token);
                  break;
                case 6:
                  dust.mVperAmp = atof(token);
                  break;
                case 7:
                  dust.debounce = atoi (token);
                  break;
                case 8:
                  dust.DEBUG = atoi(token);            // last parameter of string, will set the DEBUG variable to send messages to serial monitor
                  break;
              } //end of case statement
          }   // end of for loop parsing line
        } // end of test whether or not the tool was active/commented out
      writeDebug ("servo count == > " + String(dust.servoCount) + " | NUMBER_OF_TOOLS ==> " + String(dust.NUMBER_OF_TOOLS), 1);
      writeDebug ("NUMBER_OF_GATES ==> " + String (dust.NUMBER_OF_GATES) + " | DC_SpinDown == > " + String(dust.DC_spindown), 1);
      writeDebug ("DC Relay Pin ==> " + String(dust.dustCollectionRelayPin) + " | Manual Switch ==> " + String(dust.manualSwitchPin), 1);
      writeDebug(" mVperAmp ==> " + String(dust.mVperAmp) + " | debounce ==> " + String(dust.debounce) + " | DEBUG ==> " + String(dust.DEBUG), 1);
    //fConfig.close();
    }
  
  /***********************************************************************
    *    int   readGateConfig (char sectDelim, char* delim)
    *    
    *    funciton to read the configuration of the gates, and remove it from the setup section
    *    many of the variables defined in the setup, can be moved to local variables by having
    *    locals, and settign the structure variables through this function
    ************************************************************************/
  
  int readGateConfig (char sectDelim, char delim[])
    {
      char          gateString[160] = {'\0'};
      char          *token;
      int           index;
      char          tempString[48] = {'\0'};
      int           counter;
      char          checker;
      int           bytesRead;
      char          serGet;

    Serial.println("entered readgate section");
      //fConfig.open(fileName, O_READ);
      bytesRead = fConfig.fgets(gateString, sizeof(gateString));
   Serial.print ("sectDelim ==> ");
   Serial.println(sectDelim);
      while (gateString[0] != sectDelim)                          // test to see if this is a comments section
        {
          bytesRead = fConfig.fgets(gateString, sizeof(gateString));
        }
  //  Loop through the configuration and get the blast gate configurations
      counter = 0;
  
      while (gateString[0] == sectDelim)
        {                   
          if ( gateString[0] != '#')
            {
    Serial.println(gateString);
              token = strtok(gateString, delim);

              for (index = 0; index < 6; index++)      
                {
                  token = strtok(NULL, delim);  
                  switch (index)
                    {
                      case 0:
                        blastGate[counter].gateID = atoi(token);
                        break;
                      case 1:
                        blastGate[counter].pwmSlot = atoi(token);
                        break;
                      case 2:
                        strcpy (blastGate[counter].gateName,token);
                        break;
                      case 3:
                        blastGate[counter].openPos = atoi(token);
                        break;
                      case 4:
                        blastGate[counter].closePos = atoi(token);
                        break;
                      case 5:
                          strcpy (tempString,token);
                          stripComma (tempString, blastGate[counter].gateConfig);
                          break; 
                    }  // end of case statement
                  }  // end of the parsing of the line  For loop
              memset (gateString, '\0', sizeof(gateString));
              counter++;
            }   // end of the active switch, test for the commenting of the blast gates
          bytesRead = fConfig.fgets(gateString, sizeof(gateString));      
        }   // end of the section for all the blast gates
      return counter;
    }
  
  /*****************************************************************************
   *   int readSensorConfig (char sectDelim, char* delim)
   *          used to read the voltage sensor section of the configuration file.   similar to GateConfig
   *          this function will read the next section of the config file and set the 
   *          outlet box structures with the appropriate instance variables. 
   *          
   *          fSensor - the open file pointer
   *          sectDelim -- the marker to known when you are reading comments and hit the end of the section 
   *          delim -- pointer to the variable holding the special character that acts as the token flag in the config line
   *          
   *          returns, number of gates found
   **********************************************************************************/
  int readSensorConfig (char sectDelim, char delim[])
    {
         char          gateString[160] = {'\0'};
        char          *token;
        int           index;
        char          tempString[48] = {'\0'};
        int           counter;
        char          checker;
        int           bytesRead;
              
      //  Loop through the configuration and get the electric switches configuration

        counter = 0;
        bytesRead = fConfig.fgets(gateString, sizeof(gateString));
        
        while (gateString[0] != sectDelim)
          bytesRead = fConfig.fgets(gateString, sizeof(gateString));
        
        while (gateString[0] == sectDelim)
          {  
            index = 0; 
            if ( gateString[0] != '#')
              {
                token = strtok(gateString, delim);
                for (index = 0; index < 7; index++)      // Tokenize the baseline config parameters
                  {
                    token = strtok(NULL, delim);
                    switch (index)
                      {
                        case 0:
                          toolSwitch[counter].switchID = atoi(token);
                          break;
                        case 1:
                          strcpy (toolSwitch[counter].tool,token);
                          break;
                        case 2:
                          toolSwitch[counter].voltSensor = atoi(token);
                          break;
                        case 3:
                            //  v6.1.10.18.2021 -- changed atol to atof to match the data type in the structure
                          toolSwitch[counter].voltBaseline = atof(token);
                          break; 
                        case 4:
                          toolSwitch[counter].mainGate = atoi(token);
                          break; 
                        case 5:
                            //  v6.1.10.18.2021 -- changed atol to atof to match the data type in the structure
                          toolSwitch[counter].VCC = atof(token);
                          break;
                        case 6:
                            //  v6.1.10.18.2021 -- changed atol to atof to match the data type in the structure
                          toolSwitch[counter].voltDiff = atof(token);
                          break;
                      }   // end of the case statement          
                  }  // end of parsing the line
              }  // end of testing for an active switch and not commented in config file
            memset (gateString, '\0', sizeof(gateString));
            bytesRead = fConfig.fgets(gateString, sizeof(gateString));
            counter++;
    Serial.println(gateString);
         }   // end of electronic switch section
       //fConfig.close();
       return counter;
    }
/*****************************************************************************
     void startWifiFromConfig (char sectDelim, char delim[], int WifiStart)
            used to read the Blynk section of the configuration file.
            The ssid, password and server connection are all defined in this section
            and will be used to understand how to connect to the Blynk server

            fSensor - the open file pointer
            sectDelim -- the marker to known when you are reading comments and hit the end of the section
            delim -- pointer to the variable holding the special character that acts as the token flag in the config line


 **********************************************************************************/
void startWifiFromConfig (char sectDelim, char delim[], int WifiStart)
{
  char     gateString[160] = {'\0'};
  int      bytesRead;
  int      index;
  char     *token;
  SdBaseFile   fEncrypt;


  //  read the section for the Blynk wifi setup -- move this to a function as well, just need to make sure it works
  if (WifiStart)
  {
   Serial.println("Starting the WIFI");
    bytesRead = fConfig.fgets(gateString, sizeof(gateString));
    while (gateString[0] != sectDelim)                          // test to see if this is a comments section
      bytesRead = fConfig.fgets(gateString, sizeof(gateString));
    token = strtok(gateString, delim);
    token = strtok(NULL, delim);
    //strcpy(cypherKey, token);             // This will be the filename of the wifi config file.
    strcpy(cypherKey, "DustWifi v53.cfg");

    memset (gateString, '\0', sizeof (gateString));
  }
  Serial.println(cypherKey);
  if (fEncrypt.open(cypherKey))
  {
    Serial.println("I was able to open the wifi file");
    fEncrypt.read(gateString, 96);
    //aes128_dec_multiple(cypherKey, gateString, 96);
    //aes128_dec(cypherKey, gateString, 96);
    index = 0;
    token = strtok(gateString, decrDelim);
    for (index = 0; index < 7; index++)      // Tokenize the baseline config parameters
    {
      token = strtok(NULL, decrDelim);
      switch (index)
      {
        case 0:
          strcpy(blynkWIFIConnect.ssid, token);
          break;
        case 1:
          strcpy (blynkWIFIConnect.pass, token);
          break;
        case 2:
          strcpy (blynkWIFIConnect.server, token);
          break;
        case 3:
          strcpy(blynkWIFIConnect.port ,token);
          break;
        case 4:
          strcpy(blynkWIFIConnect.ESPSpeed ,token);
          break;
        case 5:
          strcpy (blynkWIFIConnect.BlynkConnection, token);
          break;
        case 6:
          strcpy (blynkWIFIConnect.auth, token);
          break;

      }   // end of the case statement
    }  // end of parsing the line

    
    /* **************************************************
        setting up  the Blynk functionality and connecting to the Blynk server.   There are variables I shoudl be adding to the setup file, to know
        the baud rate, the address of the blynk server, the port # and other parameters necessary for Blynk to function
    ******************************************/
    if (dust.DEBUG == true)
    {
      writeDebug ("auth == > " + String (blynkWIFIConnect.auth), 1);
      writeDebug ("wifi coms speed ==> " + String(blynkWIFIConnect.ESPSpeed) + "  | Connection ==> " + String(blynkWIFIConnect.BlynkConnection), 1);
      writeDebug ("ssid ==> " + String(blynkWIFIConnect.ssid) + "  |  pass ==> " + String(blynkWIFIConnect.pass) + "  | server ==> " + String (blynkWIFIConnect.server) + " | serverPort ==> " + String(blynkWIFIConnect.serverPort), 1);
      //delay(500);
    }
    // Set ESP8266 baud rate
   /* EspSerial.begin(ESP8266_BAUD);
    if (WifiStart)
    {
      if ( strcmp(BlynkConnection, "Local") == 0 )
      {
        if (DEBUG)
          writeDebug("trying to connect to the local server", 1);
        Blynk.begin(auth, wifi, ssid, pass, server, serverPort);

        //  Blynk.begin("UemFhawjdXVMaFOaSpEtM91N9ay1SzAI", wifi, "Everest", "<pass>", "192.168.1.66", <port>);
      }
      else if (strcmp(BlynkConnection, "Blynk") == 0)
      {
        Blynk.begin(auth, wifi, ssid, pass, "blynk-cloud.com", 80);
      }

    }  // end of if Wifi start */
  } // end of If OPen Ecnrypt
  else
    Serial.println(F("couldn't open the encryption file"));

  fConfig.close();
}

/*****************************************************************************
 *  function to dump the entire EEPROM to a recovery file -- this is a raw
 *  data dump that can be read back in as a raw file.   It's a fast way to 
 *  recover after having a working running system.   It is paried with the 
 *  recoverEEPROMFromFIle
 * ***************************************************************************/
void exportEEPROMToFIle()
  {
      SdBaseFile  fEEPROM;
      int x = 0;

      sdCard.remove ("DustConnectorEEPROM.cfg");
      fEEPROM.open("DustCollectorEEPROM.cfg", O_WRITE|O_CREAT|O_WRITE);
      fEEPROM.rewind();
      do
        {
          fEEPROM.write(EEPROM[x]);
          Serial.print(F("EEPROM Address being writen to file ==> "));
          Serial.println(x);
          x++;
        } while (x <= EEPROM.length());
      fEEPROM.close(); 
  }

/*****************************************************************************
 *  function to read an archived EEPROM file to EEPROM.   This is a raw binary 
 *  read, and put to EEPROM.   There are no conversions in this recovery, as it
 *  should be an image of what was exported in exportEEPROMToFile()
 * ***************************************************************************/
void recoverEEPROMFromFile()
  {
    SdBaseFile   fRdEEPROM;
    int x = 0;

   // BlynkEdgent.begin();
    fRdEEPROM.open ("DustCollectorEEPROM.cfg", O_READ);
    fRdEEPROM.rewind();
    x=0;
    do
    {
      EEPROM.put(x, fRdEEPROM.read());
      Serial.print(F("EEPROM Address being read from file ==> "));
      Serial.print(x);
      Serial.print(" ==> ");
      Serial.println(EEPROM[x]);
      x++;
    } while (fRdEEPROM.available() && x <= EEPROM.length());
    
  }

void setup() 
  {
    Serial.begin(500000);
    delay (1000);

    pinMode(SD_WRITE, OUTPUT);       // define the SD card writing pin
            
    if (!sdCard.begin(SD_WRITE))
        Serial.println(F("Could not open the SD card"));
    else
      Serial.println(F("opened SD Card, available to open file"));

    if (fConfig.open(fileName))
      {
        Serial.println(F("in the open file section, starting to read"));
    
        readConfig (sectDelim, delim);                    // reads the first global config line from the config file.  
        gates = readGateConfig (gateSectDelim, delim);        // read the gates section of the config file
  Serial.println("finished reading gates");      
        outlets = readSensorConfig (outletSectDelim, delim);    // read the voltage sensor section of the config file    
        startWifiFromConfig (wifiSectDelim, delim, 1);           // read the wifi section of the config file, start the wifi, and connect to the blynk server
      }
    else
      { 
        Serial.print(F("could not open the file ==> "));
        Serial.println(fileName);
      }
    fConfig.close();
    
  }

void loop() 
  {
    // put your main code here, to run repeatedly:
    char   serGet;
    int     nTemp = 0;
    char    checker;

    Serial.println("");
    Serial.println (F("enter an <X> if you want to clear the EEPROM"));
    Serial.println(F("enter an <A> if you want to LLOOAADDD the settings to EEPROM"));
    Serial.println(F("enter an <B> if you want to RREEAADD the settings from EEPROM"));
    Serial.println(F("enter an <C> if you what the GATES section LLOOADDEEEDD"));
    Serial.println(F("enter an <D> if you want to RREEAADD the GATES config from EEPROM"));
    Serial.println(F("enter an <E> if you want to LLOOAADD the OUTLETS section to EEPROM"));
    Serial.println(F("enter an <F> if you want to RREEAADD the  OUTLET config from EERPOM"));
    Serial.println(F("enter an <G> if you want to LLLOOAADD the WIFI settings"));
    Serial.println(F("enter an <H> if you want to RREEEAADDD the WIFI settings"));
    Serial.println(F("enter <L> if you want to see what's in EEPROM"));
    Serial.println(F("enter an <S> to export the EEPROM for archiving"));
    Serial.println(F("enter an <R> to recover EEPROM from file"));

    while (Serial.available()==0);
      serGet = Serial.read();
    
    switch (serGet)
    {
      case 'X':
        {
          clearEEProm(EEPROM.length());
          break;
        }
      case 'A':     // Add main configuration to EEPROM starting at address 0, write end of 
         {          //  write end of main config into EEPROM.Length - 10
          fConfig.open(fileName, O_READ);
          fConfig.rewind();
          readConfig (sectDelim, delim);  
          eeAddress = SET_CONFIG;
          Serial.println("");
          Serial.println(F("going to write a bunch of data to the EEPROM, the filled byte indicates it hasn't been written"));
          EEPROM.put (eeAddress, dust);
          eeAddress += sizeof(dust);
          EEPROM.write(EEPROM.length()-10, eeAddress);
          nTemp = EEPROM[EEPROM.length() - 1];
          //fConfig.close();
          break;
        }
      case 'B':
        {
            eeAddress = SET_CONFIG;
            EEPROM.get (eeAddress, dust);
            Serial.println (F("Values of the DUST structure"));
            Serial.print(F ("   servoCount      ==> " ));
            Serial.println (dust.servoCount);
            Serial.print(F ("   NUMBER of Tools ==> " ));
            Serial.println (dust.NUMBER_OF_TOOLS);
            Serial.print(F ("   NUMBER_OF_GATES ==> " ));
            Serial.println (dust.NUMBER_OF_GATES);
            Serial.print(F ("   DC_spindown     ==> " ));
            Serial.println (dust.DC_spindown);
            Serial.print(F ("   DustColPin      ==> " ));
            Serial.println (dust.dustCollectionRelayPin);
            Serial.print(F ("   ManSwitchPin    ==> " ));
            Serial.println (dust.manualSwitchPin);
            Serial.print(F ("   mVperAmp        ==> " ));
            Serial.println (dust.mVperAmp);
            Serial.print(F ("   debounce        ==> " ));
            Serial.println (dust.debounce);
            Serial.print(F ("   DEBUG           ==> " ));
            Serial.println (dust.DEBUG);          
            break;
        }

      case 'C':       // Add Blast gates to the EEPROM memory, starting at address 100
        {             // write end of Gates EEPROM address into EEPROM.length() - 8
            eeAddress = GATE_ADDRESS;           
            for (int addy =0; addy < gates; addy ++)
              {
                Serial.print(".");
                EEPROM.put (eeAddress, blastGate[addy]);
                eeAddress += sizeof(blastGate[addy]);
              }
            if (dust.DEBUG)
              {
                Serial.print(F("\n\t\tSize of Gate structure ==> " ));
                Serial.println(sizeof(blastGate[2]));
                Serial.print("\t");
                Serial.println(sizeof(blastGate));
              } 
            EEPROM[EEPROM.length() - 8] = eeAddress;
            Serial.println(F("\n\t\t Blast gate Config loaded"));
            break;
          }
      case 'D':
        {
          eeAddress = GATE_ADDRESS;  
          for (int x = 0;  x < dust.NUMBER_OF_GATES; x++)
            {
              EEPROM.get(eeAddress, blastGate[x]);
              eeAddress += sizeof (blastGate[x]);
              Serial.print(F ("\ngateID   ==> " ));
              Serial.println(blastGate[x].gateID);
              Serial.print(F ("pwmSlot    ==> " ));
              Serial.println(blastGate[x].pwmSlot);
              Serial.print(F ("gateName   ==> " ));
              Serial.println(blastGate[x].gateName);
              Serial.print(F ("openPos    ==> " ));
              Serial.println(blastGate[x].openPos);
              Serial.print(F ("closePos   ==> "));
              Serial.println (blastGate[x].closePos);
              Serial.print(F ("openClose  ==> "));
              Serial.println(blastGate[x].gateConfig);
            }
          break;
        }
      case 'E':       // Write outlets config into EEPROMM starting at address 1100
        {             // write the end address of the structures at EEPROM.length() - 5
         eeAddress = OUTLET_ADDRESS;
          for (int x=0; x< outlets; x++)
            {
              Serial.print (".");
              EEPROM.put(eeAddress, toolSwitch[x]);
              eeAddress += sizeof(toolSwitch[x]);
            }
          if (dust.DEBUG)
            {
              Serial.print(F("\n\t\tSize of outlet structure ==> " ));
              Serial.println(sizeof(toolSwitch[2]));
              Serial.print(F ("\t\t\tToal size of outlet Array ==> " ));
              Serial.println(sizeof(toolSwitch));
            } 
          EEPROM[EEPROM.length() - 5] = eeAddress;
          Serial.println(F("\n\t\tOutlet config Loaded"));
          break;
        }
      case 'F':
        {
          eeAddress = OUTLET_ADDRESS;
          for (int x=0; x< outlets; x++)
            {
              EEPROM.get(eeAddress, toolSwitch[x]);
              Serial.print(F("\nSwitchID     ==> "));
              Serial.println(toolSwitch[x].switchID);
              Serial.print(F ("toolName       ==> "));
              Serial.println(toolSwitch[x].tool);
              Serial.print(F ("voltSensor     ==> " ));
              Serial.println(toolSwitch[x].voltSensor);
              Serial.print(F ("voltBaseline   ==> " ));
              Serial.println(toolSwitch[x].voltBaseline);
              Serial.print(F ("mainGate       ==> " ));
              Serial.println(toolSwitch[x].mainGate);
              Serial.print(F ("VCC            ==> " ));
              Serial.println(toolSwitch[x].VCC);
              Serial.print(F ("ampThresh      ==> " ));
              Serial.println(toolSwitch[x].ampThreshold);
              Serial.print(F ("voltDiff       ==> " ));
              Serial.println(toolSwitch[x].voltDiff);
              Serial.print(F ("isON           ==> " ));
              Serial.println(toolSwitch[x].isON);
              eeAddress += sizeof(toolSwitch[x]);
            }
          
          break;
        }
      case 'G':       // write wifi config into EEPROM starting at address 1500
        {             // write the end address of structure into EEPROM.length() - 2
          eeAddress = WIFI_ADDRESS;
          blynkWIFIConnect.speed = 115200;
          blynkWIFIConnect.serverPort = 80;
          strcpy(blynkWIFIConnect.BlynkConnection,"Blynk");
          strcpy (blynkWIFIConnect.pass,"67NorseSk!");
          strcpy (blynkWIFIConnect.ssid,"Everest");
          //strcpy (blynkWIFIConnect.ssid,"Everest 2.4G");
          strcpy (blynkWIFIConnect.auth, "iLO3VC0Vq6XRdTAjKTb7Y3LT6ifV8E-r");
          EEPROM.put(WIFI_ADDRESS, blynkWIFIConnect);
          eeAddress += sizeof(blynkWIFIConnect);
          EEPROM.write (EEPROM.length() - 2, eeAddress);
          break;
        }
      case 'H':
        {
          EEPROM.get(WIFI_ADDRESS, blynkWIFIConnect);
         // blynkWIFIConnect.speed = atol(blynkWIFIConnect.ESPSpeed);
         // blynkWIFIConnect.serverPort = atol(blynkWIFIConnect.port);
          Serial.print (F("\n ssid    ==> "));
          Serial.println(blynkWIFIConnect.ssid);
          Serial.print (F("pass       ==> "));
          Serial.println(blynkWIFIConnect.pass);
          Serial.print(F("server     ==> "));
          Serial.println(blynkWIFIConnect.server);
          Serial.print(F("port       ==> "));
          Serial.println(blynkWIFIConnect.port);
          Serial.print (F("serverPort ==> "));
          Serial.println(blynkWIFIConnect.serverPort);
          Serial.print(F("ESPSpeed   ==> " ));
          Serial.println(blynkWIFIConnect.ESPSpeed);
          Serial.print(F("speed      ==> " ));
          Serial.println(blynkWIFIConnect.speed);
          Serial.print(F("BlynkConn  ==> "));
          Serial.println(blynkWIFIConnect.BlynkConnection);
          Serial.print(F("auth       ==> "));
          Serial.println(blynkWIFIConnect.auth);
          break;
        }
      case 'L':
        {
          eeAddress = 0;
          do
          {
            writeDebug("address " + String(eeAddress) + "==> " + String(EEPROM[eeAddress]),1);
            eeAddress += 1;
          } while (eeAddress <= EEPROM.length());
          
          break;
        }
      case 'S':
        exportEEPROMToFIle();
        break;
      case 'R':
        recoverEEPROMFromFile();
        break;

      
    }
 
  }
