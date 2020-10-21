/*
  Register your SARA-R5/SIM combo on a mobile network operator
  By: Paul Clark
  SparkFun Electronics
  Date: October 20th 2020

  Please see LICENSE.md for the license information

  This example demonstrates how to initialize your MicroMod Asset Tracker Carrier Board and
  connect it to a mobile network operator (Verizon, AT&T, T-Mobile, etc.).

  Before beginning, you may need to adjust the mobile network operator (MNO)
  setting on line 83. See comments above that line to help select either
  Verizon, T-Mobile, AT&T or others.
  
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

// Map registration status messages to more readable strings
String registrationString[] =
{
  "Not registered",                         // 0
  "Registered, home",                       // 1
  "Searching for operator",                 // 2
  "Registration denied",                    // 3
  "Registration unknown",                   // 4
  "Registered, roaming",                    // 5
  "Registered, home (SMS only)",            // 6
  "Registered, roaming (SMS only)",         // 7
  "Registered, emergency service only",     // 8
  "Registered, home, CSFB not preferred",   // 9
  "Registered, roaming, CSFB not prefered"  // 10
};

// Network operator can be set to e.g.:
// MNO_SW_DEFAULT -- DEFAULT
// MNO_SIM_ICCID -- SIM DEFAULT
// MNO_ATT -- AT&T 
// MNO_VERIZON -- Verizon
// MNO_TELSTRA -- Telstra
// MNO_TMO -- T-Mobile US
// MNO_CT -- China Telecom
// MNO_VODAFONE
// MNO_STD_EUROPE
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_SIM_ICCID;

// APN -- Access Point Name. Gateway between GPRS MNO
// and another computer network. E.g. "hologram
//const String APN = "hologram";
// The APN can be omitted: this is the so-called "blank APN" setting that may be suggested by
// network operators (e.g. to roaming devices); in this case the APN string is not included in
// the message sent to the network.
const String APN = "";

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

  if (!assetTracker.setNetwork(MOBILE_NETWORK_OPERATOR))
  {
    Serial.println(F("Error setting network. Try cycling the power."));
    while (1) ;
  }
  
  if (assetTracker.setAPN(APN) == SARA_R5_SUCCESS)
  {
    Serial.println(F("APN successfully set."));
  }
  
  Serial.println(F("Network set. Ready to go!"));
  
  // RSSI: Received signal strength:
  Serial.println("RSSI: " + String(assetTracker.rssi()));
  // Registration Status
  int regStatus = assetTracker.registration();
  if ((regStatus >= 0) && (regStatus <= 10))
  {
    Serial.println("Network registration: " + registrationString[regStatus]);
  }
  
  if (regStatus > 0) {
    Serial.println(F("All set. Go to the next example!"));
  }
}

void loop() {
  // Do nothing. Now that we're registered move on to the next example.
}
