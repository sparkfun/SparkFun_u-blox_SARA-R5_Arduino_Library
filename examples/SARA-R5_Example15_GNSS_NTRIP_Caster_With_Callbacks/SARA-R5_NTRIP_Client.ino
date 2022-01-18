
#include "secrets.h" // Update secrets.h with your AssistNow token string

const unsigned long maxTimeBeforeHangup_ms = 20000UL; //If we fail to get a complete RTCM frame after 20s, then disconnect from caster

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
      Serial.println(F(" and the connection is open!"));      
    }
    else
    {
      Serial.println(F("!"));
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
    char *serverRequest = new char[SERVER_BUFFER_SIZE];
    memset(serverRequest, 0, SERVER_BUFFER_SIZE);
    snprintf(serverRequest,
             SERVER_BUFFER_SIZE,
             "GET /%s HTTP/1.0\r\nUser-Agent: NTRIP SparkFun u-blox Client v1.0\r\n",
             mountPoint);

    // Set up the credentials
    const int CREDENTIALS_BUFFER_SIZE = 512;
    char *credentials = new char[CREDENTIALS_BUFFER_SIZE];
    memset(credentials, 0, CREDENTIALS_BUFFER_SIZE);
    if (strlen(casterUser) == 0)
    {
      strncpy(credentials, "Accept: */*\r\nConnection: close\r\n", CREDENTIALS_BUFFER_SIZE);
    }
    else
    {
      //Pass base64 encoded user:pw
      int userCredentialsSize = sizeof(casterUser) + sizeof(casterUserPW) + 1; //The ':' takes up a spot
      char *userCredentials = new char[userCredentialsSize];
      memset(userCredentials, 0, userCredentialsSize);
      snprintf(userCredentials, userCredentialsSize, "%s:%s", casterUser, casterUserPW);

      Serial.print(F("beginClient: Sending credentials: "));
      Serial.println(userCredentials);

#if defined(ARDUINO_ARCH_ESP32)
      //Encode with ESP32 built-in library
      base64 b;
      String strEncodedCredentials = b.encode(userCredentials);
      int encodedCredentialsSize = strEncodedCredentials.length() + 1;
      char *encodedCredentials = new char[encodedCredentialsSize];
      memset(encodedCredentials, 0, encodedCredentialsSize);
      strEncodedCredentials.toCharArray(encodedCredentials, encodedCredentialsSize); //Convert String to char array
#elif defined(ARDUINO_ARCH_APOLLO3) || defined(ARDUINO_ARDUINO_NANO33BLE)
      int encodedCredentialsSize = userCredentialsSize * 8;
      char *encodedCredentials = new char[encodedCredentialsSize];
      memset(encodedCredentials, 0, encodedCredentialsSize);
      size_t olen;
      mbedtls_base64_encode((unsigned char *)encodedCredentials, encodedCredentialsSize, &olen, (const unsigned char *)userCredentials, strlen(userCredentials));
#else
      //Encode with nfriendly library
      int encodedLen = base64_enc_len(userCredentialsSize);
      char *encodedCredentials = new char[encodedLen]; //Create array large enough to house encoded data
      memset(encodedCredentials, 0, encodedLen);
      base64_encode(encodedCredentials, userCredentials, strlen(userCredentials)); //Note: Input array is consumed
#endif

      snprintf(credentials, CREDENTIALS_BUFFER_SIZE, "Authorization: Basic %s\r\n", encodedCredentials);
      delete[] userCredentials;
      delete[] encodedCredentials;
    }

    // Add the encoded credentials to the server request
    strncat(serverRequest, credentials, SERVER_BUFFER_SIZE - 1);
    strncat(serverRequest, "\r\n", SERVER_BUFFER_SIZE - 1);

    Serial.print(F("beginClient: serverRequest size: "));
    Serial.print(strlen(serverRequest));
    Serial.print(F(" of "));
    Serial.print(SERVER_BUFFER_SIZE);
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
        closeConnection(theSocket, connectionIsOpen);
        delete[] serverRequest;
        delete[] credentials;
        return (false);
      }
      delay(100);
    }

    Serial.print(F("beginClient: server replied with "));
    Serial.print(availableLength);
    Serial.println(F(" bytes"));

    //Check reply
    int connectionResult = 0;
    char *response = new char[512 * 4];
    memset(response, 0, 512 * 4);
    size_t responseSpot = 0;
    while ((availableLength > 0) && (connectionResult == 0)) // Read bytes from the caster and store them
    {
      if ((responseSpot + availableLength) >= ((512 * 4) - 1)) // Exit the loop if we get too much data
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

    // Free the memory allocated for serverRequest, credentials and response
    delete[] serverRequest;
    delete[] credentials;
    delete[] response;

    if (connectionResult != 200)
    {
      Serial.print(F("beginClient: Failed to connect to "));
      Serial.println(casterHost);
      closeConnection(theSocket, connectionIsOpen);
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

//Check the connection
//Return false if: the connection has dropped, or if we receive no data for maxTimeBeforeHangup_ms
bool checkConnection(int theSocket, bool connectionIsOpen)
{
  if ((theSocket >= 0) && connectionIsOpen) // Check that the connection is still open
  {
    ; // Nothing to do here. The RTCM is pushed to the GNSS by the callabck.
  }
  else
  {
    Serial.println(F("checkConnection: Connection dropped!"));
    return (false); // Connection has dropped - return false
  }  
  
  //Timeout if we don't have new data for maxTimeBeforeHangup_ms
  if ((millis() - lastReceivedRTCM_ms) > maxTimeBeforeHangup_ms)
  {
    Serial.println(F("checkConnection: RTCM timeout!"));
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
