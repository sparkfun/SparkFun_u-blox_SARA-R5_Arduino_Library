/*

  SARA-R5 Example
  ===============

  u-blox AssistNow Online

  Written by: Paul Clark
  Date: January 8th 2021

  This example uses the SARA's mobile data connection to:
    * Request AssistNow Online data from the u-blox server
    * Provide assistance data to an external u-blox GNSS module over I2C (not to the one built-in to the SARA-R510M8S)

  The PDP profile is read from NVM. Please make sure you have run examples 4 & 7 previously to set up the profile.  

  You will need to have a token to be able to access Thingstream. See the AssistNow README for more details:
  https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library/tree/main/examples/AssistNow

  Update secrets.h with your AssistNow token string

  Note: this code does not use the AssistNow or CellLocate features built-in to the SARA-R510M8S.
        Those features are great but the assistance data remains 'hidden' and cannot be read and passed to an external GNSS.
        This code downloads the assistance data to the SARA-R5's internal file system where it can be accessed,
        used and re-used with an external GNSS.

  Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Licence: MIT
  Please see LICENSE.md for full details

*/

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_u-blox_SARA-R5_Arduino_Library

// Uncomment the next line to connect to the SARA-R5 using hardware Serial1
#define saraSerial Serial1

// Uncomment the next line to create a SoftwareSerial object to pass to the SARA-R5 library instead
//SoftwareSerial saraSerial(8, 9);

// Create a SARA_R5 object to use throughout the sketch
// Usually we would tell the library which GPIO pin to use to control the SARA power (see below),
// but we can start the SARA without a power pin. It just means we need to manually 
// turn the power on if required! ;-D
SARA_R5 mySARA;

// Create a SARA_R5 object to use throughout the sketch
// We need to tell the library what GPIO pin is connected to the SARA power pin.
// If you're using the MicroMod Asset Tracker and the MicroMod Artemis Processor Board,
// the pin name is G2 which is connected to pin AD34.
// Change the pin number if required.
//SARA_R5 mySARA(34);

