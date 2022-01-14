
#include "secrets.h" // Update secrets.h with your AssistNow token string

const unsigned long maxTimeBeforeHangup_ms = 20000UL; //If we fail to get a complete RTCM frame after 20s, then disconnect from caster
const int maxSocketRead = 1000; // Limit socket reads to this length

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Connect to NTRIP Caster. Return true is connection is successful.
bool beginClient(int *theSocket, bool *connectionIsOpen)
{
  // Sanity check - return now if the socket is already open (should be impossible, but still...)
  if (*theSocket >= 0)
  {
    Serial.print(F("beginClient: Socket is already open"));
    if (*connectionIsOpen)
    {
      Serial.print(F(" and the connection is open!"));      
    }
    else
    {
      Serial.print(F("!"));
    }
    return (false);
  }

  Serial.println(F("beginClient: Opening TCP socket"));

  *theSocket = mySARA.socketOpen(SARA_R5_TCP);
  if (*theSocket == -1)
  {
    Serial.println(F("beginClient: socketOpen failed!"));
    return (false);
  }

  Serial.print(F("beginClient: Using socket "));
  Serial.println(*theSocket);

  Serial.print(F("beginClient: Connecting to "));
  Serial.print(casterHost);
  Serial.print(F(" on port "));
  Serial.println(casterPort);

  if (mySARA.socketConnect(*theSocket, casterHost, casterPort) != SARA_R5_SUCCESS)
  {
    Serial.println(F("beginClient: socketConnect failed!"));
  }
  else
  {
    Serial.print(F("beginClient: Connected to "));
    Serial.print(casterHost);
    Serial.print(F(" : "));
    Serial.println(casterPort);

    Serial.print(F("beginClient: Requesting NTRIP Data from mount point "));
    Serial.println(mountPoint);

    // Set up the server request (GET)
    const int SERVER_BUFFER_SIZE = 512;
    char serverRequest[SERVER_BUFFER_SIZE];
    snprintf(serverRequest,
             SERVER_BUFFER_SIZE,
             "GET /%s HTTP/1.0\r\nUser-Agent: NTRIP SparkFun u-blox Client v1.0\r\n",
             mountPoint);

    // Set up the credentials
    char credentials[512];
    if (strlen(casterUser) == 0)
    {
      strncpy(credentials, "Accept: */*\r\nConnection: close\r\n", sizeof(credentials));
    }
    else
    {
      //Pass base64 encoded user:pw
      char userCredentials[sizeof(casterUser) + sizeof(casterUserPW) + 1]; //The ':' takes up a spot
      snprintf(userCredentials, sizeof(userCredentials), "%s:%s", casterUser, casterUserPW);

      Serial.print(F("beginClient: Sending credentials: "));
      Serial.println(userCredentials);

#if defined(ARDUINO_ARCH_ESP32)
      //Encode with ESP32 built-in library
      base64 b;
      String strEncodedCredentials = b.encode(userCredentials);
      char encodedCredentials[strEncodedCredentials.length() + 1];
      strEncodedCredentials.toCharArray(encodedCredentials, sizeof(encodedCredentials)); //Convert String to char array
#else
      //Encode with nfriendly library
      int encodedLen = base64_enc_len(strlen(userCredentials));
      char encodedCredentials[encodedLen];                                         //Create array large enough to house encoded data
      base64_encode(encodedCredentials, userCredentials, strlen(userCredentials)); //Note: Input array is consumed
#endif

      snprintf(credentials, sizeof(credentials), "Authorization: Basic %s\r\n", encodedCredentials);
    }

    // Add the encoded credentials to the server request
    strncat(serverRequest, credentials, SERVER_BUFFER_SIZE);
    strncat(serverRequest, "\r\n", SERVER_BUFFER_SIZE);

    Serial.print(F("beginClient: serverRequest size: "));
    Serial.print(strlen(serverRequest));
    Serial.print(F(" of "));
    Serial.print(sizeof(serverRequest));
    Serial.println(F(" bytes available"));

    // Send the server request
    Serial.println(F("beginClient: Sending server request: "));
    Serial.println(serverRequest);

    mySARA.socketWrite(*theSocket, (const char *)serverRequest);

    //Wait up to 5 seconds for response. Poll the number of available bytes. Don't use the callback yet.
    unsigned long startTime = millis();
    int availableLength = 0;
    while (availableLength == 0)
    {
      mySARA.socketReadAvailable(*theSocket, &availableLength);
      if (millis() > (startTime + 5000))
      {
        Serial.println(F("beginClient: Caster timed out!"));
        return (false);
      }
      delay(100);
    }

    Serial.print(F("beginClient: server replied with "));
    Serial.print(availableLength);
    Serial.println(F(" bytes"));

    //Check reply
    int connectionResult = 0;
    char response[512 * 4];
    memset(response, 0, 512 * 4);
    size_t responseSpot = 0;
    while ((availableLength > 0) && (connectionResult == 0)) // Read bytes from the caster and store them
    {
      if ((responseSpot + availableLength) >= (sizeof(response) - 1)) // Exit the loop if we get too much data
        break;

      mySARA.socketRead(*theSocket, availableLength, &response[responseSpot]);
      responseSpot += availableLength;

      //Serial.print(F("beginClient: response is: "));
      //Serial.println(response);

      if (connectionResult == 0) // Only print success/fail once
      {
        if (strstr(response, "200") != NULL) //Look for '200 OK'
        {
          Serial.println(F("beginClient: 200 seen!"));
          connectionResult = 200;
        }
        if (strstr(response, "401") != NULL) //Look for '401 Unauthorized'
        {
          Serial.println(F("beginClient: Hey - your credentials look bad! Check your caster username and password."));
          connectionResult = 401;
        }
      }

      mySARA.socketReadAvailable(*theSocket, &availableLength); // Update availableLength

      Serial.print(F("beginClient: socket now has "));
      Serial.print(availableLength);
      Serial.println(F(" bytes available"));
    }
    response[responseSpot] = '\0'; // NULL-terminate the response

    //Serial.print(F("beginClient: Caster responded with: ")); Serial.println(response); // Uncomment this line to see the full response

    if (connectionResult != 200)
    {
      Serial.print(F("beginClient: Failed to connect to "));
      Serial.println(casterHost);
      return (false);
    }
    else
    {
      Serial.print(F("beginClient: Connected to: "));
      Serial.println(casterHost);
      lastReceivedRTCM_ms = millis(); //Reset timeout
    }
  
  } //End attempt to connect

  *connectionIsOpen = true;
  return (true);
} // /beginClient

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Read and push the RTCM data to the GNSS
//Return false if: the connection has dropped, or if we receive no data for maxTimeBeforeHangup_ms
bool processConnection(int theSocket, bool connectionIsOpen)
{
  if (!connectionIsOpen)
  {
    Serial.print(F("processConnection: the connection is closed"));
    if (theSocket < 0)
    {
      Serial.print(F(" and the socket is closed too!"));      
    }
    else
    {
      Serial.print(F("!"));
    }
    return (false);
  }

  if (theSocket < 0)
  {
    Serial.print(F("processConnection: the socket is closed!"));      
    return (false);
  }

  int availableLength = 0;
  mySARA.socketReadAvailable(theSocket, &availableLength);

  if (availableLength > 0)
  {
    Serial.print(F("processConnection: Socket "));
    Serial.print(theSocket);
    Serial.print(F(" has "));
    Serial.print(availableLength);
    Serial.println(F(" bytes available"));

    if (availableLength > maxSocketRead)
    {
      Serial.println(F("processConnection: only reading 1024 bytes"));
      availableLength = 1000;
    }

    uint8_t *socketData = new uint8_t[availableLength];

    if (socketData == NULL)
    {
      Serial.print(F("processConnection: new (malloc) failed!"));
      return (false);
    }

    if (mySARA.socketRead(theSocket, availableLength, (char *)socketData) != SARA_R5_SUCCESS)
    {
      Serial.print(F("processConnection: socketRead failed!"));
      free(socketData);
      return (false);      
    }
  
    Serial.println(F("processConnection: Pushing it to the GNSS..."));
    myGNSS.pushRawData(socketData, availableLength);

    delete[] socketData;

    lastReceivedRTCM_ms = millis(); // Update lastReceivedRTCM_ms
  }
  
  //Timeout if we don't have new data for maxTimeBeforeHangup_ms
  if ((millis() - lastReceivedRTCM_ms) > maxTimeBeforeHangup_ms)
  {
    Serial.println(F("processConnection: RTCM timeout!"));
    return (false); // Connection has timed out - return false
  }

  return (true);
} // /processConnection

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void closeConnection(int *theSocket, bool *connectionIsOpen)
{
  // Check the socket is actually open, otherwise we'll get an error when we try to close it
  if (*theSocket >= 0)
  {
    mySARA.socketClose(*theSocket);
    Serial.println(F("closeConnection: Connection closed!"));
  }
  else
  {
    Serial.println(F("closeConnection: Connection was already closed!"));    
  }

  *theSocket = -1; // Clear the socket number to indicate it is closed
  *connectionIsOpen = false; // Flag that the connection is closed
}  

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
