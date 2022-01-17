/*

  SARA-R5 Example
  ===============

  Socket "Ping Pong" - Binary UDP Data Transfers on multiple sockets

  Written by: Paul Clark
  Date: December 30th 2021

  This example demonstrates how to ping-pong binary data from a SARA-R5 to UDP Echo Server using multiple UDP sockets.

  The PDP profile is read from NVM. Please make sure you have run examples 4 & 7 previously to set up the profile.
  
  The code asks for the IP Address to ping
  The code then opens multiple UDP sockets using the base port number UDP_PORT_BASE
  The code sends an initial "Ping" (Binary 0x00, 0x01, 0x02, 0x03) on all sockets using socketWriteUDP (+USOST)
  The code polls continuously. When a +UUSORF URC message is received, data is read and passed to the socketReadCallbackPlus.
  When "Pong" (Binary 0x04, 0x05, 0x06, 0x07) is received by the callback, the code sends "Ping" in reply
  The Ping-Pong repeats 20 times
  The socket is closed after the 20th Ping is sent

  This example needs an external UDP Echo Server. E.g. Python running on your home computer.
  Here's a quick how-to (assuming you are familiar with Python):
    Open up a Python editor on your computer
    Copy the Multi_UDP_Echo.py from the GitHub repo Utils folder: https://github.com/sparkfun/SparkFun_u-blox_SARA-R5_Arduino_Library/tree/main/Utils
    Log in to your router
    Find your computer's local IP address (usually 192.168.0.something)
    Go into your router's Security / Port Forwarding settings:
      Create a new port forwarding rule
      The IP address is your local IP address
      Set the local port range to 1200-1206 (if you changed UDP_PORT_BASE, use that port number instead)
      Set the external port range to 1200-1206
      Set the protocol to UDP (or BOTH)
      Enable the rule
    This will open up a direct connection from the outside world, through your router, to ports 1200-1206 on your computer
      Remember to lock them down again when you're done!
    Edit the Python code and change 'HOST' to your local IP number:
      HOST = '192.168.0.nnn'
    Run the Python code
    Ask Google for your computer's public IP address:
      Google "what is my IP address"
    Run this code
    Enter your computer's public IP address when asked
    Sit back and watch the ping-pong!
    The code will stop after 20 Pings+Echos and 20 Pongs+Echos on each port
      On 5 ports, that's 400 UDP transfers in total!

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

// Create a SARA_R5 object to use throughout the sketch
// If you are using the LTE GNSS Breakout, and have access to the SARA's RESET_N pin, you can pass that to the library too
// allowing it to do an emergency shutdown if required.
// Change the pin numbers if required.
//SARA_R5 mySARA(34, 35); // PWR_ON, RESET_N

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

const int numConnections = 5; // How many sockets do you want to use? Max is 7.
unsigned int UDP_PORT_BASE = 1200; // Change this if required

// Keep track of how many ping-pong exchanges have taken place. "Ping" closes the socket when pingCount reaches pingPongLimit.
volatile int pingCount[numConnections];
volatile int pongCount[numConnections];
const int pingPongLimit = 20;

// Keep track of how long the sockets have been open. "Pong" closes the sockets when timeLimit (millis) is reached.
unsigned long startTime;
const unsigned long timeLimit = 120000; // 120 seconds

#include <IPAddress.h> // Needed for sockets
volatile int socketNum[numConnections]; // Record the socket numbers. -1 indicates the socket is invalid or closed.

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketData is provided to the SARA-R5 library via a 
// callback setter -- setSocketReadCallbackPlus. (See setup())
void processSocketData(int socket, const char *theData, int length, IPAddress remoteAddress, int remotePort)
{
  int connection = -1;
  for (int i = 0; i < numConnections; i++)
    if (socketNum[i] == socket)
      connection = i;

  if (connection == -1)
  {
    Serial.println();
    Serial.print(F("Data received on unexpected socket "));
    Serial.println(socket);
    return;
  }
  
  Serial.println();
  Serial.print(F("Data received on socket "));
  Serial.print(socket);
  Serial.print(F(" from IP "));
  Serial.print(remoteAddress[0]);
  Serial.print(F("."));
  Serial.print(remoteAddress[1]);
  Serial.print(F("."));
  Serial.print(remoteAddress[2]);
  Serial.print(F("."));
  Serial.print(remoteAddress[3]);
  Serial.print(F(" using port "));
  Serial.print(remotePort);
  Serial.print(F(" :"));
  for (int i = 0; i < length; i++)
  {
    Serial.print(F(" 0x"));
    if (theData[i] < 16)
      Serial.print(F("0"));
    Serial.print(theData[i], HEX);
  }
  Serial.println();
  
  if ((theData[0] == 0x00) && (theData[1] == 0x01) && (theData[2] == 0x02) && (theData[3] == 0x03)) // Look for the "Ping"
  {
    if (pongCount[connection] < pingPongLimit)
    {
      const char pong[] = { 0x04, 0x05, 0x06, 0x07 };
      mySARA.socketWriteUDP(socket, remoteAddress, remotePort, pong, 4); // Send the "Pong"
      pongCount[connection]++;
    }
  }

  if ((theData[0] == 0x04) && (theData[1] == 0x05) && (theData[2] == 0x06) && (theData[3] == 0x07)) // Look for the "Pong"
  {
    if (pingCount[connection] < pingPongLimit)
    {
      // Use the const char * version for binary data
      const char ping[] = { 0x00, 0x01, 0x02, 0x03 };
      mySARA.socketWriteUDP(socket, remoteAddress, remotePort, ping, 4); // Send the "Ping"
  
      pingCount[connection]++;
    }
  }

  Serial.print(F("pingCount = "));
  Serial.print(pingCount[connection]);
  Serial.print(F(" : pongCount = "));
  Serial.println(pongCount[connection]);
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketClose is provided to the SARA-R5 library via a 
// callback setter -- setSocketCloseCallback. (See setup())
// 
// Note: the SARA-R5 only sends a +UUSOCL URC when the socket os closed by the remote
void processSocketClose(int socket)
{
  Serial.println();
  Serial.print(F("Socket "));
  Serial.print(socket);
  Serial.println(F(" closed!"));
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

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
  Serial.print(ip[0]);
  Serial.print(F("."));
  Serial.print(ip[1]);
  Serial.print(F("."));
  Serial.print(ip[2]);
  Serial.print(F("."));
  Serial.print(ip[3]);
  Serial.println(F("\""));
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup()
{
  for (int i = 0; i < numConnections; i++) // Reset the ping and pong counts
  {
    pingCount[i] = 0;
    pongCount[i] = 0;
  }

  String currentOperator = "";

  Serial.begin(115200); // Start the serial console

  // Wait for user to press key to begin
  Serial.println(F("SARA-R5 Example"));
  Serial.println(F("Wait until the SARA's NI LED lights up - then press any key to begin"));
  
  while (!Serial.available()) // Wait for the user to press a key (send any serial character)
    ;
  while (Serial.available()) // Empty the serial RX buffer
    Serial.read();

  //mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

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

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

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

  // Deactivate the profile - in case one is already active
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_DEACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("Warning: performPDPaction (deactivate profile) failed. Probably because no profile was active."));
  }

  // Load the profile from NVM - these were saved by a previous example
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_LOAD) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (load from NVM) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  // Set a callback to process the results of the PSD Action - OPTIONAL
  //mySARA.setPSDActionCallback(&processPSDAction);

  // Activate the profile
  if (mySARA.performPDPaction(0, SARA_R5_PSD_ACTION_ACTIVATE) != SARA_R5_SUCCESS)
  {
    Serial.println(F("performPDPaction (activate profile) failed! Freezing..."));
    while (1)
      ; // Do nothing more
  }

  //for (int i = 0; i < 100; i++) // Wait for up to a second for the PSD Action URC to arrive - OPTIONAL
  //{
  //  mySARA.bufferedPoll(); // Keep processing data from the SARA so we can process the PSD Action
  //  delay(10);
  //}

  //Print the dynamic IP Address (for profile 0)
  IPAddress myAddress;
  mySARA.getNetworkAssignedIPAddress(0, &myAddress);
  Serial.print(F("\r\nMy IP Address is: "));
  Serial.print(myAddress[0]);
  Serial.print(F("."));
  Serial.print(myAddress[1]);
  Serial.print(F("."));
  Serial.print(myAddress[2]);
  Serial.print(F("."));
  Serial.println(myAddress[3]);

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  // Set a callback to process the socket data
  mySARA.setSocketReadCallbackPlus(&processSocketData);
  
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  // Set a callback to process the socket close
  mySARA.setSocketCloseCallback(&processSocketClose);
  
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  Serial.println(F("\r\nPlease enter the IP number you want to ping (nnn.nnn.nnn.nnn followed by LF / Newline): "));

  char c = 0;
  bool selected = false;
  int val = 0;
  IPAddress theAddress = {0,0,0,0};
  int field = 0;
  while (!selected)
  {
    while (!Serial.available()) ; // Wait for a character to arrive
    c = Serial.read(); // Read it
    if (c == '\n') // Is it a LF?
    {
      theAddress[field] = val; // Store the current value
      if (field == 3)
        selected = true;
      else
      {
        Serial.println(F("Invalid IP Address. Please try again:"));
        val = 0;
        field = 0;
      }
    }
    else if (c == '.') // Is it a separator
    {
      theAddress[field] = val; // Store the current value
      if (field <= 2)
        field++; // Increment the field
      val = 0; // Reset the value
    }
    else if ((c >= '0') && (c <= '9'))
    {
      val *= 10; // Multiply by 10
      val += c - '0'; // Add the digit
    }
  }

  Serial.print(F("Remote address is "));
  Serial.print(theAddress[0]);
  Serial.print(F("."));
  Serial.print(theAddress[1]);
  Serial.print(F("."));
  Serial.print(theAddress[2]);
  Serial.print(F("."));
  Serial.println(theAddress[3]);
  
  // Open the sockets
  for (int i = 0; i < numConnections; i++)
  {
  
    socketNum[i] = mySARA.socketOpen(SARA_R5_UDP);
    if (socketNum[i] == -1)
    {
      Serial.println(F("socketOpen failed! Freezing..."));
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }

    Serial.print(F("Connection "));
    Serial.print(i);
    Serial.print(F(" is using socket "));
    Serial.println(socketNum[i]);

    // Send the first ping to start the ping-pong
    const char ping[] = { 0x00, 0x01, 0x02, 0x03 };
    mySARA.socketWriteUDP(socketNum[i], theAddress, UDP_PORT_BASE + i, ping, 4); // Send the "Ping"
  }
    
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  startTime = millis(); // Record that start time

} // /setup()

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void loop()
{
  mySARA.bufferedPoll(); // Process the backlog (if any) and any fresh serial data

  for (int i = 0; i < numConnections; i++)
  {
    if (((pingCount[i] >= pingPongLimit) || (millis() > (startTime + timeLimit))) && (socketNum[i] >= 0))
    {
      printSocketParameters(socketNum[i]);
      Serial.print(F("\r\nClosing socket "));
      Serial.println(socketNum[i]);
      mySARA.socketClose(socketNum[i]); // Close the socket
      socketNum[i] = -1;
    }
  }
} // /loop()

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Print the socket parameters
// Note: the socket must be open. ERRORs will be returned if the socket is closed.
void printSocketParameters(int socket)
{
  Serial.print(F("\r\nSocket parameters for socket: "));
  Serial.println(socket);
  
  Serial.print(F("Socket type: "));
  SARA_R5_socket_protocol_t socketType;
  mySARA.querySocketType(socket, &socketType);
  if (socketType == SARA_R5_TCP)
    Serial.println(F("TCP"));
  else if (socketType == SARA_R5_UDP)
    Serial.println(F("UDP"));
  else
    Serial.println(F("UNKNOWN! (Error!)"));
  
  Serial.print(F("Total bytes sent: "));
  uint32_t bytesSent;
  mySARA.querySocketTotalBytesSent(socket, &bytesSent);
  Serial.println(bytesSent);
  
  Serial.print(F("Total bytes received: "));
  uint32_t bytesReceived;
  mySARA.querySocketTotalBytesReceived(socket, &bytesReceived);
  Serial.println(bytesReceived);
}
