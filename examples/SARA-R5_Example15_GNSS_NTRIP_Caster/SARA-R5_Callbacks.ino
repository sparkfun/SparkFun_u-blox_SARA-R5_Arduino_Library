// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processSocketClose is provided to the SARA-R5 library via a 
// callback setter -- setSocketCloseCallback. (See setup())
// 
// Note: the SARA-R5 only sends a +UUSOCL URC when the socket is closed by the remote
void processSocketClose(int socket)
{
  Serial.println();
  Serial.print(F("processSocketClose: Socket "));
  Serial.print(socket);
  Serial.println(F(" closed!"));

  if (socket == socketNum)
  {
    socketNum = -1;
    connectionOpen = false;
  }
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// processPSDAction is provided to the SARA-R5 library via a 
// callback setter -- setPSDActionCallback. (See setup())
void processPSDAction(int result, IPAddress ip)
{
  Serial.println();
  Serial.print(F("processPSDAction: result: "));
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
