/*

  SARA-R5 Example
  ===============

  Send SMS

  Written by: Paul Clark
  Date: November 18th 2020

  This example demonstrates how to send SMS messages using the SARA.

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

  Serial.println();
  Serial.println(F("*** Set the Serial Monitor line ending to Newline ***"));
}

void loop()
{
  String destinationNumber = "";
  String message = "";
  boolean keepGoing = true;
  
  Serial.println();
  Serial.println(F("Enter the destination number (followed by LF / Newline): "));
  
  while (keepGoing)
  {
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == '\n')
      {
        keepGoing = false; // Stop if we receive a newline
      }
      else
      {
        destinationNumber += c; // Add serial characters to the destination number
      }
    }
  }
  
  keepGoing = true;
  Serial.println();
  Serial.println(F("Enter the message (followed by LF): "));
  
  while (keepGoing)
  {
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == '\n')
      {
        keepGoing = false; // Stop if we receive a newline
      }
      else
      {
        message += c; // Add serial characters to the destination number
      }
    }
  }

  // Once we receive a newline, send the text.
  Serial.println("Sending: \"" + message + "\" to " + destinationNumber);
  // Call mySARA.sendSMS(String number, String message) to send an SMS message.
  if (mySARA.sendSMS(destinationNumber, message) == SARA_R5_SUCCESS)
  {
    Serial.println(F("sendSMS was successful"));
  }
}
