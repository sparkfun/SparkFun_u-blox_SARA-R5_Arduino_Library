/*
  Send an SMS with the SARA-R5
  By: Paul Clark
  SparkFun Electronics
  Date: October 20th 2020

  Please see LICENSE.md for the license information

  This example demonstrates how to send an SMS message with your MicroMod Asset Tracker Carrier Board.

  Before uploading, set the DESTINATION_NUMBER constant to the 
  phone number you want to send a text message to.
  (Don't forget to add an internation code (e.g. 1 for US)).

  Once programmed, open the serial monitor, set the baud rate to 9600,
  and type a message to be sent via SMS.
  
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
// We need to tell the library what GPIO pin is connected to the SARA power pin.
// If you're using the MicroMod Asset Tracker and the MicroMod Artemis Processor Board,
// the pin number is GPIO2 which is connected to AD34. TO DO: Check this!
SARA_R5 assetTracker(34);

// Set the cell phone number to be texted
String DESTINATION_NUMBER = "11234567890";

void setup() {
  Serial.begin(9600);

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

  Serial.println(F("Type a message. Send a Newline (\\n) to send it..."));
}

void loop() {
  static String message = "";
  if (Serial.available())
  {
    char c = Serial.read();
    // Read a message until a \n (newline) is received
    if (c == '\n')
    {
      // Once we receive a newline. send the text.
      Serial.println("Sending: \"" + String(message) + "\" to " + DESTINATION_NUMBER);
      // Call assetTracker.sendSMS(String number, String message) to send an SMS
      // message.
      assetTracker.sendSMS(DESTINATION_NUMBER, message);
      message = ""; // Clear message string
    }
    else
    {
      message += c; // Add last character to message
    }
  }
}
