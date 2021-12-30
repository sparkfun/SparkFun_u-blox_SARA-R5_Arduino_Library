/*

  SARA-R5 Example
  ===============

  Socket "Ping Pong" - TCP Data Transfers

  Written by: Paul Clark
  Date: November 18th 2020

  This example demonstrates how to transfer data from one SARA-R5 to another using TCP sockets.

  This example includes the code from Example7_ConfigurePacketSwitchedData to let you see the SARA-R5's IP address.
  If you select "Ping":
    The code asks for the IP Address of the SARA-R5 you want to communicate with
    The code then opens a TCP socket to the "Pong" SARA-R5 using port number TCP_PORT
    The code sends an initial "Hello World?" using Write Socket Data (+USOWR)
    The code polls continuously. When a +UUSORD URC message is received, data is read and passed to the socketReadCallback.
    When "Hello World!" is received by the callback, the code sends "Hello World?" in reply
    The Ping-Pong repeats 100 times
    The socket is closed after the 100th reply is received
  If you select "Pong":
    The code opens a TCP socket and waits for data to arrive
    The code polls continuously. When a +UUSORD URC message is received, data is read and passed to the socketReadCallback.
    When "Hello World?" is received by the callback, the code sends "Hello World!" in reply
    The socket is closed after 600 seconds
  Start the "Pong" first!

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

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

unsigned int TCP_PORT = 1200; // Change this if required

bool iAmPing;

// Keep track of how many ping-pong exchanges have taken place. "Server" closes the socket when pingPongLimit is reached.
volatile int pingCount = 0;
volatile int pongCount = 0;
const int pingPongLimit = 100;

// Keep track of how long the socket has been open. "Client" closes the socket when timeLimit (millis) is reached.
unsigned long startTime;
const unsigned long timeLimit = 600000; // 600 seconds

#include <IPAddress.h> // Needed for sockets
volatile int socketNum;

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketListen is provided to the SARA-R5 library via a 
// callback setter -- setSocketListenCallback. (See setup())
void processSocketListen(int listeningSocket, IPAddress localIP, unsigned int listeningPort, int socket, IPAddress remoteIP, unsigned int port)
{
  Serial.println();
  Serial.print(F("Socket connection made: listeningSocket "));
  Serial.print(listeningSocket);
  Serial.print(F(" localIP "));
  Serial.print(localIP[0]);
  Serial.print(F("."));
  Serial.print(localIP[1]);
  Serial.print(F("."));
  Serial.print(localIP[2]);
  Serial.print(F("."));
  Serial.print(localIP[3]);
  Serial.print(F(" listeningPort "));
  Serial.print(listeningPort);
  Serial.print(F(" socket "));
  Serial.print(socket);
  Serial.print(F(" remoteIP "));
  Serial.print(remoteIP[0]);
  Serial.print(F("."));
  Serial.print(remoteIP[1]);
  Serial.print(F("."));
  Serial.print(remoteIP[2]);
  Serial.print(F("."));
  Serial.print(remoteIP[3]);
  Serial.print(F(" port "));
  Serial.println(port);
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketData is provided to the SARA-R5 library via a 
// callback setter -- setSocketReadCallback. (See setup())
void processSocketData(int socket, String theData)
{
  Serial.println();
  Serial.print(F("Data received on socket "));
  Serial.print(socket);
  Serial.print(F(" : "));
  Serial.println(theData);
  
  if (theData == String("Hello World?")) // Look for the "Ping"
  {
    const char pong[] = "Hello World!";
    mySARA.socketWrite(socket, pong); // Send the "Pong"
    pongCount++;
  }

  if (theData == String("Hello World!")) // Look for the "Pong"
  {
    const char ping[] = "Hello World?";
    mySARA.socketWrite(socket, ping); // Send the "Ping"
    pingCount++;
  }

  Serial.print(F("pingCount = "));
  Serial.print(pingCount);
  Serial.print(F(" : pongCount = "));
  Serial.println(pongCount);
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketClose is provided to the SARA-R5 library via a 
// callback setter -- setSocketCloseCallback. (See setup())
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
  Serial.print(String(ip[0]));
  Serial.print(F("."));
  Serial.print(String(ip[1]));
  Serial.print(F("."));
  Serial.print(String(ip[2]));
  Serial.print(F("."));
  Serial.print(String(ip[3]));
  Serial.println(F("\""));
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

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

  mySARA.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial

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

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  // Set a callback to process the socket listen
  mySARA.setSocketListenCallback(&processSocketListen);
  
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  // Set a callback to process the socket data
  mySARA.setSocketReadCallback(&processSocketData);
  
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  // Set a callback to process the socket close
  mySARA.setSocketCloseCallback(&processSocketClose);
  
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  Serial.println(F("\r\nDo you want to Ping or Pong? (Always start the Pong first!)\r\n"));
  Serial.println(F("1: Ping"));
  Serial.println(F("2: Pong"));
  Serial.println(F("\r\nPlease enter the number (followed by LF / Newline): "));
  
  c = 0;
  selected = false;
  selection = 0;
  while (!selected)
  {
    while (!Serial.available()) ; // Wait for a character to arrive
    c = Serial.read(); // Read it
    if (c == '\n') // Is it a LF?
    {
      if ((selection >= 1) && (selection <= 2))
      {
        selected = true;
        if (selection == 1)
          Serial.println(F("\r\nPing selected!"));
        else
          Serial.println(F("\r\nPong selected!"));
      }
      else
      {
        Serial.println(F("Invalid choice. Please try again:"));
        selection = 0;
      }
    }
    else
    {
      selection = c - '0'; // Store a single digit
    }
  }

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  if (selection == 1) // Ping
  {
    iAmPing = true;
    
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
        if ((field == 3)
            && (theAddress[0] >= 0) && (theAddress[0] <= 255)
            && (theAddress[1] >= 0) && (theAddress[1] <= 255)
            && (theAddress[2] >= 0) && (theAddress[2] <= 255)
            && (theAddress[3] >= 0) && (theAddress[3] <= 255))
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
      else
      {
        val *= 10; // Multiply by 10
        val += c - '0'; // Add the digit
      }
    }

    char theAddressString[16];
    sprintf(theAddressString, "%d.%d.%d.%d", theAddress[0], theAddress[1], theAddress[2], theAddress[3]);
    Serial.print(F("Remote address is "));
    Serial.println(theAddressString);
    
    // Open the socket
    socketNum = mySARA.socketOpen(SARA_R5_TCP);
    if (socketNum == -1)
    {
      Serial.println(F("socketOpen failed! Freezing..."));
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }

    Serial.print(F("Using socket "));
    Serial.println(socketNum);

    // Connect to the remote IP Address
    if (mySARA.socketConnect(socketNum, (const char *)theAddressString, TCP_PORT) != SARA_R5_SUCCESS)
    {
      Serial.println(F("socketConnect failed! Freezing..."));
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }
    else
    {
      Serial.println(F("Socket connected!"));
    }

    // Send the first ping to start the ping-pong
    const char ping[] = "Hello World?";
    mySARA.socketWrite(socketNum, ping); // Send the "Ping"
        
  }

    
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  else // if (selection == 2) // Pong
  {
    iAmPing = false;
    
    // Open the socket
    socketNum = mySARA.socketOpen(SARA_R5_TCP);
    if (socketNum == -1)
    {
      Serial.println(F("socketOpen failed! Freezing..."));
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }

    Serial.print(F("Using socket "));
    Serial.println(socketNum);

    // Start listening for a connection
    if (mySARA.socketListen(socketNum, TCP_PORT) != SARA_R5_SUCCESS)
    {
      Serial.println(F("socketListen failed! Freezing..."));
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }
  }

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  startTime = millis(); // Record that start time

}

void loop()
{
  mySARA.bufferedPoll(); // Process the backlog (if any) and any fresh serial data

  if (iAmPing) // Ping - close the socket when we've reached pingPongLimit
  {
    if (pingCount >= pingPongLimit)
    {
      mySARA.socketClose(socketNum); // Close the socket - no more pings will be sent
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }
  }
  
  else // Pong - close the socket when we've reached the timeLimit
  {
    if (millis() > (startTime + timeLimit))
    {
      mySARA.socketClose(socketNum); // Close the socket - no more pongs will be sent
      while (1)
        mySARA.bufferedPoll(); // Do nothing more except process any received data
    }
  }
}
