/*

  SARA-R5 Example
  ===============

  Receive SMS

  Written by: Paul Clark
  Date: November 18th 2020

  This example demonstrates how to receive SMS messages using the SARA.

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

  while (Serial.available()) // Empty the serial RX buffer
    Serial.read();
}

void loop()
{
  static bool printReadMessages = true; // Print all messages once. Then only print new messages. Unless a message is deleted.
  static int previousUsed = -1; // Store the previous number of used memory locations
  
  // Read the number of used and total messages
  int used;
  int total;
  if (mySARA.getPreferredMessageStorage(&used, &total) != SARA_R5_SUCCESS)
  {
    Serial.println(F("An error occurred when trying to read ME memory!"));
  }
  else
  {
    if ((used != previousUsed) || printReadMessages) // Has a new message arrived? Or was the delete menu opened?
    {
      Serial.print(F("\r\nNumber of used memory locations: "));
      Serial.println(used);
      Serial.print(F("Total number of memory locations: "));
      Serial.println(total);
      Serial.println();

      int memoryLocation = 1;
      int foundMessages = 0;
      // Keep reading until we find all the messages or we reach the end of the memory
      while ((foundMessages < used) && (memoryLocation <= total))
      {
        String unread = "";
        String from = "";
        String dateTime = "";
        String message = "";
        // Read the message from this location. Reading from empty message locations returns an ERROR
        // unread can be: "REC UNREAD", "REC READ", "STO UNSENT", "STO SENT"
        // If the location is empty, readSMSmessage will return a SARA_R5_ERROR_UNEXPECTED_RESPONSE
        if (mySARA.readSMSmessage(memoryLocation, &unread, &from, &dateTime, &message) == SARA_R5_SUCCESS)
        {
          if (printReadMessages || (unread == "REC UNREAD")) 
          {
            Serial.print(F("Message location: "));
            Serial.println(memoryLocation);
            Serial.print(F("Status: "));
            Serial.println(unread);
            Serial.print(F("Originator: "));
            Serial.println(from);
            Serial.print(F("Date and time: "));
            Serial.println(dateTime);
            Serial.println(message);
            Serial.println();
          }
          foundMessages++; // We found a message
        }
        memoryLocation++; // Move on to the next memory location
      }

      printReadMessages = false;
      previousUsed = used; // Update previousUsed

      Serial.println(F("Waiting for a new message..."));
      Serial.println();
      Serial.println(F("Hit any key to delete a message..."));
      Serial.println();
    }
  }

  int delayCount = 0;
  while (delayCount < 5000)
  {
    delay(1); // Delay for five seconds, unless the user presses a key
    delayCount++;

    if (Serial.available())
    {
      Serial.println(F("To delete a single message:                        enter its location followed by LF / Newline"));
      Serial.println(F("To delete all read messages:                       enter r followed by LF / Newline"));
      Serial.println(F("To delete all read and sent messages:              enter s followed by LF / Newline"));
      Serial.println(F("To delete all read, sent and unsent messages:      enter u followed by LF / Newline"));
      Serial.println(F("To delete all messages, including unread messages: enter a followed by LF / Newline"));
      Serial.println(F("To exit:                                           enter LF / Newline"));

      Serial.read(); // Read and discard the char that opened the menu

      int location = 0;
      bool selected = false;
      while (!selected)
      {
        while (!Serial.available()) ; // Wait for a character to arrive
        char c = Serial.read(); // Read it
        if (c == '\n') // Is it a LF?
        {
          if ((location >= 1) && (location <= total)) // Delete a single message at location
          {
            if (mySARA.deleteSMSmessage(location) == SARA_R5_SUCCESS)
            {
              Serial.println(F("\r\nMessage deleted!\r\n"));
              printReadMessages = true;
            }
            else
            {
              Serial.println(F("\r\nMessage not deleted!\r\n"));
            }
          }
          else if (location == 1001) // r
          {
            if (mySARA.deleteReadSMSmessages() == SARA_R5_SUCCESS)
            {
              Serial.println(F("\r\nRead messages deleted!\r\n"));
              printReadMessages = true;
            }
            else
            {
              Serial.println(F("\r\nMessages not deleted!\r\n"));
            }
          }
          else if (location == 1002) // s
          {
            if (mySARA.deleteReadSentSMSmessages() == SARA_R5_SUCCESS)
            {
              Serial.println(F("\r\nRead and sent messages deleted!\r\n"));
              printReadMessages = true;
            }
            else
            {
              Serial.println(F("\r\nMessages not deleted!\r\n"));
            }
          }
          else if (location == 1003) // u
          {
            if (mySARA.deleteReadSentUnsentSMSmessages() == SARA_R5_SUCCESS)
            {
              Serial.println(F("\r\nRead, sent and unsent messages deleted!\r\n"));
              printReadMessages = true;
            }
            else
            {
              Serial.println(F("\r\nMessages not deleted!\r\n"));
            }
          }
          else if (location == 1004) // a
          {
            if (mySARA.deleteAllSMSmessages() == SARA_R5_SUCCESS)
            {
              Serial.println(F("\r\nAll messages deleted!\r\n"));
              printReadMessages = true;
            }
            else
            {
              Serial.println(F("\r\nMessages not deleted!\r\n"));
            }
          }
          else
            Serial.println(F("\r\nExit...\r\n"));
          selected = true;
        }
        else if ((c >= '0') && (c <= '9'))
        {
          location *= 10; // Multiply by 10
          location += c - '0'; // Add the digit
        }
        else if (c == 'r')
        {
          location = 1001;
        }
        else if (c == 's')
        {
          location = 1002;
        }
        else if (c == 'u')
        {
          location = 1003;
        }
        else if (c == 'a')
        {
          location = 1004;
        }
      }

      delayCount = 5000;
    }
  }
}
