/*

  SARA-R5 Example
  ===============

  Set Clock With NTP

  Written by: Paul Clark
  Date: January 9th 2022

  This example demonstrates how to set the SARA-R5's internal Real Time Clock using NTP.
  When the SARA-R5 registers on a network, it will set its clock automatically if:
    Automatic time zone is enabled
    Your network supports NITZ (Network Identity and Time Zone)
  But for things like AssistNow Offline, it is convenient to have the SARA's RTC set to UTC.
  Then the clock can be used to select the AssistNow data for now/today without time zone headaches.
  This example shows how to:
    Disable the automatic time zone (using autoTimeZoneForBegin before .begin)
    Set the SARA's clock to UTC using NTP (see SARA-R5_NTP.ino)

  The PDP profile is read from NVM. Please make sure you have run examples 4 & 7 previously to set up the profile.  

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
}

void loop()
{
  // Nothing to do here
}
