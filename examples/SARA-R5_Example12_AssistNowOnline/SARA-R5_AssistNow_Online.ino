
#include "secrets.h" // Update secrets.h with your AssistNow token string

// u-blox AssistNow https servers
const char assistNowServer[] = "online-live1.services.u-blox.com";
//const char assistNowServer[] = "online-live2.services.u-blox.com"; // Alternate server

const char getQuery[] = "GetOnlineData.ashx?";
const char tokenPrefix[] = "token=";
const char tokenSuffix[] = ";";
const char getGNSS[] = "gnss=gps,glo;"; // GNSS can be: gps,qzss,glo,bds,gal
const char getDataType[] = "datatype=eph,alm,aux;"; // Data type can be: eph,alm,aux,pos

volatile bool httpResultSeen = false; // Flag to indicate that the HTTP URC was received

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// processHTTPcommandResult is provided to the SARA-R5 library via a 
// callback setter -- setHTTPCommandCallback. (See the end of setup())
void processHTTPcommandResult(int profile, int command, int result)
{
  Serial.println();
  Serial.print(F("HTTP Command Result:  profile: "));
  Serial.print(profile);
  Serial.print(F("  command: "));
  Serial.print(command);
  Serial.print(F("  result: "));
  Serial.print(result);
  if (result == 0)
    Serial.print(F(" (fail)"));
  if (result == 1)
    Serial.print(F(" (success)"));
  Serial.println();

  // Get and print the most recent HTTP protocol error
  int error_class;
  int error_code;
  mySARA.getHTTPprotocolError(0, &error_class, &error_code);
  Serial.print(F("Most recent HTTP protocol error:  class: "));
  Serial.print(error_class);
  Serial.print(F("  code: "));
  Serial.print(error_code);
  if (error_code == 0)
    Serial.print(F(" (no error)"));
  Serial.println();

  httpResultSeen = true; // Set the flag
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool getAssistNowOnlineData(String theFilename)
{
  // Use HTTP GET to receive the AssistNow_Online data. Store it in the SARA-R5's internal file system.

  String theServer = assistNowServer; // Convert the AssistNow server to String

  const int REQUEST_BUFFER_SIZE  = 256;
  char theRequest[REQUEST_BUFFER_SIZE];

  // Assemble the request
  // Note the slash at the beginning
  snprintf(theRequest, REQUEST_BUFFER_SIZE, "/%s%s%s%s%s%s",
    getQuery,
    tokenPrefix,
    myAssistNowToken,
    tokenSuffix,
    getGNSS,
    getDataType);

  String theRequestStr = theRequest; // Convert to String

  Serial.print(F("getAssistNowOnlineData: HTTP GET is https://"));
  Serial.print(theServer);
  Serial.println(theRequestStr);

  Serial.print(F("getAssistNowOnlineData: the AssistNow data will be stored in: "));
  Serial.println(theFilename);

  // Reset HTTP profile 0
  mySARA.resetHTTPprofile(0);
  
  // Set the server name
  mySARA.setHTTPserverName(0, theServer);
  
  // Use HTTPS
  mySARA.setHTTPsecure(0, false); // Setting this to true causes the GET to fail. Maybe due to the default CMNG profile?

  // Set a callback to process the HTTP command result
  mySARA.setHTTPCommandCallback(&processHTTPcommandResult);

  httpResultSeen = false; // Clear the flag

  // HTTP GET
  mySARA.sendHTTPGET(0, theRequestStr, theFilename);

  // Wait for 20 seconds while calling mySARA.bufferedPoll() to see the HTTP result.
  Serial.print(F("getAssistNowOnlineData: Waiting up to 20 seconds for the HTTP Result"));
  int i = 0;
  while ((i < 20000) && (httpResultSeen == false))
  {
    mySARA.bufferedPoll(); // Keep processing data from the SARA so we can catch the HTTP command result
    i++;
    delay(1);
    if (i % 1000 == 0)
      Serial.print(F("."));
  }
  Serial.println();
  
  if (httpResultSeen == false)
  {
    Serial.print(F("getAssistNowOnlineData: HTTP GET failed!"));
    return false;
  }

  int fileSize;
  if (mySARA.getFileSize(theFilename, &fileSize) != SARA_R5_SUCCESS)
  {
    Serial.print(F("getAssistNowOnlineData: No file written?!"));
    return false;    
  }
  
  Serial.print(F("File size is: "));
  Serial.println(fileSize);

  return true;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void prettyPrintString(String theString) // Pretty-print a String in HEX and ASCII format
{
  int theLength = theString.length();
  
  Serial.println();
  Serial.print(F("String length is "));
  Serial.print(theLength);
  Serial.print(F(" (0x"));
  Serial.print(theLength, HEX);
  Serial.println(F(")"));
  Serial.println();

  for (int i = 0; i < theLength; i += 16)
  {
    if (i < 10000) Serial.print(F("0"));
    if (i < 1000) Serial.print(F("0"));
    if (i < 100) Serial.print(F("0"));
    if (i < 10) Serial.print(F("0"));
    Serial.print(i);

    Serial.print(F(" 0x"));

    if (i < 0x1000) Serial.print(F("0"));
    if (i < 0x100) Serial.print(F("0"));
    if (i < 0x10) Serial.print(F("0"));
    Serial.print(i, HEX);

    Serial.print(F(" "));

    int j;
    for (j = 0; ((i + j) < theLength) && (j < 16); j++)
    {
      if (theString[i + j] < 0x10) Serial.print(F("0"));
      Serial.print(theString[i + j], HEX);
      Serial.print(F(" "));
    }

    if (((i + j) == theLength) && (j < 16))
    {
      for (int k = 0; k < (16 - (theLength % 16)); k++)
      {
        Serial.print(F("   "));
      }
    }
      
    for (j = 0; ((i + j) < theLength) && (j < 16); j++)
    {
      if ((theString[i + j] >= 0x20) && (theString[i + j] <= 0x7E))
        Serial.write(theString[i + j]);
      else
        Serial.print(F("."));
    }

    Serial.println();
  }

  Serial.println();
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void prettyPrintChars(char *theData, int theLength) // Pretty-print char data in HEX and ASCII format
{
  Serial.println();
  Serial.print(F("String length is "));
  Serial.print(theLength);
  Serial.print(F(" (0x"));
  Serial.print(theLength, HEX);
  Serial.println(F(")"));
  Serial.println();

  for (int i = 0; i < theLength; i += 16)
  {
    if (i < 10000) Serial.print(F("0"));
    if (i < 1000) Serial.print(F("0"));
    if (i < 100) Serial.print(F("0"));
    if (i < 10) Serial.print(F("0"));
    Serial.print(i);

    Serial.print(F(" 0x"));

    if (i < 0x1000) Serial.print(F("0"));
    if (i < 0x100) Serial.print(F("0"));
    if (i < 0x10) Serial.print(F("0"));
    Serial.print(i, HEX);

    Serial.print(F(" "));

    int j;
    for (j = 0; ((i + j) < theLength) && (j < 16); j++)
    {
      if (theData[i + j] < 0x10) Serial.print(F("0"));
      Serial.print(theData[i + j], HEX);
      Serial.print(F(" "));
    }

    if (((i + j) == theLength) && (j < 16))
    {
      for (int k = 0; k < (16 - (theLength % 16)); k++)
      {
        Serial.print(F("   "));
      }
    }
      
    for (j = 0; ((i + j) < theLength) && (j < 16); j++)
    {
      if ((theData[i + j] >= 0x20) && (theData[i + j] <= 0x7E))
        Serial.write(theData[i + j]);
      else
        Serial.print(F("."));
    }

    Serial.println();
  }

  Serial.println();
}
