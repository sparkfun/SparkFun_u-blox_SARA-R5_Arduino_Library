/*

  SARA-R5 Example
  ===============

  Clock

  Written by: Paul Clark
  Date: January 3rd 2022

  This example demonstrates how to use the clock function to read the time from the SARA-R5.

  Note: the clock will be set to network time only if your network supports NITZ (Network Identity and Time Zone)

  Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Licence: MIT
  Please see LICENSE.md for full details

*/

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

void setup()
{
  Serial.begin(115200); // Start the serial console

  // Wait for user to press key to begin
  Serial.println(F("SARA-R5 Example"));
  
  //mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

  // For the MicroMod Asset Tracker, we need to invert the power pin so it pulls high instead of low
  // Comment the next line if required
  mySARA.invertPowerPin(true); 

  // Initialize the SARA
  if (mySARA.begin(saraSerial, 9600) )
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

  // Make sure automatic time zone updates are enabled
  if (mySARA.autoTimeZone(true) != SARA_R5_SUCCESS)
    Serial.println(F("Enable autoTimeZone failed!"));

  // Read and print the clock as a String
  String theTime = mySARA.clock();
  Serial.println(theTime);

  // Read and print the hour, minute, etc. separately
  uint8_t year, month, day, hour, minute, second;
  int8_t timeZone;
  if (mySARA.clock( &year, &month, &day, &hour, &minute, &second, &timeZone ) == SARA_R5_SUCCESS)
    // Note: not all Arduino boards implement printf correctly. The formatting may not be correct on some boards.
    // Note: the timeZone is defined in 15 minute increments, not hours. -28 indicates the time zone is 7 hours behind UTC/GMT.
    Serial.printf("%02d/%02d/%02d %02d:%02d:%02d %+d\r\n", year, month, day, hour, minute, second, timeZone);

}

void loop()
{
  // Nothing to do here
}
