/*

  SARA-R5 Example
  ===============

  u-blox AssistNow Offline

  Written by: Paul Clark
  Date: January 8th 2021

  This example uses the SARA's mobile data connection to:
    * Request AssistNow Offline data from the u-blox server
    * Provide assistance data to an external u-blox GNSS module over I2C (not to the one built-in to the SARA-R510M8S)

  The PDP profile is read from NVM. Please make sure you have run examples 4 & 7 previously to set up the profile.

  This example uses UTC time from the SARA-R5's Real Time Clock to select the AssistNow Offline data for today.

  You will need to have a token to be able to access Thingstream. See the AssistNow README for more details:
  https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library/tree/main/examples/AssistNow

  Update secrets.h with your AssistNow token string

  Note: this code does not use the AssistNow or CellLocate features built-in to the SARA-R510M8S.
        Those features are great but the assistance data remains 'hidden' and cannot be read and passed to an external GNSS.
        This code downloads the assistance data to the SARA-R5's internal file system where it can be accessed,
        used and re-used with an external GNSS.

  Note: AssistNow Offline is not supported by the ZED-F9P! "The ZED-F9P supports AssistNow Online only."

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

  // Disable the automatic time zone so we can use UTC. We need to do this _before_ .begin
  mySARA.autoTimeZoneForBegin(false);

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

  //Get the time from an NTP server and use it to set the clock. See SARA-R5_NTP.ino
  uint8_t y, mo, d, h, min, s;
  bool success = getNTPTime(&y, &mo, &d, &h, &min, &s);
  if (!success)
  {
    Serial.println(F("getNTPTime failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  //Set the SARA's RTC. Set the time zone to zero so the clock uses UTC
  if (mySARA.setClock(y, mo, d, h, min, s, 0) != SARA_R5_SUCCESS)
  {
    Serial.println(F("setClock failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }
  
  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Read and print the clock as a String
  Serial.print(F("The UTC time is: "));
  String theTime = mySARA.clock();
  Serial.println(theTime);

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Start I2C. Connect to the GNSS.

  Wire.begin(); //Start I2C

  // Uncomment the next line to enable the 'major' GNSS debug messages on Serial so you can see what AssistNow data is being sent
  //myGNSS.enableDebugging(Serial, true);

  if (myGNSS.begin() == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("u-blox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }
  Serial.println(F("u-blox module connected"));

  myGNSS.setI2COutput(COM_TYPE_UBX); //Turn off NMEA noise

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Request the AssistNow data from the server. Data is stored in the SARA's file system.
  
  String theFilename = "assistnow_offline.ubx"; // The file that will contain the AssistNow Offline data

///* Comment from here ===>
  if (getAssistNowOfflineData(theFilename) == false) // See SARA-R5_AssistNow_Offline.ino
  {
    Serial.println(F("getAssistNowOfflineData failed! Freezing..."));
    while (1)
      ; // Do nothing more    
  }
//*/ // <=== to here if you want to re-use the existing AssistNow Offline data

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Read the AssistNow data from file

  int fileSize;
  if (mySARA.getFileSize(theFilename, &fileSize) != SARA_R5_SUCCESS)
  {
    Serial.print(F("getFileSize failed! Freezing..."));
    while (1)
      ; // Do nothing more    
  }
  
  Serial.print(F("File size is: "));
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

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // Read UTC time from the SARA-R5's RTC

  uint8_t year, month, day, hours, minutes, seconds;
  int8_t timeZone; // Should be zero for UTC
  if (mySARA.clock(&year, &month, &day, &hours, &minutes, &seconds, &timeZone) != SARA_R5_SUCCESS)
  {
    Serial.println(F("clock failed! Freezing..."));
    while (1)
      ; // Do nothing more        
  }

  if (year < 22)
  {
    Serial.print(F("WARNING: the SARA-R5's clock time is: "));
    Serial.print(mySARA.clock());
    Serial.println(F(". Did you forget to set the clock to UTC?"));       
  }

  if (timeZone != 0)
  {
    Serial.print(F("WARNING: the SARA-R5's time zone is: "));
    if (timeZone >= 0) Serial.println(F("+"));
    Serial.print(timeZone);
    Serial.println(F(". Did you forget to set the clock to UTC?"));
  }

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // Find where the AssistNow data for today starts and ends

  size_t todayStart = 0; // Default to sending all the data
  size_t tomorrowStart = (size_t)fileSize;
  
  if (fileSize > 0)
  {
    // Find the start of today's data
    todayStart = myGNSS.findMGAANOForDate((const uint8_t *)theAssistData, (size_t)fileSize, year + 2000, month, day);
    if (todayStart < (size_t)fileSize)
    {
      Serial.print(F("Found the data for today starting at location "));
      Serial.println(todayStart);
    }
    else
    {
      Serial.println("Could not find the data for today. This will not work well. The GNSS needs help to start up quickly.");
    }
    
    // Find the start of tomorrow's data
    tomorrowStart = myGNSS.findMGAANOForDate((const uint8_t *)theAssistData, (size_t)fileSize, year + 2000, month, day, 1);
    if (tomorrowStart < (size_t)fileSize)
    {
      Serial.print(F("Found the data for tomorrow starting at location "));
      Serial.println(tomorrowStart);
    }
    else
    {
      Serial.println("Could not find the data for tomorrow. (Today's data may be the last?)");
    }
  }

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // Push the RTC time to the module

  mySARA.clock(&year, &month, &day, &hours, &minutes, &seconds, &timeZone); // Refresh the time

  // setUTCTimeAssistance uses a default time accuracy of 2 seconds which should be OK here.
  // Have a look at the library source code for more details.
  myGNSS.setUTCTimeAssistance(year + 2000, month, day, hours, minutes, seconds); // Push it to the GNSS module

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // Push the AssistNow data for today to the module - without the time

  if ((tomorrowStart - todayStart) > 0)
  {
    // Tell the module to return UBX_MGA_ACK_DATA0 messages when we push the AssistNow data
    myGNSS.setAckAiding(1);

    // Speed things up by setting setI2CpollingWait to 1ms
    myGNSS.setI2CpollingWait(1);

    // Push the AssistNow data for today - without the time
    //
    // pushAssistNowData is clever and will only push valid UBX-format data.
    // It will ignore the HTTP header at the start of the AssistNow file.
    myGNSS.pushAssistNowData(todayStart, true, (const uint8_t *)theAssistData, tomorrowStart - todayStart, SFE_UBLOX_MGA_ASSIST_ACK_YES, 100);

    // Delete the memory allocated to store the AssistNow data
    delete[] theAssistData;

  // Set setI2CpollingWait to 125ms to avoid pounding the I2C bus
    myGNSS.setI2CpollingWait(125);
  }

  //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // Delete the file after use. This is optional as the SARA will automatically overwrite the file.
  // And you might want to reuse it? AssistNow Offline data is valid for up to 35 days.

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
