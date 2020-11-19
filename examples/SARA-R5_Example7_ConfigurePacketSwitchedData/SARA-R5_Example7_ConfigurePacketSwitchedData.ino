/*

  SARA-R5 Example
  ===============

  Configure Packet Switched Data

  Written by: Paul Clark
  Date: November 18th 2020

  This example demonstrates how to configure Packet Switched Data on the SARA-R5.

  The earlier examples let you configure the network profile and select an operator.
  The default operator - defined in your SIM - will be allocated to "Context ID 1".
  This example defines a Packet Switched Data Profile ID, based on the selected Context ID, and then activates it.
  The profile parameters are also saved to NVM so they can be used by the next examples.
  The only complicated bit is that - strictly - we need to disconnect from the network first in order to find out what
  the defined IP type is for the chosen Context ID - as opposed to what is granted by the network. However, we'll
  take a guess that it is the default (IPv4v6). You can change this if required by editing the call to setPDPconfiguration.

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

// processPSDAction is provided to the SARA-R5 library via a 
// callback setter -- setPSDActionCallback. (See setup())
void processPSDAction(int result, IPAddress ip)
{
  Serial.println();
  Serial.print(F("PSD Action:  result: "));
  Serial.print(String(result));
  if (result == 0)
    Serial.print(F(" (success)"));
  Serial.print(F("  IP Address: \""));
  Serial.print(String(ip[0]));
  Serial.print(F("."));
  Serial.print(String(ip[1]));
  Serial.print(F("."));
  Serial.print(String(ip[2]));
  Serial.print(F("."));
  Serial.print(String(ip[3]));
  Serial.println(F("\""));
}

void setup()
{
  String currentOperator = "";

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

  int minCID = SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS; // Keep a record of the highest and lowest CIDs
  int maxCID = 0;

  Serial.println(F("The available Context IDs are:"));
  Serial.println(F("Context ID:\tAPN Name:\tIP Address:"));
  for (int cid = 0; cid < SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS; cid++)
  {
    String apn = "";
    IPAddress ip(0, 0, 0, 0);
    mySARA.getAPN(cid, &apn, &ip);
    if (apn.length() > 0)
    {
      Serial.print(cid);
      Serial.print(F("\t"));
      Serial.print(apn);
      Serial.print(F("\t"));
      Serial.println(ip);
    }
    if (cid < minCID)
      minCID = cid; // Record the lowest CID
    if (cid > maxCID)
      maxCID = cid; // Record the highest CID
  }
  Serial.println();

  Serial.println(F("Which Context ID do you want to use for your Packet Switched Data connection?"));
  Serial.println(F("Please enter the number (followed by LF / Newline): "));
  
  char c = 0;
  bool selected = false;
  int selection = 0;
  while (!selected)
  {
    while (!Serial.available()) ; // Wait for a character to arrive
    c = Serial.read(); // Read it
    if (c == '\n') // Is it a LF?
    {
      if ((selection >= minCID) && (selection <= maxCID))
      {
        selected = true;
        Serial.println("Using CID: " + String(selection));
      }
      else
      {
        Serial.println(F("Invalid CID. Please try again:"));
        selection = 0;
      }
    }
    else
    {
      selection *= 10; // Multiply selection by 10
      selection += c - '0'; // Add the new digit to selection      
    }
  }

  // Deactivate the profile
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_DEACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("Warning: performPDPaction (deactivate profile) failed. Probably because no profile was active."));
  }

  // Map PSD profile 0 to the selected CID
  if (mySARA.setPDPconfiguration(0, SARA_R5_PSD_CONFIG_PARAM_MAP_TO_CID, selection) != SARA_R5_SUCCESS)
  {
    Serial.println(F("setPDPconfiguration (map to CID) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Set the protocol type - this needs to match the defined IP type for the CID (as opposed to what was granted by the network)
  if (mySARA.setPDPconfiguration(0, SARA_R5_PSD_CONFIG_PARAM_PROTOCOL, SARA_R5_PSD_PROTOCOL_IPV4V6_V4_PREF) != SARA_R5_SUCCESS)
  // You _may_ need to change the protocol type: ----------------------------------------^
  {
    Serial.println(F("setPDPconfiguration (set protocol type) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Set a callback to process the results of the PSD Action
  mySARA.setPSDActionCallback(&processPSDAction);

  // Activate the profile
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_ACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (activate profile) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  for (int i = 0; i < 100; i++) // Wait for up to a second for the PSD Action URC to arrive
  {
    mySARA.poll(); // Keep processing data from the SARA so we can process the PSD Action
    delay(10);
  }

  // Save the profile to NVM - so we can load it again in the later examples
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_STORE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (save to NVM) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

}

void loop()
{
  mySARA.poll(); // Keep processing data from the SARA so we can process URCs from the PSD Action
}
