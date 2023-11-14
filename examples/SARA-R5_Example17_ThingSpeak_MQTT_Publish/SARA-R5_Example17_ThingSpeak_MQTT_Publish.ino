/*

  SARA-R5 Example
  ===============

  ThingSpeak (MQTT)

  Written by: Paul Clark
  Date: November 14th 2023

  This example uses the SARA's mobile data connection to publish random temperatures on ThingSpeak using MQTT.
  https://thingspeak.com/

  See: https://uk.mathworks.com/help/thingspeak/mqtt-basics.html#responsive_offcanvas

  You will need to:
    Create a ThingSpeak User Account – https://thingspeak.com/login
    Create a new Channel by selecting Channels, My Channels, and then New Channel
    Note the Channel ID and copy&paste it into myChannelID below
    Click on the Devices drop-down at the top of the screen and select MQTT
    Create a new MQTT Device using "Add a new device". Give it a name
    Authorize the New Channel you created above, then Add Channel, then Add Device
    Copy the Username, Client ID and password into myUsername, myClientID and myPassword below
  The random temperature reading will be added to the channel as "Field 1"

  Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Licence: MIT
  Please see LICENSE.md for full details

*/

#include <IPAddress.h>

// ThingSpeak via MQTT Publish

String brokerName = "mqtt3.thingspeak.com"; // MQTT Broker

const int brokerPort = 1883; // MQTT port (TCP, no encryption)

String myUsername = "OAAxOjYHIwooJykfCiYoEx0";

String myClientID = "OAAxOjYHIwooJykfCiYoEx0";

String myPassword = "RqY/6L246tULLVWUzCqJBX/V";

String myChannelID = "1225363";

// SARA-R5

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

// processMQTTcommandResult is provided to the SARA-R5 library via a 
// callback setter -- setMQTTCommandCallback. (See the end of setup())
void processMQTTcommandResult(int command, int result)
{
  Serial.println();
  Serial.print(F("MQTT Command Result:  command: "));
  Serial.print(command);
  Serial.print(F("  result: "));
  Serial.print(result);
  if (result == 0)
    Serial.print(F(" (fail)"));
  if (result == 1)
    Serial.print(F(" (success)"));
  Serial.println();

  // Get and print the most recent MQTT protocol error
  int error_class;
  int error_code;
  mySARA.getMQTTprotocolError(&error_class, &error_code);
  Serial.print(F("Most recent MQTT protocol error:  class: "));
  Serial.print(error_class);
  Serial.print(F("  code: "));
  Serial.print(error_code);
  if (error_code == 0)
    Serial.print(F(" (no error)"));
  Serial.println();
}

void setup()
{
  delay(1000);
  
  String currentOperator = "";

  Serial.begin(115200); // Start the serial console

  // Wait for user to press key to begin
  Serial.println(F("SARA-R5 Example"));
  Serial.println(F("Wait for the SARA NI LED to light up - then press any key to begin"));
  
  while (!Serial.available()) // Wait for the user to press a key (send any serial character)
    ;
  while (Serial.available()) // Empty the serial RX buffer
    Serial.read();

  mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

  // For the MicroMod Asset Tracker, we need to invert the power pin so it pulls high instead of low
  // Comment the next line if required
  mySARA.invertPowerPin(true); 

  // Initialize the SARA
  if (mySARA.begin(saraSerial, 115200) )
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

  // Deactivate the PSD profile - in case one is already active
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_DEACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("Warning: performPDPaction (deactivate profile) failed. Probably because no profile was active."));
  }

  // Load the PSD profile from NVM - these were saved by a previous example
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_LOAD) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (load from NVM) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Activate the PSD profile
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_ACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (activate profile) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Set up the MQTT command callback
  mySARA.setMQTTCommandCallback(processMQTTcommandResult);

  // Disable the security profile
  mySARA.setMQTTsecure(false);

  // Set Client ID
  mySARA.setMQTTclientId(myClientID);

  // Set the broker name and port
  mySARA.setMQTTserver(brokerName, brokerPort);

  // Set the user name and password
  mySARA.setMQTTcredentials(myUsername, myPassword);

  // Connect
  if (mySARA.connectMQTT() == SARA_R5_SUCCESS)
    Serial.println(F("MQTT connected"));
  else
    Serial.println(F("MQTT failed to connect!"));
}

void loop()
{
  float temperature = ((float)random(2000,3000)) / 100.0; // Create a random temperature between 20 and 30

  // Send data using MQTT Publish
  String Topic = "channels/" + myChannelID + "/publish";
  String DataField = "field1=" + String(temperature) + "&status=MQTTPUBLISH";

  Serial.print(F("Publishing a temperature of "));
  Serial.print(String(temperature));
  Serial.println(F(" to ThingSpeak"));
        
  // Publish the text message
  mySARA.mqttPublishTextMsg(Topic, DataField.c_str(), 0, true); // This defaults to QoS = 0, and retain = false 

  // Wait for 20 seconds
  for (int i = 0; i < 20000; i++)
  {
    mySARA.poll(); // Keep processing data from the SARA
    delay(1);
  }
}
