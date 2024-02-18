 #define    SD_WRITE 53
 #define    SET_CONFIG 0
 #define    GATE_ADDRESS 100
 #define    OUTLET_ADDRESS  1400
 #define    WIFI_ADDRESS    2900
  
    boolean     collectorIsOn = false;  
    boolean     toolON = false;
    byte        initiation = true;
    int         blynkSelectedGate;          // used to store the selected gate in the Blynk menu item
    boolean     manualOveride = false;
    int         inspectionPin;
    int         highPos;                    // used as the open max position in the config screen of the Blynk app
    int         lowPos;                     // used as the closed max position in the config screen of the Blynk app
    boolean     bGo = false;
    char        cypherKey[17] = {'\0'};
    SdFat       sdCard;
    SdBaseFile  fConfig;
    char     delim[2] = {'~', '\0'};         // delimeter for parsing the servo config
    char      decrDelim[2] = {char(222), '\0'};
    char     delimCheck = char(222);
    char     gateCheck = char(174);
    char     sectDelim = '$';
    char     gateSectDelim = '>';
    char     outletSectDelim = '[';
    char     wifiSectDelim = ']';
    char     checker;
    int      gates;
    int      outlets;
    int      selectedOutlet;
    char     fileName[32] = "DustGatesDefinition 53.cfg";

  

 /***********************************************
   *   Structure for the voltage sensor definition.  each instance
   *   variable is read and filled from the config file
   ***********************************************/
    struct      voltageSensor {
              int       switchID = -1;
              char      tool[32] = {'\0'};
              int       voltSensor;
              float     voltBaseline = 0;
              int       mainGate;
              float     VCC;
              float     ampThreshold;
              float     voltDiff;
              int       isON = false;
        }  toolSwitch[16];    
    
  /***********************************************
   *   Structure for the blast gate definition.  each instance
   *   variable is read and filled from the config file
   ***********************************************/
      struct    gateDefinition {
          int    gateID = -1;
          int    pwmSlot;
          char   gateName[32] = {'\0'};        // name of gate
          int    openPos;                   // min servo open position
          int    closePos;                   // max servo closed position
          bool   openClose;                  // is the gate open or closed?
          char   gateConfig [32] = {'\0'};      // string containing instructions on which gates to open and close
        }   blastGate[20];

  /************************************************
   *   structure to store the variables from the configuration to start the 
   *    BLYNK server.   
   *        ssid   --   wifi Connection
   *        pass   --   Passcode to the wifi
   *        server --   IP address of the server, or blynk.com
   *        port   --   listening port on the above server (defulat to 80 if connecting to blynk cloud)
   *        auth   --   Blynk app authentication key to connect to this app
   *******************************************************/
      struct  wifistruc {
        char     ssid[32] = {'\0'};
        char     pass[32] = {'\0'};
        char     server[48] = {'\0'};
        char     port[16] = {'\0'};
        long     serverPort = 80;
        char     ESPSpeed[16] = {'\0'};
        long     speed = 115200;
        char     BlynkConnection[16] = {'\0'};
        char     auth[40] = {'\0'};
      }  blynkWIFIConnect;
  /***************************************
   *    structure to hold the base configuration variables
   *    don't know if i am goign to use the structure in the main program
   *    but for writing to EEPROM, it's easier to write the structure instead
   *    of a whole bunch of variables.
   * *******************************************************/

      struct systemConfig {
        int         servoCount;   
        int         NUMBER_OF_TOOLS = 0;
        int         NUMBER_OF_GATES = 0;
        int         DC_spindown;
        int         dustCollectionRelayPin;     // the pin which will trigger the dust collector power, turns it on and off by high or low setting on the pin
        int         manualSwitchPin;
        float       mVperAmp;                   // use 100 for 20A Module and 66 for 30A Module
        long        debounce;
        boolean     DEBUG = true;
      }  dust;

/***************************************
      stripComma
          function to remove the commas from the comma delimited list.
 ******************************************/
void stripComma (char source[48], char destination[48], bool overWrite)
{
  int    temp = 0;
  int    holder = 0;
  while (source[temp] != '\0' && temp < sizeof(source))
    if (source[temp] != ',')
    {
      destination[holder] = source[temp];
      holder++;
      temp++;
    }
    else
      temp++;
  if (overWrite)
    strcpy (source, destination);
}

/**********************
   My easy way to write to the Serial debugger screen
**********************/

void writeDebug (String toWrite, byte LF)
{

  if (LF)
  {
    Serial.println(toWrite);
  }
  else
  {
    Serial.print(toWrite);
  }

}