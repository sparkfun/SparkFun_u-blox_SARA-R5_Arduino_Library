/*

  SARA-R5 Example
  ===============

  Identification
  
  Written by: Paul Clark
  Date: November 18th 2020

  This example demonstrates how to read the SARA's:
    Manufacturer identification
    Model identification
    Firmware version identification
    Product Serial No.
    IMEI identification
    IMSI identification
    SIM CCID
    Subscriber number
    Capabilities
    SIM state

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

// Map SIM states to more readable strings
String simStateString[] =
{
  "Not present",      // 0
  "PIN needed",       // 1
  "PIN blocked",      // 2
  "PUK blocked",      // 3
  "Not operational",  // 4
  "Restricted",       // 5
  "Operational"       // 6
};

// processSIMstate is provided to the SARA-R5 library via a 
// callback setter -- setSIMstateReadCallback. (See setup())
void processSIMstate(SARA_R5_sim_states_t state)
{
  Serial.println();
  Serial.print(F("SIM state:           "));
  Serial.print(String(state));
  Serial.println();
}

void setup()
{
  Serial.begin(115200); // Start the serial console

  // Wait for user to press key to begin
  Serial.println(F("SARA-R5 Example"));
  Serial.println(F("Press any key to begin"));
  
  while (!Serial.available()) // Wait for the user to press a key (send any serial character)
    ;
  while (Serial.available()) // Empty the serial RX buffer
    Serial.read();

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

  Serial.println("Manufacturer ID:     " + String(mySARA.getManufacturerID()));
  Serial.println("Model ID:            " + String(mySARA.getModelID()));
  Serial.println("Firmware Version:    " + String(mySARA.getFirmwareVersion()));
  Serial.println("Product Serial No.:  " + String(mySARA.getSerialNo()));
  Serial.println("IMEI:                " + String(mySARA.getIMEI()));
  Serial.println("IMSI:                " + String(mySARA.getIMSI()));
  Serial.println("SIM CCID:            " + String(mySARA.getCCID()));
  Serial.println("Subscriber No.:      " + String(mySARA.getSubscriberNo()));
  Serial.println("Capabilities:        " + String(mySARA.getCapabilities()));

  // Set a callback to return the SIM state once requested
  mySARA.setSIMstateReportCallback(&processSIMstate);
  // Now enable SIM state reporting for states 0 to 6 (by setting the reporting mode LSb)
  if (mySARA.setSIMstateReportingMode(1) == SARA_R5_SUCCESS)
    Serial.println("SIM state reports requested...");
  // You can disable the SIM staus reports again by calling assetTracker.setSIMstateReportingMode(0)
}

void loop()
{
  mySARA.poll(); // Keep processing data from the SARA so we can extract the SIM status
}