// Create a SARA_R5 object to use throughout the sketch
// If you are using the LTE GNSS Breakout, and have access to the SARA's RESET_N pin, you can pass that to the library too
// allowing it to do an emergency shutdown if required.
// Change the pin numbers if required.
//SARA_R5 mySARA(34, 35); // PWR_ON, RESET_N

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void setup()
{
  String currentOperator = "";

  Serial.begin(115200); // Start the serial console

  // Wait for user to press key to begin
  Serial.println(F("SARA-R5 Example"));
  Serial.println(F("Wait for the SARA NI LED to light up - then press any key to begin"));
  
  while (!Serial.available()) // Wait for the user to press a key (send any serial character)
    ;
  while (Serial.available()) // Empty the serial RX buffer
    Serial.read();

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  //mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

  // For the MicroMod Asset Tracker, we need to invert the power pin so it pulls high instead of low
  // Comment the next line if required
  mySARA.invertPowerPin(true); 

  // Initialize the SARA
  if (mySARA.begin(saraSerial, 115200) )
  {
    Serial.println(F("SARA-R5 connected!"));
  }
  else
  {
    Serial.println(F("Unable to communicate with the SARA."));
    Serial.println(F("Manually power-on (hold the SARA On button for 3 seconds) on and try again."));
    while (1) ; // Loop forever on fail
  }
  Serial.println();

  // First check to see if we're connected to an operator:
  if (mySARA.getOperator(&currentOperator) == SARA_R5_SUCCESS)
  {
    Serial.print(F("Connected to: "));
    Serial.println(currentOperator);
  }
  else
  {
    Serial.print(F("The SARA is not yet connected to an operator. Please use the previous examples to connect. Or wait and retry. Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Deactivate the PSD profile - in case one is already active
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_DEACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("Warning: performPDPaction (deactivate profile) failed. Probably because no profile was active."));
  }

  // Load the PSD profile from NVM - these were saved by a previous example
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_LOAD) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (load from NVM) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Activate the profile
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_ACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (activate profile) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Start I2C. Connect to the GNSS.

  Wire.begin(); //Start I2C

  //myGNSS.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

  if (myGNSS.begin() == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("u-blox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }
  Serial.println(F("u-blox module connected"));

  myGNSS.setI2COutput(COM_TYPE_UBX); //Turn off NMEA noise

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Request the AssistNow data from the server. Data is stored in the SARA's file system.
  
  String theFilename = "assistnow_online.ubx"; // The file that will contain the AssistNow Online data
  
  if (getAssistNowOnlineData(theFilename) == false) // See SARA-R5_AssistNow_Online.ino
  {
    Serial.println(F("getAssistNowOnlineData failed! Freezing..."));
    while (1)
      ; // Do nothing more    
  }

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Read the AssistNow data from file and push it to the module

  int fileSize;
  if (mySARA.getFileSize(theFilename, &fileSize) != SARA_R5_SUCCESS)
  {
    Serial.print(F("getFileSize failed! Freezing..."));
    while (1)
      ; // Do nothing more    
  }
  
  Serial.print(F("AssistNow file size is: "));
  Serial.println(fileSize);

  // Read the data from file
  char *theAssistData = new char[fileSize];
  if (mySARA.getFileContents(theFilename, theAssistData) != SARA_R5_SUCCESS)
  {
    Serial.println(F("getFileContents failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  //prettyPrintChars(theAssistData, fileSize); // Uncomment this line to see the whole file contents (including the HTTP header)
  
  // Tell the module to return UBX_MGA_ACK_DATA0 messages when we push the AssistNow data
  myGNSS.setAckAiding(1);

  // Speed things up by setting setI2CpollingWait to 1ms
  myGNSS.setI2CpollingWait(1);

  // Push all the AssistNow data.
  //
  // pushAssistNowData is clever and will only push valid UBX-format data.
  // It will ignore the HTTP header at the start of the AssistNow file.
  //
  // We have called setAckAiding(1) to instruct the module to return MGA-ACK messages.
  // So, set the pushAssistNowData mgaAck parameter to SFE_UBLOX_MGA_ASSIST_ACK_YES.
  // Wait for up to 100ms for each ACK to arrive! 100ms is a bit excessive... 7ms is nearer the mark.
  myGNSS.pushAssistNowData((const uint8_t *)theAssistData, fileSize, SFE_UBLOX_MGA_ASSIST_ACK_YES, 100);

  // Delete the memory allocated to store the AssistNow data
  delete[] theAssistData;

  // Set setI2CpollingWait to 125ms to avoid pounding the I2C bus
  myGNSS.setI2CpollingWait(125);

  // Delete the file after use. This is optional as the SARA will automatically overwrite the file.
  // And you might want to reuse it? AssistNow Online data is valid for 2-4 hours.
  //if (mySARA.deleteFile(theFilename) != SARA_R5_SUCCESS)
  //{
  //  Serial.println(F("Warning: deleteFile failed!"));
  //}  
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void loop()
{
  // Print the UBX-NAV-PVT data so we can see how quickly the fixType goes to 3D
  
  long latitude = myGNSS.getLatitude();
  Serial.print(F("Lat: "));
  Serial.print(latitude);

  long longitude = myGNSS.getLongitude();
  Serial.print(F(" Long: "));
  Serial.print(longitude);
  Serial.print(F(" (degrees * 10^-7)"));

  long altitude = myGNSS.getAltitude();
  Serial.print(F(" Alt: "));
  Serial.print(altitude);
  Serial.print(F(" (mm)"));

  byte SIV = myGNSS.getSIV();
  Serial.print(F(" SIV: "));
  Serial.print(SIV);

  byte fixType = myGNSS.getFixType();
  Serial.print(F(" Fix: "));
  if(fixType == 0) Serial.print(F("No fix"));
  else if(fixType == 1) Serial.print(F("Dead reckoning"));
  else if(fixType == 2) Serial.print(F("2D"));
  else if(fixType == 3) Serial.print(F("3D"));
  else if(fixType == 4) Serial.print(F("GNSS + Dead reckoning"));
  else if(fixType == 5) Serial.print(F("Time only"));

  Serial.println();
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
