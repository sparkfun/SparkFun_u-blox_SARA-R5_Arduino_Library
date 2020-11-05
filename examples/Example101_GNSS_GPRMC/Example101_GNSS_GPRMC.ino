/*
  Use the GPS RMC sentence read feature to get position, speed, and clock data
  By: Paul Clark
  SparkFun Electronics
  Date: October 20th 2020

  Please see LICENSE.md for the license information

  This example demonstrates how to use the gpsGetRmc feature.

*/

#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_u-blox_SARA-R5_Arduino_Library

// Uncomment the next line to connect to the SARA-R5 using hardware Serial1
#define saraSerial Serial1

// Uncomment the next line to create a SoftwareSerial object to pass to the SARA-R5 library instead
//SoftwareSerial saraSerial(8, 9);

// Create a SARA_R5 object to use throughout the sketch
// Usually we would tell the library which GPIO pin to use to control the SARA power (see below),
// but we can start the SARA without a power pin. It just means we need to use a finger to 
// turn the power on manually if required! ;-D
SARA_R5 mySARA;

// Create a SARA_R5 object to use throughout the sketch
// We need to tell the library what GPIO pin is connected to the SARA power pin.
// If you're using the MicroMod Asset Tracker and the MicroMod Artemis Processor Board,
// the pin name is G2 which is connected to pin AD34.
// Change the pin number if required.
//SARA_R5 mySARA(34);

// If you're using the MicroMod Asset Tracker then we need to define which pin controls the active antenna power.
// If you're using the MicroMod Artemis Processor Board, the pin name is G6 which is connected to pin D14.
// Change the pin number if required.
//const int antennaPowerEnablePin = 14; // Uncomment this line to define the pin number for the antenna power enable
const int antennaPowerEnablePin = -1; // Uncomment this line if your board does not have an antenna power enable

PositionData gps;
SpeedData spd;
ClockData clk;
boolean valid;

#define GPS_POLL_RATE 5000 // Read GPS every 2 seconds
unsigned long lastGpsPoll = 0;

void setup() {
  Serial.begin(115200);

  // Wait for user to press key in terminal to begin
  Serial.println(F("SARA-R5 Example"));
  Serial.println(F("Press any key to begin GPS'ing"));
  while (!Serial.available()) ;
  while (Serial.available()) Serial.read();

  //mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages

  // For the Asset Tracker, we need to invert the power pin so it pulls high instead of low
  // Comment the next line if required
  mySARA.invertPowerPin(true); 

  // Initialize the SARA
  if (mySARA.begin(saraSerial, 9600) ) {
    Serial.println(F("SARA-R5 connected!"));
  }

  // Enable the GPS's RMC sentence output. This will also turn the
  // GPS module on ((mySARA.gpsPower(true)) if it's not already
  if (mySARA.gpsEnableRmc(true) != SARA_R5_SUCCESS) {
    Serial.println(F("Error initializing GPS. Freezing..."));
    while (1) ;
  }

  // Enable power for the GNSS active antenna
  enableGNSSAntennaPower();
}

void loop() {
  if ((lastGpsPoll == 0) || (lastGpsPoll + GPS_POLL_RATE < millis()))
  {
    // Call (mySARA.gpsGetRmc to get coordinate, speed, and timing data
    // from the GPS module. Valid can be used to check if the GPS is
    // reporting valid data
    if (mySARA.gpsGetRmc(&gps, &spd, &clk, &valid) == SARA_R5_SUCCESS)
    {
      printGPS();
      lastGpsPoll = millis();
    }
    else
    {
      delay(1000); // If RMC read fails, wait a second and try again
    }
  }
}

void printGPS(void)
{
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
  Serial.println("Date (MM/DD/YY): " + String(clk.date.month) + "/" + 
    String(clk.date.day) + "/" + String(clk.date.year));
  Serial.println("Magnetic variation: " + String(spd.magVar));
  Serial.println("Status: " + String(gps.status));
  Serial.println("Mode: " + String(gps.mode));
  Serial.println();
}

// Disable power for the GNSS active antenna
void disableGNSSAntennaPower()
{
  if (antennaPowerEnablePin >= 0)
  {
    pinMode(antennaPowerEnablePin, OUTPUT);
    digitalWrite(antennaPowerEnablePin, LOW);
  }
}
// Enable power for the GNSS active antenna
void enableGNSSAntennaPower()
{
  if (antennaPowerEnablePin >= 0)
  {
    pinMode(antennaPowerEnablePin, OUTPUT);
    digitalWrite(antennaPowerEnablePin, HIGH);
  }
}
