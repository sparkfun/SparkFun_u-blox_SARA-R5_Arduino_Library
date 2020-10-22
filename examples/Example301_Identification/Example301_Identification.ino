/*
  Read the identification and version information from the SARA-R5
  By: Paul Clark
  SparkFun Electronics
  Date: October 20th 2020

  Please see LICENSE.md for the license information

  This example demonstrates how to read the:
    Manufacturer identification
    Model identification
    Firmware version identification
    Product Serial No.
    IMEI identification
    IMSI identification
    SIM CCID
    Subscriber number
    Capabilities

  This code is intend to run on the MicroMod Asset Tracker Carrier Board
  using (e.g.) the MicroMod Artemis Processor Board

  Select the SparkFun RedBoard Artemis ATP from the SparkFun Apollo3 boards
  
*/

//Click here to get the library: http://librarymanager/All#SparkFun_u-blox_SARA-R5_Arduino_Library
#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h>

// Uncomment the next line to connect to the SARA-R5 using hardware Serial1
#define saraSerial Serial1

// Uncomment the next line to create a SoftwareSerial object to pass to the SARA-R5 library instead
//SoftwareSerial saraSerial(8, 9);

// Create a SARA_R5 object to use throughout the sketch
SARA_R5 assetTracker;

void setup() {
  Serial.begin(9600);

  Serial.println(F("Initializing the Asset Tracker (SARA-R5)..."));
  Serial.println(F("...this may take ~25 seconds if the SARA is off."));
  Serial.println(F("...it may take ~5 seconds if it just turned on."));

  // Initialize the SARA
  if (assetTracker.begin(saraSerial, 9600))
  {
    Serial.println(F("Asset Tracker (SARA-R5) connected!"));
  }
  else
  {
    Serial.println(F("Unable to communicate with the SARA."));
    Serial.println(F("Manually power-on (hold POWER for 3 seconds) on and try again."));
    while (1) ; // Loop forever on fail
  }
  Serial.println();

  Serial.println("Manufacturer ID:     " + String(assetTracker.getManufacturerID()));
  Serial.println("Model ID:            " + String(assetTracker.getModelID()));
  Serial.println("Firmware Version:    " + String(assetTracker.getFirmwareVersion()));
  Serial.println("Product Serial No.:  " + String(assetTracker.getSerialNo()));
  Serial.println("IMEI:                " + String(assetTracker.getIMEI()));
  Serial.println("IMSI:                " + String(assetTracker.getIMSI()));
  Serial.println("SIM CCID:            " + String(assetTracker.getCCID()));
  Serial.println("Subscriber No.:      " + String(assetTracker.getSubscriberNo()));
  Serial.println("Capabilities:        " + String(assetTracker.getCapabilities()));
}

void loop() {
  // Do nothing. Now that we're registered move on to the next example.
}
