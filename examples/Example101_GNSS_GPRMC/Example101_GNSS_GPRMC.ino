/*
  Use the GPS RMC sentence read feature to get position, speed, and clock data
  By: Paul Clark
  SparkFun Electronics
  Date: October 20th 2020

  Please see LICENSE.md for the license information

  This example demonstrates how to use the gpsGetRmc feature.

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
// Usually we would tell the library which GPIO pin to use to control the SARA power (see below),
// but we can start the SARA without a power pin. It just means we need to use a finger to 
// turn the power on manually if required! ;-D
SARA_R5 assetTracker;

// Create a SARA_R5 object to use throughout the sketch
// We need to tell the library what GPIO pin is connected to the SARA power pin.
// If you're using the MicroMod Asset Tracker and the MicroMod Artemis Processor Board,
// the pin number is GPIO2 which is connected to AD34. TO DO: Check this!
// Change the pin number if required.
//SARA_R5 assetTracker(34);

PositionData gps;
SpeedData spd;
ClockData clk;
boolean valid;

#define GPS_POLL_RATE 5000 // Read GPS every 2 seconds
unsigned long lastGpsPoll = 0;

void setup() {
  Serial.begin(9600);

  // Wait for user to press key in terminal to begin
  Serial.println("Press any key to begin GPS'ing");
  while (!Serial.available()) ;
  while (Serial.available()) Serial.read();

  // Initialize the SARA
  if (assetTracker.begin(saraSerial, 9600) ) {
    Serial.println(F("Asset Tracker (SARA-R5) connected!"));
  }

  // Enable the GPS's RMC sentence output. This will also turn the
  // GPS module on ((assetTracker.gpsPower(true)) if it's not already
  if (assetTracker.gpsEnableRmc(true) != SARA_R5_SUCCESS) {
    Serial.println(F("Error initializing GPS. Freezing..."));
    while (1) ;
  }
}

void loop() {
  if ((lastGpsPoll == 0) || (lastGpsPoll + GPS_POLL_RATE < millis())) {
    // Call (assetTracker.gpsGetRmc to get coordinate, speed, and timing data
    // from the GPS module. Valid can be used to check if the GPS is
    // reporting valid data
    if (assetTracker.gpsGetRmc(&gps, &spd, &clk, &valid) == SARA_R5_SUCCESS) {
      printGPS();
      lastGpsPoll = millis();
    } else {
      delay(1000); // If RMC read fails, wait a second and try again
    }
  }
}

void printGPS(void) {
  Serial.println();
  Serial.println("UTC: " + String(gps.utc));
  Serial.print("Time: ");
  if (clk.time.hour < 10) Serial.print('0'); // Print leading 0
  Serial.print(String(clk.time.hour) + ":");
  if (clk.time.minute < 10) Serial.print('0'); // Print leading 0
  Serial.print(String(clk.time.minute) + ":");
  if (clk.time.second < 10) Serial.print('0'); // Print leading 0
  Serial.print(String(clk.time.second) + ".");
  if (clk.time.ms < 10) Serial.print('0'); // Print leading 0
  Serial.println(String(clk.time.ms));
  Serial.println("Latitude: " + String(gps.lat, 7));
  Serial.println("Longitude: " + String(gps.lon, 7));
  Serial.println("Speed: " + String(spd.speed, 4) + " @ " + String(spd.cog, 4));
  Serial.println("Date: " + String(clk.date.month) + "/" + 
    String(clk.date.day) + "/" + String(clk.date.year));
  Serial.println("Magnetic variation: " + String(spd.magVar));
  Serial.println("Status: " + String(gps.status));
  Serial.println("Mode: " + String(gps.mode));
  Serial.println();
}
