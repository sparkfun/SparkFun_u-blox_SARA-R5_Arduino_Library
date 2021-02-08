/*
  Arduino library for the u-blox SARA-R5 LTE-M / NB-IoT modules with secure cloud, as used on the SparkFun MicroMod Asset Tracker
  By: Paul Clark
  October 19th 2020

  Based extensively on the:
  Arduino Library for the SparkFun LTE CAT M1/NB-IoT Shield - SARA-R4
  Written by Jim Lindblom @ SparkFun Electronics, September 5, 2018

  This Arduino library provides mechanisms to initialize and use
  the SARA-R5 module over either a SoftwareSerial or hardware serial port.

  Please see LICENSE.md for the license information

*/

#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h>

SARA_R5::SARA_R5(int powerPin, int resetPin, uint8_t maxInitDepth)
{
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    _softSerial = NULL;
#endif
    _hardSerial = NULL;
    _baud = 0;
    _resetPin = resetPin;
    _powerPin = powerPin;
    _invertPowerPin = false;
    _maxInitDepth = maxInitDepth;
    _socketReadCallback = NULL;
    _socketCloseCallback = NULL;
    _gpsRequestCallback = NULL;
    _simStateReportCallback = NULL;
    _psdActionRequestCallback = NULL;
    _pingRequestCallback = NULL;
    _httpCommandRequestCallback = NULL;
    _lastRemoteIP = {0, 0, 0, 0};
    _lastLocalIP = {0, 0, 0, 0};

    memset(saraRXBuffer, 0, RXBuffSize);
  	memset(saraResponseBacklog, 0, RXBuffSize);
}

#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
boolean SARA_R5::begin(SoftwareSerial &softSerial, unsigned long baud)
{
    SARA_R5_error_t err;

    _softSerial = &softSerial;

    err = init(baud);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        return true;
    }
    return false;
}
#endif

boolean SARA_R5::begin(HardwareSerial &hardSerial, unsigned long baud)
{
    SARA_R5_error_t err;

    _hardSerial = &hardSerial;

    err = init(baud);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        return true;
    }
    return false;
}

//Calling this function with nothing sets the debug port to Serial
//You can also call it with other streams like Serial1, SerialUSB, etc.
void SARA_R5::enableDebugging(Stream &debugPort)
{
	_debugPort = &debugPort;
	_printDebug = true;
}

boolean SARA_R5::bufferedPoll(void)
{
	int avail = 0;
  char c = 0;
  bool handled = false;
	unsigned long timeIn = micros();
	memset(saraRXBuffer, 0, RXBuffSize);
	int backlogLen = strlen(saraResponseBacklog);
	char *event;

	if (backlogLen > 0)
  {
    //The backlog also logs reads from other tasks like transmitting.
		if (_printDebug == true) _debugPort->println("Backlog found!");
		memcpy(saraRXBuffer+avail, saraResponseBacklog, backlogLen);
		avail+=backlogLen;
		memset(saraResponseBacklog, 0, RXBuffSize);
	}

	if (hwAvailable() > 0 || backlogLen > 0) // If either new data is available, or backlog had data.
  {
		while (micros() - timeIn < rxWindowUS && avail < RXBuffSize)
    {
			if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
      {
			    c = readChar();
          saraRXBuffer[avail++] = c;
				   timeIn = micros();
      }
		}
		event = strtok(saraRXBuffer, "\r\n");
		while (event != NULL)
    {
			if (_printDebug == true) _debugPort->print("Event:");
			if (_printDebug == true) _debugPort->print(event);
			handled = processReadEvent(event);
			backlogLen = strlen(saraResponseBacklog);
			if (backlogLen > 0 && (avail+backlogLen) < RXBuffSize)
      {
				if (_printDebug == true) _debugPort->println("Backlog added!");
				memcpy(saraRXBuffer+avail, saraResponseBacklog, backlogLen);
				avail+=backlogLen;
				memset(saraResponseBacklog, 0, RXBuffSize);//Clear out backlog buffer.
			}
			event = strtok(NULL, "\r\n");
			if (_printDebug == true) _debugPort->println("!");//Just to denote end of processing event.
    }
  }
	free(event);
  return handled;
}

boolean SARA_R5::processReadEvent(char* event)
{
	{
		int socket, length;
		int ret = sscanf(event, "+UUSORD: %d,%d", &socket, &length);
		if (ret == 2)
		{
			if (_printDebug == true) _debugPort->println("PARSED SOCKET READ");
			parseSocketReadIndication(socket, length);
			return true;
		}
	}
	{
		int socket, length;
		int ret = sscanf(event, "+UUSORF: %d,%d", &socket, &length);
		if (ret == 2)
    {
			if (_printDebug == true) _debugPort->println("PARSED UDP READ");
			parseSocketReadIndicationUDP(socket, length);
			return true;
		}
	}
	{
		int socket, listenSocket;
		unsigned int port, listenPort;
		IPAddress remoteIP, localIP;
		int ret = sscanf(event,
				   "+UUSOLI: %d,\"%d.%d.%d.%d\",%u,%d,\"%d.%d.%d.%d\",%u",
				   &socket,
				   &remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3],
				   &port, &listenSocket,
				   &localIP[0], &localIP[1], &localIP[2], &localIP[3],
				   &listenPort);
		if (ret > 4)
		{
			if (_printDebug == true) _debugPort->println("PARSED SOCKET LISTEN");
			parseSocketListenIndication(localIP, remoteIP);
			return true;
		}
	}
	{
		int socket;
		int ret = sscanf(event, "+UUSOCL: %d", &socket);
		if (ret == 1)
		{
			if (_printDebug == true) _debugPort->println("PARSED SOCKET CLOSE");
			if ((socket >= 0) && (socket <= 6))
			{
				if (_socketCloseCallback != NULL)
				{
					_socketCloseCallback(socket);
				}
			}
			return true;
		}
	}
	return false;
}

boolean SARA_R5::poll(void)
{
    int avail = 0;
    char c = 0;
    bool handled = false;

    memset(saraRXBuffer, 0, RXBuffSize);

    if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
    {
        while (c != '\n')
        {
            if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
            {
                c = readChar();
                saraRXBuffer[avail++] = c;
            }
        }
        {
            int socket, length;
            if (sscanf(saraRXBuffer, "+UUSORD: %d,%d", &socket, &length) == 2)
            {
                parseSocketReadIndication(socket, length);
                handled = true;
            }
        }
        {
            int socket, listenSocket;
            unsigned int port, listenPort;
            IPAddress remoteIP, localIP;

            if (sscanf(saraRXBuffer,
                       "+UUSOLI: %d,\"%d.%d.%d.%d\",%u,%d,\"%d.%d.%d.%d\",%u",
                       &socket,
                       &remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3],
                       &port, &listenSocket,
                       &localIP[0], &localIP[1], &localIP[2], &localIP[3],
                       &listenPort) > 4)
            {
                parseSocketListenIndication(localIP, remoteIP);
                handled = true;
            }
        }
        {
            int socket;

            if (sscanf(saraRXBuffer,
                       "+UUSOCL: %d", &socket) == 1)
            {
                if ((socket >= 0) && (socket <= 6))
                {
                    if (_socketCloseCallback != NULL)
                    {
                        _socketCloseCallback(socket);
                    }
                }
                handled = true;
            }
        }
        {
            ClockData clck;
            PositionData gps;
            SpeedData spd;
            unsigned long uncertainty;
            int scanNum;
            unsigned int latH, lonH, altU, speedU, cogU;
            char latL[10], lonL[10];

            // Maybe we should also scan for +UUGIND and extract the activated gnss system?

            if (strstr(saraRXBuffer, "+UULOC"))
            {
                // Found a Location string!
                // This assumes the ULOC response type is "1". TO DO: check that is true...
                scanNum = sscanf(saraRXBuffer,
                                 "+UULOC: %hhu/%hhu/%u,%hhu:%hhu:%hhu.%u,%u.%[^,],%u.%[^,],%u,%lu,%u,%u,*%s",
                                 &clck.date.day, &clck.date.month, &clck.date.year,
                                 &clck.time.hour, &clck.time.minute, &clck.time.second, &clck.time.ms,
                                 &latH, latL, &lonH, lonL, &altU, &uncertainty,
                                 &speedU, &cogU);
                if (scanNum < 13)
                    return false; // Break out if we didn't find enough

                gps.lat = (float)latH + ((float)atol(latL) / pow(10, strlen(latL)));
                gps.lon = (float)lonH + ((float)atol(lonL) / pow(10, strlen(lonL)));
                gps.alt = (float)altU;
                if (scanNum >= 15) // If detailed response, get speed data
                {
                    spd.speed = (float)speedU;
                    spd.cog = (float)cogU;
                }

                if (_gpsRequestCallback != NULL)
                {
                    _gpsRequestCallback(clck, gps, spd, uncertainty);
                }

                handled = true;
            }
        }
        {
            SARA_R5_sim_states_t state;
            int scanNum;

            if (strstr(saraRXBuffer, "+UUSIMSTAT"))
            {
                scanNum = sscanf(saraRXBuffer, "+UUSIMSTAT:%d", &state);
                if (scanNum < 1)
                    return false; // Break out if we didn't find enough

                if (_simStateReportCallback != NULL)
                {
                    _simStateReportCallback(state);
                }

                handled = true;
            }
        }
        {
            int result;
            IPAddress remoteIP = (0,0,0,0);
            int scanNum;

            if (strstr(saraRXBuffer, "+UUPSDA"))
            {
                scanNum = sscanf(saraRXBuffer, "+UUPSDA: %d,\"%d.%d.%d.%d\"",
                        &result, &remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3]);
                if (scanNum < 1)
                    return false; // Break out if we didn't find enough

                if (_psdActionRequestCallback != NULL)
                {
                    _psdActionRequestCallback(result, remoteIP);
                }

                handled = true;
            }
        }
        {
            int retry = 0;
            int p_size = 0;
            int ttl = 0;
            String remote_host = "";
            IPAddress remoteIP = (0,0,0,0);
            long rtt = 0;
            int scanNum;
            char *searchPtr = saraRXBuffer;

            // Find the first/next occurrence of +UUPING:
            searchPtr = strstr(searchPtr, "+UUPING: ");
            if (searchPtr != NULL)
            {
                // if (_printDebug == true)
                // {
                //   _debugPort->print("poll +UUPING: saraRXBuffer: ");
                //   _debugPort->println(saraRXBuffer);
                // }

                // Extract the retries and payload size
                scanNum = sscanf(searchPtr, "+UUPING: %d,%d,", &retry, &p_size);

                if (scanNum < 2)
                    return false; // Break out if we didn't find enough

                searchPtr = strchr(++searchPtr, '\"'); // Search to the first quote

                // Extract the remote host name, stop at the next quote
                while ((*(++searchPtr) != '\"') && (*searchPtr != '\0'))
                {
                    remote_host.concat(*(searchPtr));
                }

                if (*searchPtr == '\0')
                  return false; // Break out if we didn't find enough

                scanNum = sscanf(searchPtr, "\",\"%d.%d.%d.%d\",%d,%d",
                        &remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3], &ttl, &rtt);

                if (scanNum < 6)
                    return false; // Break out if we didn't find enough

                if (_pingRequestCallback != NULL)
                {
                    _pingRequestCallback(retry, p_size, remote_host, remoteIP, ttl, rtt);
                }

                handled = true;
            }
        }
        {
            int profile, command, result;
            int scanNum;

            if (strstr(saraRXBuffer, "+UUHTTPCR"))
            {
                scanNum = sscanf(saraRXBuffer, "+UUHTTPCR: %d,%d,%d", &profile, &command, &result);
                if (scanNum < 3)
                    return false; // Break out if we didn't find enough

                if ((profile >= 0) && (profile < SARA_R5_NUM_HTTP_PROFILES))
                {
                    if (_httpCommandRequestCallback != NULL)
                    {
                        _httpCommandRequestCallback(profile, command, result);
                    }
                }

                handled = true;
            }
        }

        if ((handled == false) && (strlen(saraRXBuffer) > 2))
        {
            if (_printDebug == true) _debugPort->println("Poll: " + String(saraRXBuffer));
        }
        else
        {
        }
    }
    return handled;
}

void SARA_R5::setSocketReadCallback(void (*socketReadCallback)(int, String))
{
    _socketReadCallback = socketReadCallback;
}

void SARA_R5::setSocketCloseCallback(void (*socketCloseCallback)(int))
{
    _socketCloseCallback = socketCloseCallback;
}

void SARA_R5::setGpsReadCallback(void (*gpsRequestCallback)(ClockData time,
              PositionData gps, SpeedData spd, unsigned long uncertainty))
{
    _gpsRequestCallback = gpsRequestCallback;
}

void SARA_R5::setSIMstateReportCallback(void (*simStateReportCallback)
              (SARA_R5_sim_states_t state))
{
    _simStateReportCallback = simStateReportCallback;
}

void SARA_R5::setPSDActionCallback(void (*psdActionRequestCallback)
              (int result, IPAddress ip))
{
    _psdActionRequestCallback = psdActionRequestCallback;
}

void SARA_R5::setPingCallback(void (*pingRequestCallback)
              (int retry, int p_size, String remote_hostname, IPAddress ip, int ttl, long rtt))
{
    _pingRequestCallback = pingRequestCallback;
}

void SARA_R5::setHTTPCommandCallback(void (*httpCommandRequestCallback)
              (int profile, int command, int result))
{
    _httpCommandRequestCallback = httpCommandRequestCallback;
}

size_t SARA_R5::write(uint8_t c)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->write(c);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->write(c);
    }
#endif
    return (size_t)0;
}

size_t SARA_R5::write(const char *str)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->print(str);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->print(str);
    }
#endif
    return (size_t)0;
}

size_t SARA_R5::write(const char *buffer, size_t size)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->print(buffer);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->print(buffer);
    }
#endif
    return (size_t)0;
}

SARA_R5_error_t SARA_R5::at(void)
{
    SARA_R5_error_t err;
    char *command;

    err = sendCommandWithResponse(NULL, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    return err;
}

SARA_R5_error_t SARA_R5::enableEcho(boolean enable)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_ECHO) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (enable)
    {
        sprintf(command, "%s1", SARA_R5_COMMAND_ECHO);
    }
    else
    {
        sprintf(command, "%s0", SARA_R5_COMMAND_ECHO);
    }
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

String SARA_R5::getManufacturerID(void)
{
    char *response;
    char idResponse[16] = { 0x00 }; // E.g. u-blox
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_MANU_ID,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", idResponse) != 1)
        {
            memset(idResponse, 0, 16);
        }
    }
    free(response);
    return String(idResponse);
}

String SARA_R5::getModelID(void)
{
    char *response;
    char idResponse[16] = { 0x00 }; // E.g. SARA-R510M8Q
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_MODEL_ID,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", idResponse) != 1)
        {
            memset(idResponse, 0, 16);
        }
    }
    free(response);
    return String(idResponse);
}

String SARA_R5::getFirmwareVersion(void)
{
    char *response;
    char idResponse[16] = { 0x00 }; // E.g. 11.40
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_FW_VER_ID,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", idResponse) != 1)
        {
            memset(idResponse, 0, 16);
        }
    }
    free(response);
    return String(idResponse);
}

String SARA_R5::getSerialNo(void)
{
    char *response;
    char idResponse[16] = { 0x00 }; // E.g. 357520070120767
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_SERIAL_NO,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", idResponse) != 1)
        {
            memset(idResponse, 0, 16);
        }
    }
    free(response);
    return String(idResponse);
}

String SARA_R5::getIMEI(void)
{
    char *response;
    char imeiResponse[16] = { 0x00 }; // E.g. 004999010640000
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(imeiResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_IMEI,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", imeiResponse) != 1)
        {
            memset(imeiResponse, 0, 16);
        }
    }
    free(response);
    return String(imeiResponse);
}

String SARA_R5::getIMSI(void)
{
    char *response;
    char imsiResponse[16] = { 0x00 }; // E.g. 222107701772423
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(imsiResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_IMSI,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n%s\r\n", imsiResponse) != 1)
        {
            memset(imsiResponse, 0, 16);
        }
    }
    free(response);
    return String(imsiResponse);
}

String SARA_R5::getCCID(void)
{
    char *response;
    char ccidResponse[21] = { 0x00 }; // E.g. +CCID: 8939107900010087330
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(ccidResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_CCID,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n+CCID: %s", ccidResponse) != 1)
        {
            memset(ccidResponse, 0, 21);
        }
    }
    free(response);
    return String(ccidResponse);
}

String SARA_R5::getSubscriberNo(void)
{
    char *response;
    char idResponse[128] = { 0x00 }; // E.g. +CNUM: "ABCD . AAA","123456789012",129
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_CNUM,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_10_SEC_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n+CNUM: %s", idResponse) != 1)
        {
            memset(idResponse, 0, 128);
        }
    }
    free(response);
    return String(idResponse);
}

String SARA_R5::getCapabilities(void)
{
    char *response;
    char idResponse[128] = { 0x00 }; // E.g. +GCAP: +FCLASS, +CGSM
    SARA_R5_error_t err;

    response = sara_r5_calloc_char(sizeof(idResponse) + 16);

    err = sendCommandWithResponse(SARA_R5_COMMAND_REQ_CAP,
                                  SARA_R5_RESPONSE_OK, response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n+GCAP: %s", idResponse) != 1)
        {
            memset(idResponse, 0, 128);
        }
    }
    free(response);
    return String(idResponse);
}

SARA_R5_error_t SARA_R5::reset(void)
{
    SARA_R5_error_t err;

    err = functionality(SILENT_RESET_WITH_SIM);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Reset will set the baud rate back to 115200
        //beginSerial(9600);
        err = SARA_R5_ERROR_INVALID;
        while (err != SARA_R5_ERROR_SUCCESS)
        {
            beginSerial(SARA_R5_DEFAULT_BAUD_RATE);
            setBaud(_baud);
            delay(200);
            beginSerial(_baud);
            err = at();
            delay(500);
        }
        return init(_baud);
    }
    return err;
}

String SARA_R5::clock(void)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *clockBegin;
    char *clockEnd;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_CLOCK) + 2);
    if (command == NULL)
        return "";
    sprintf(command, "%s?", SARA_R5_COMMAND_CLOCK);

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return "";
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return "";
    }

    // Response format: \r\n+CCLK: "YY/MM/DD,HH:MM:SS-TZ"\r\n\r\nOK\r\n
    clockBegin = strchr(response, '\"'); // Find first quote
    if (clockBegin == NULL)
    {
        free(command);
        free(response);
        return "";
    }
    clockBegin += 1;                     // Increment pointer to begin at first number
    clockEnd = strchr(clockBegin, '\"'); // Find last quote
    if (clockEnd == NULL)
    {
        free(command);
        free(response);
        return "";
    }
    *(clockEnd) = '\0'; // Set last quote to null char -- end string

    free(command);
    free(response);

    return String(clockBegin);
}

SARA_R5_error_t SARA_R5::clock(uint8_t *y, uint8_t *mo, uint8_t *d,
                                     uint8_t *h, uint8_t *min, uint8_t *s, uint8_t *tz)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *clockBegin;
    char *clockEnd;

    int iy, imo, id, ih, imin, is, itz;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_CLOCK) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_COMMAND_CLOCK);

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    // Response format: \r\n+CCLK: "YY/MM/DD,HH:MM:SS-TZ"\r\n\r\nOK\r\n
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        if (sscanf(response, "\r\n+CCLK: \"%d/%d/%d,%d:%d:%d-%d\"\r\n",
                   &iy, &imo, &id, &ih, &imin, &is, &itz) == 7)
        {
            *y = iy;
            *mo = imo;
            *d = id;
            *h = ih;
            *min = imin;
            *s = is;
            *tz = itz;
        }
        else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }

    free(command);
    free(response);
    return err;
}

SARA_R5_error_t SARA_R5::setUtimeMode(SARA_R5_utime_mode_t mode, SARA_R5_utime_sensor_t sensor)
{
  SARA_R5_error_t err;
  char *command;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_REQUEST_TIME) + 5);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  if (mode == SARA_R5_UTIME_MODE_STOP) // stop UTIME does not require a sensor
    sprintf(command, "%s=%d", SARA_R5_GNSS_REQUEST_TIME, mode);
  else
    sprintf(command, "%s=%d,%d", SARA_R5_GNSS_REQUEST_TIME, mode, sensor);

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                NULL, SARA_R5_10_SEC_TIMEOUT);
  free(command);
  return err;
}

SARA_R5_error_t SARA_R5::getUtimeMode(SARA_R5_utime_mode_t *mode, SARA_R5_utime_sensor_t *sensor)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  SARA_R5_utime_mode_t m;
  SARA_R5_utime_sensor_t s;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_REQUEST_TIME) + 2);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s?", SARA_R5_GNSS_REQUEST_TIME);

  response = sara_r5_calloc_char(48);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                response, SARA_R5_10_SEC_TIMEOUT);

  // Response format: \r\n+UTIME: <mode>[,<sensor>]\r\n\r\nOK\r\n
  if (err == SARA_R5_ERROR_SUCCESS)
  {
      int scanned = sscanf(response, "\r\n+UTIME: %d,%d\r\n", &m, &s);
      if (scanned == 2)
      {
          *mode = m;
          *sensor = s;
      }
      else if (scanned == 1)
      {
          *mode = m;
          *sensor = SARA_R5_UTIME_SENSOR_NONE;
      }
      else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);
  return err;
}

SARA_R5_error_t SARA_R5::setUtimeIndication(SARA_R5_utime_urc_configuration_t config)
{
  SARA_R5_error_t err;
  char *command;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_TIME_INDICATION) + 3);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s=%d", SARA_R5_GNSS_TIME_INDICATION, config);

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
  free(command);
  return err;
}

SARA_R5_error_t SARA_R5::getUtimeIndication(SARA_R5_utime_urc_configuration_t *config)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  SARA_R5_utime_urc_configuration_t c;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_TIME_INDICATION) + 2);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s?", SARA_R5_GNSS_TIME_INDICATION);

  response = sara_r5_calloc_char(48);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

  // Response format: \r\n+UTIMEIND: <mode>\r\n\r\nOK\r\n
  if (err == SARA_R5_ERROR_SUCCESS)
  {
      int scanned = sscanf(response, "\r\n+UTIMEIND: %d\r\n", &c);
      if (scanned == 1)
      {
          *config = c;
      }
      else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);
  return err;
}

SARA_R5_error_t SARA_R5::setUtimeConfiguration(int32_t offsetNanoseconds, int32_t offsetSeconds)
{
  SARA_R5_error_t err;
  char *command;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_TIME_CONFIGURATION) + 48);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s=%d,%d", SARA_R5_GNSS_TIME_CONFIGURATION, offsetNanoseconds, offsetSeconds);

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
  free(command);
  return err;
}

SARA_R5_error_t SARA_R5::getUtimeConfiguration(int32_t *offsetNanoseconds, int32_t *offsetSeconds)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  int32_t ons;
  int32_t os;

  command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_TIME_CONFIGURATION) + 2);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s?", SARA_R5_GNSS_TIME_CONFIGURATION);

  response = sara_r5_calloc_char(48);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

  // Response format: \r\n+UTIMECFG: <offset_nano>,<offset_sec>\r\n\r\nOK\r\n
  if (err == SARA_R5_ERROR_SUCCESS)
  {
      int scanned = sscanf(response, "\r\n+UTIMECFG: %d,%d\r\n", &ons, &os);
      if (scanned == 2)
      {
          *offsetNanoseconds = ons;
          *offsetSeconds = os;
      }
      else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);
  return err;
}

SARA_R5_error_t SARA_R5::autoTimeZone(boolean enable)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_AUTO_TZ) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_COMMAND_AUTO_TZ, enable ? 1 : 0);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    free(command);
    return err;
}

int8_t SARA_R5::rssi(void)
{
    char *command;
    char *response;
    SARA_R5_error_t err;
    int rssi;

    command = sara_r5_calloc_char(strlen(SARA_R5_SIGNAL_QUALITY) + 1);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s", SARA_R5_SIGNAL_QUALITY);

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command,
                                  SARA_R5_RESPONSE_OK, response, 10000, AT_COMMAND);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return -1;
    }

    if (sscanf(response, "\r\n+CSQ: %d,%*d", &rssi) != 1)
    {
        rssi = -1;
    }

    free(command);
    free(response);
    return rssi;
}

SARA_R5_registration_status_t SARA_R5::registration(void)
{
    char *command;
    char *response;
    SARA_R5_error_t err;
    int status;

    command = sara_r5_calloc_char(strlen(SARA_R5_REGISTRATION_STATUS) + 2);
    if (command == NULL)
        return SARA_R5_REGISTRATION_INVALID;
    sprintf(command, "%s?", SARA_R5_REGISTRATION_STATUS);

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_REGISTRATION_INVALID;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT, AT_COMMAND);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return SARA_R5_REGISTRATION_INVALID;
    }

    if (sscanf(response, "\r\n+CREG: %*d,%d", &status) != 1)
    {
        status = SARA_R5_REGISTRATION_INVALID;
    }
    free(command);
    free(response);
    return (SARA_R5_registration_status_t)status;
}

boolean SARA_R5::setNetworkProfile(mobile_network_operator_t mno, boolean autoReset, boolean urcNotification)
{
    mobile_network_operator_t currentMno;

    // Check currently set MNO profile
    if (getMNOprofile(&currentMno) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    if (currentMno == mno)
    {
        return true;
    }

    // Disable transmit and receive so we can change operator
    if (functionality(MINIMUM_FUNCTIONALITY) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    if (setMNOprofile(mno, autoReset, urcNotification) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    if (reset() != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    return true;
}

mobile_network_operator_t SARA_R5::getNetworkProfile(void)
{
    mobile_network_operator_t mno;
    SARA_R5_error_t err;

    err = getMNOprofile(&mno);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        return MNO_INVALID;
    }
    return mno;
}

SARA_R5_error_t SARA_R5::setAPN(String apn, uint8_t cid, SARA_R5_pdp_type pdpType)
{
    SARA_R5_error_t err;
    char *command;
    char pdpStr[8];

    memset(pdpStr, 0, 8);

    if (cid >= 8)
        return SARA_R5_ERROR_UNEXPECTED_PARAM;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_DEF) + strlen(apn.c_str()) + 16);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    switch (pdpType)
    {
    case PDP_TYPE_INVALID:
        free(command);
        return SARA_R5_ERROR_UNEXPECTED_PARAM;
        break;
    case PDP_TYPE_IP:
        memcpy(pdpStr, "IP", 2);
        break;
    case PDP_TYPE_NONIP:
        memcpy(pdpStr, "NONIP", 2);
        break;
    case PDP_TYPE_IPV4V6:
        memcpy(pdpStr, "IPV4V6", 2);
        break;
    case PDP_TYPE_IPV6:
        memcpy(pdpStr, "IPV6", 2);
        break;
    default:
        free(command);
        return SARA_R5_ERROR_UNEXPECTED_PARAM;
        break;
    }
    if (apn == NULL)
    {
      if (_printDebug == true) _debugPort->println("APN: NULL");
      sprintf(command, "%s=%d,\"%s\",\"\"", SARA_R5_MESSAGE_PDP_DEF,
            cid, pdpStr);
    }
    else
    {
      if (_printDebug == true) _debugPort->println("APN: " + ((String)apn));
      sprintf(command, "%s=%d,\"%s\",\"%s\"", SARA_R5_MESSAGE_PDP_DEF,
            cid, pdpStr, apn.c_str());
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

// Return the Access Point Name and IP address for the chosen context identifier
SARA_R5_error_t SARA_R5::getAPN(int cid, String *apn, IPAddress *ip)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    int ipOctets[4];
    int rcid;

    if (cid > SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_DEF) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_MESSAGE_PDP_DEF);

    response = sara_r5_calloc_char(1024);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
      // Example:
      // +CGDCONT: 0,"IP","payandgo.o2.co.uk","0.0.0.0",0,0,0,0,0,0,0,0,0,0
      // +CGDCONT: 1,"IP","payandgo.o2.co.uk.mnc010.mcc234.gprs","10.160.182.234",0,0,0,2,0,0,0,0,0,0

      char *searchPtr = response;

      boolean keepGoing = true;
      while (keepGoing == true)
      {
        int apnLen = 0;
        int scanned = 0;
        // Find the first/next occurrence of +CGDCONT:
        searchPtr = strstr(searchPtr, "+CGDCONT: ");
        if (searchPtr != NULL)
        {
          searchPtr += strlen("+CGDCONT: "); // Point to the cid
          rcid = (*searchPtr) - '0'; // Get the first/only digit of cid
          searchPtr++;
          if (*searchPtr != ',') // Get the second digit of cid - if there is one
          {
            rcid *= 10;
            rcid += (*searchPtr) - '0';
          }
          if (_printDebug == true) _debugPort->println("getAPN: cid is " + ((String)rcid));
          if (rcid == cid) // If we have a match
          {
            // Search to the third double-quote
            for (int i = 0; i < 3; i++)
            {
                searchPtr = strchr(++searchPtr, '\"');
            }
            if (searchPtr != NULL)
            {
                // Fill in the APN:
                //searchPtr = strchr(searchPtr, '\"'); // Move to first quote
                while ((*(++searchPtr) != '\"') && (*searchPtr != '\0'))
                {
                    apn->concat(*(searchPtr));
                    apnLen++;
                }
                // Now get the IP:
                if (searchPtr != NULL)
                {
                    scanned = sscanf(searchPtr, "\",\"%d.%d.%d.%d\"",
                                         &ipOctets[0], &ipOctets[1], &ipOctets[2], &ipOctets[3]);
                    if (scanned == 4)
                    {
                        for (int octet = 0; octet < 4; octet++)
                        {
                            (*ip)[octet] = (uint8_t)ipOctets[octet];
                        }
                    }
                }
            }
          }
          else // We don't have a match so let's clear the APN and IP address
          {
            *apn = "";
            *ip = (0,0,0,0);
          }
        }
        if ((rcid == cid) || (searchPtr == NULL) || (*searchPtr == '\0')) // Stop searching
        {
          keepGoing = false;
        }
      }
    }
    else
    {
        err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::setSIMstateReportingMode(int mode)
{
  SARA_R5_error_t err;
  char *command;

  command = sara_r5_calloc_char(strlen(SARA_R5_SIM_STATE) + 4);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s=%d", SARA_R5_SIM_STATE, mode);

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
  free(command);
  return err;
}

SARA_R5_error_t SARA_R5::getSIMstateReportingMode(int *mode)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  int m;

  command = sara_r5_calloc_char(strlen(SARA_R5_SIM_STATE) + 3);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s?", SARA_R5_SIM_STATE);

  response = sara_r5_calloc_char(48);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

  if (err == SARA_R5_ERROR_SUCCESS)
  {
      int scanned = sscanf(response, "\r\n+USIMSTAT: %d\r\n", &m);
      if (scanned == 1)
      {
          *mode = m;
      }
      else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);
  return err;
}

const char *PPP_L2P[5] = {
    "",
    "PPP",
    "M-HEX",
    "M-RAW_IP",
    "M-OPT-PPP",
};

SARA_R5_error_t SARA_R5::enterPPP(uint8_t cid, char dialing_type_char,
                                        unsigned long dialNumber, SARA_R5::SARA_R5_l2p_t l2p)
{
    SARA_R5_error_t err;
    char *command;

    if ((dialing_type_char != 0) && (dialing_type_char != 'T') &&
        (dialing_type_char != 'P'))
    {
        return SARA_R5_ERROR_UNEXPECTED_PARAM;
    }

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_ENTER_PPP) + 32);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (dialing_type_char != 0)
    {
        sprintf(command, "%s%c*%lu**%s*%hhu#", SARA_R5_MESSAGE_ENTER_PPP, dialing_type_char,
                dialNumber, PPP_L2P[l2p], cid);
    }
    else
    {
        sprintf(command, "%s*%lu**%s*%hhu#", SARA_R5_MESSAGE_ENTER_PPP,
                dialNumber, PPP_L2P[l2p], cid);
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

uint8_t SARA_R5::getOperators(struct operator_stats *opRet, int maxOps)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    uint8_t opsSeen = 0;

    command = sara_r5_calloc_char(strlen(SARA_R5_OPERATOR_SELECTION) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=?", SARA_R5_OPERATOR_SELECTION);

    response = sara_r5_calloc_char((maxOps + 1) * 48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    // AT+COPS maximum response time is 3 minutes (180000 ms)
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_3_MIN_TIMEOUT);

    // Sample responses:
    // +COPS: (3,"Verizon Wireless","VzW","311480",8),,(0,1,2,3,4),(0,1,2)
    // +COPS: (1,"313 100","313 100","313100",8),(2,"AT&T","AT&T","310410",8),(3,"311 480","311 480","311480",8),,(0,1,2,3,4),(0,1,2)

    if (_printDebug == true)
    {
      _debugPort->print("getOperators: Response: {");
      _debugPort->print(response);
      _debugPort->println("}");
    }

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        char *opBegin;
        char *opEnd;
        int op = 0;
        int sscanRead = 0;
        int stat;
        char longOp[26];
        char shortOp[11];
        int act;
        unsigned long numOp;

        opBegin = response;

        for (; op < maxOps; op++)
        {
            opBegin = strchr(opBegin, '(');
            if (opBegin == NULL)
                break;
            opEnd = strchr(opBegin, ')');
            if (opEnd == NULL)
                break;

            int sscanRead = sscanf(opBegin, "(%d,\"%[^\"]\",\"%[^\"]\",\"%lu\",%d)%*s",
                                   &stat, longOp, shortOp, &numOp, &act);
            if (sscanRead == 5)
            {
                opRet[op].stat = stat;
                opRet[op].longOp = (String)(longOp);
                opRet[op].shortOp = (String)(shortOp);
                opRet[op].numOp = numOp;
                opRet[op].act = act;
                opsSeen += 1;
            }
            // TODO: Search for other possible patterns here
            else
            {
                break; // Break out if pattern doesn't match.
            }
            opBegin = opEnd + 1; // Move opBegin to beginning of next value
        }
    }

    free(command);
    free(response);

    return opsSeen;
}

SARA_R5_error_t SARA_R5::registerOperator(struct operator_stats oper)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_OPERATOR_SELECTION) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=1,2,\"%lu\"", SARA_R5_OPERATOR_SELECTION, oper.numOp);

    // AT+COPS maximum response time is 3 minutes (180000 ms)
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_3_MIN_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::automaticOperatorSelection()
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_OPERATOR_SELECTION) + 6);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=0,0", SARA_R5_OPERATOR_SELECTION);

    // AT+COPS maximum response time is 3 minutes (180000 ms)
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_3_MIN_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::getOperator(String *oper)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *searchPtr;
    char mode;

    command = sara_r5_calloc_char(strlen(SARA_R5_OPERATOR_SELECTION) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_OPERATOR_SELECTION);

    response = sara_r5_calloc_char(64);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    // AT+COPS maximum response time is 3 minutes (180000 ms)
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_3_MIN_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        searchPtr = strstr(response, "+COPS: ");
        if (searchPtr != NULL)
        {
            searchPtr += strlen("+COPS: "); //  Move searchPtr to first char
            mode = *searchPtr;              // Read first char -- should be mode
            if (mode == '2')                // Check for de-register
            {
                err = SARA_R5_ERROR_DEREGISTERED;
            }
            // Otherwise if it's default, manual, set-only, or automatic
            else if ((mode == '0') || (mode == '1') || (mode == '3') || (mode == '4'))
            {
                *oper = "";
                searchPtr = strchr(searchPtr, '\"'); // Move to first quote
                if (searchPtr == NULL)
                {
                    err = SARA_R5_ERROR_DEREGISTERED;
                }
                else
                {
                    while ((*(++searchPtr) != '\"') && (*searchPtr != '\0'))
                    {
                        oper->concat(*(searchPtr));
                    }
                }
                if (_printDebug == true) _debugPort->println("Operator: " + *oper);
                //oper->concat('\0');
            }
        }
    }

    free(response);
    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::deregisterOperator(void)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_OPERATOR_SELECTION) + 4);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=2", SARA_R5_OPERATOR_SELECTION);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_3_MIN_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setSMSMessageFormat(SARA_R5_message_format_t textMode)
{
    char *command;
    SARA_R5_error_t err;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_FORMAT) + 4);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_MESSAGE_FORMAT,
            (textMode == SARA_R5_MESSAGE_FORMAT_TEXT) ? 1 : 0);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::sendSMS(String number, String message)
{
    char *command;
    char *messageCStr;
    char *numberCStr;
    int messageIndex;
    SARA_R5_error_t err;

    numberCStr = sara_r5_calloc_char(number.length() + 2);
    if (numberCStr == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    number.toCharArray(numberCStr, number.length() + 1);

    command = sara_r5_calloc_char(strlen(SARA_R5_SEND_TEXT) + strlen(numberCStr) + 8);
    if (command != NULL)
    {
        sprintf(command, "%s=\"%s\"", SARA_R5_SEND_TEXT, numberCStr);

        err = sendCommandWithResponse(command, ">", NULL,
                                      SARA_R5_3_MIN_TIMEOUT);
        free(command);
        free(numberCStr);
        if (err != SARA_R5_ERROR_SUCCESS)
            return err;

        messageCStr = sara_r5_calloc_char(message.length() + 1);
        if (messageCStr == NULL)
        {
            hwWrite(ASCII_CTRL_Z);
            return SARA_R5_ERROR_OUT_OF_MEMORY;
        }
        message.toCharArray(messageCStr, message.length() + 1);
        messageCStr[message.length()] = ASCII_CTRL_Z;

        err = sendCommandWithResponse(messageCStr, SARA_R5_RESPONSE_OK,
                                      NULL, SARA_R5_3_MIN_TIMEOUT, NOT_AT_COMMAND);

        free(messageCStr);
    }
    else
    {
        free(numberCStr);
        err = SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    return err;
}

SARA_R5_error_t SARA_R5::getPreferredMessageStorage(int *used, int *total, String memory)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    int u;
    int t;

    command = sara_r5_calloc_char(strlen(SARA_R5_PREF_MESSAGE_STORE) + 6);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=\"%s\"", SARA_R5_PREF_MESSAGE_STORE, memory.c_str());

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_3_MIN_TIMEOUT);

    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return err;
    }

    int ret = sscanf(response, "\r\n+CPMS: %d,%d", &u, &t);
		if (ret == 2)
		{
			if (_printDebug == true)
      {
        _debugPort->print("getPreferredMessageStorage: memory: ");
        _debugPort->print(memory);
        _debugPort->print(" used: ");
        _debugPort->print(u);
        _debugPort->print(" total: ");
        _debugPort->println(t);
      }
      *used = u;
      *total = t;
		}
    else
    {
      err = SARA_R5_ERROR_INVALID;
    }

    free(response);
    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::readSMSmessage(int location, String *unread, String *from, String *dateTime, String *message)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  command = sara_r5_calloc_char(strlen(SARA_R5_READ_TEXT_MESSAGE) + 5);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s=%d", SARA_R5_READ_TEXT_MESSAGE, location);

  response = sara_r5_calloc_char(1024);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                SARA_R5_10_SEC_TIMEOUT);

  if (err == SARA_R5_ERROR_SUCCESS)
  {
    char *searchPtr = response;

    // Find the first occurrence of +CGDCONT:
    searchPtr = strstr(searchPtr, "+CMGR: ");
    if (searchPtr != NULL)
    {
      searchPtr += strlen("+CMGR: "); // Point to the originator address
      int pointer = 0;
      while ((*(++searchPtr) != '\"') && (*searchPtr != '\0') && (pointer < 12))
      {
        unread->concat(*(searchPtr));
        pointer++;
      }
      if ((*searchPtr == '\0') || (pointer == 12))
      {
        free(command);
        free(response);
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
      }
      // Search to the next quote
      searchPtr = strchr(++searchPtr, '\"');
      pointer = 0;
      while ((*(++searchPtr) != '\"') && (*searchPtr != '\0') && (pointer < 24))
      {
        from->concat(*(searchPtr));
        pointer++;
      }
      if ((*searchPtr == '\0') || (pointer == 24))
      {
        free(command);
        free(response);
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
      }
      // Skip two commas
      searchPtr = strchr(++searchPtr, ',');
      searchPtr = strchr(++searchPtr, ',');
      // Search to the next quote
      searchPtr = strchr(++searchPtr, '\"');
      pointer = 0;
      while ((*(++searchPtr) != '\"') && (*searchPtr != '\0') && (pointer < 24))
      {
        dateTime->concat(*(searchPtr));
        pointer++;
      }
      if ((*searchPtr == '\0') || (pointer == 24))
      {
        free(command);
        free(response);
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
      }
      // Search to the next new line
      searchPtr = strchr(++searchPtr, '\n');
      pointer = 0;
      while ((*(++searchPtr) != '\r') && (*searchPtr != '\n') && (*searchPtr != '\0') && (pointer < 512))
      {
        message->concat(*(searchPtr));
        pointer++;
      }
      if ((*searchPtr == '\0') || (pointer == 512))
      {
        free(command);
        free(response);
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
      }
    }
    else
    {
      err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }
  }
  else
  {
      err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);

  return err;
}

SARA_R5_error_t SARA_R5::setBaud(unsigned long baud)
{
    SARA_R5_error_t err;
    char *command;
    int b = 0;

    // Error check -- ensure supported baud
    for (; b < NUM_SUPPORTED_BAUD; b++)
    {
        if (SARA_R5_SUPPORTED_BAUD[b] == baud)
        {
            break;
        }
    }
    if (b >= NUM_SUPPORTED_BAUD)
    {
        return SARA_R5_ERROR_UNEXPECTED_PARAM;
    }

    // Construct command
    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_BAUD) + 7 + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%lu", SARA_R5_COMMAND_BAUD, baud);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_SET_BAUD_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::setFlowControl(SARA_R5_flow_control_t value)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_FLOW_CONTROL) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s%d", SARA_R5_FLOW_CONTROL, value);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::setGpioMode(SARA_R5_gpio_t gpio,
                                SARA_R5_gpio_mode_t mode, int value)
{
    SARA_R5_error_t err;
    char *command;

    // Example command: AT+UGPIOC=16,2
    // Example command: AT+UGPIOC=23,0,1
    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_GPIO) + 10);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (mode == GPIO_OUTPUT)
        sprintf(command, "%s=%d,%d,%d", SARA_R5_COMMAND_GPIO, gpio, mode, value);
    else
        sprintf(command, "%s=%d,%d", SARA_R5_COMMAND_GPIO, gpio, mode);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_10_SEC_TIMEOUT);

    free(command);

    return err;
}

SARA_R5::SARA_R5_gpio_mode_t SARA_R5::getGpioMode(SARA_R5_gpio_t gpio)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char gpioChar[4];
    char *gpioStart;
    int gpioMode;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_GPIO) + 2);
    if (command == NULL)
        return GPIO_MODE_INVALID;
    sprintf(command, "%s?", SARA_R5_COMMAND_GPIO);

    response = sara_r5_calloc_char(96);
    if (response == NULL)
    {
        free(command);
        return GPIO_MODE_INVALID;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return GPIO_MODE_INVALID;
    }

    sprintf(gpioChar, "%d", gpio);          // Convert GPIO to char array
    gpioStart = strstr(response, gpioChar); // Find first occurence of GPIO in response

    free(command);
    free(response);

    if (gpioStart == NULL)
        return GPIO_MODE_INVALID; // If not found return invalid
    sscanf(gpioStart, "%*d,%d\r\n", &gpioMode);

    return (SARA_R5_gpio_mode_t)gpioMode;
}

int SARA_R5::socketOpen(SARA_R5_socket_protocol_t protocol, unsigned int localPort)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    int sockId = -1;
    char *responseStart;

    command = sara_r5_calloc_char(strlen(SARA_R5_CREATE_SOCKET) + 10);
    if (command == NULL)
        return -1;
    sprintf(command, "%s=%d,%d", SARA_R5_CREATE_SOCKET, protocol, localPort);

    response = sara_r5_calloc_char(128);
    if (response == NULL)
    {
        if (_printDebug == true) _debugPort->println("Socket Open Failure: NULL response.");
        free(command);
        return -1;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err != SARA_R5_ERROR_SUCCESS)
    {
        if (_printDebug == true)
        {
          _debugPort->print("Socket Open Failure: ");
          _debugPort->println(err);
          _debugPort->println("Response: {");
          _debugPort->println(response);
          _debugPort->println("}");
        }
        free(command);
        free(response);
        return -1;
    }

    responseStart = strstr(response, "+USOCR");
    if (responseStart == NULL)
    {
        if (_printDebug == true)
        {
          _debugPort->print("Socket Open Failure: {");
          _debugPort->print(response);
          _debugPort->println("}");
        }
        free(command);
        free(response);
        return -1;
    }

    sscanf(responseStart, "+USOCR: %d", &sockId);

    free(command);
    free(response);

    return sockId;
}

SARA_R5_error_t SARA_R5::socketClose(int socket, int timeout)
{
    SARA_R5_error_t err;
    char *command;
    char *response;

    command = sara_r5_calloc_char(strlen(SARA_R5_CLOSE_SOCKET) + 10);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    response = sara_r5_calloc_char(128);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }
    sprintf(command, "%s=%d", SARA_R5_CLOSE_SOCKET, socket);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response, timeout);

    if ((err != SARA_R5_ERROR_SUCCESS) && (_printDebug == true))
    {
  		_debugPort->print("Socket Close Error Code: ");
  		_debugPort->println(socketGetLastError());
  	}

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::socketConnect(int socket, const char *address,
                                             unsigned int port)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_CONNECT_SOCKET) + strlen(address) + 11);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,\"%s\",%d", SARA_R5_CONNECT_SOCKET, socket, address, port);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, SARA_R5_IP_CONNECT_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::socketWrite(int socket, const char *str)
{
    char *command;
    char *response;
    SARA_R5_error_t err;
    unsigned long writeDelay;

    command = sara_r5_calloc_char(strlen(SARA_R5_WRITE_SOCKET) + 8);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    response = sara_r5_calloc_char(128);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }
    sprintf(command, "%s=%d,%d", SARA_R5_WRITE_SOCKET, socket, strlen(str));

    err = sendCommandWithResponse(command, "@", response,
                                  SARA_R5_2_MIN_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
  		writeDelay = millis();
      while (millis() - writeDelay < 50); //uBlox specification says to wait 50ms after receiving "@" to write data.

  		hwPrint(str);
  		err = waitForResponse(SARA_R5_RESPONSE_OK, SARA_R5_RESPONSE_ERROR, SARA_R5_SOCKET_WRITE_TIMEOUT);
  	}
    else
    {
      if (_printDebug == true)
      {
        _debugPort->print("WriteCmd Err Response: ");
  		  _debugPort->print(err);
  		  _debugPort->print(" => {");
  		  _debugPort->print(response);
  		  _debugPort->println("}");
      }
  	}

    free(command);
    free(response);
    return err;
}

SARA_R5_error_t SARA_R5::socketWrite(int socket, String str)
{
    return socketWrite(socket, str.c_str());
}

SARA_R5_error_t SARA_R5::socketWriteUDP(int socket, const char *address, int port, const char *str, int len){
	char *command;
	char *response;
	SARA_R5_error_t err;
	int dataLen = len == -1 ? strlen(str) : len;

	command = sara_r5_calloc_char(64);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  response = sara_r5_calloc_char(128);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

	sprintf(command, "%s=%d,\"%s\",%d,%d", SARA_R5_WRITE_UDP_SOCKET,
										socket, address, port, dataLen);
	err = sendCommandWithResponse(command, "@", response, SARA_R5_IP_CONNECT_TIMEOUT);

	if (err == SARA_R5_ERROR_SUCCESS)
  {
		if (len == -1) //If binary data we need to send a length.
    {
			hwPrint(str);
		}
    else
    {
			hwWriteData(str, len);
		}
		err = waitForResponse(SARA_R5_RESPONSE_OK, SARA_R5_RESPONSE_ERROR, SARA_R5_SOCKET_WRITE_TIMEOUT);
	}
  else
  {
    if (_printDebug == true) _debugPort->print("UDP Write Error: ");
		if (_printDebug == true) _debugPort->println(socketGetLastError());
	}

	free(command);
	free(response);
	return err;
}

SARA_R5_error_t SARA_R5::socketWriteUDP(int socket, String address, int port, String str, int len){
	return socketWriteUDP(socket, address.c_str(), port, str.c_str(), len);
}

SARA_R5_error_t SARA_R5::socketRead(int socket, int length, char *readDest)
{
    char *command;
    char *response;
    char *strBegin;
    int readIndex = 0;
    SARA_R5_error_t err;

    command = sara_r5_calloc_char(strlen(SARA_R5_READ_SOCKET) + 8);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_READ_SOCKET, socket, length);

    response = sara_r5_calloc_char(length + strlen(SARA_R5_READ_SOCKET) + 128);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Find the first double-quote:
        strBegin = strchr(response, '\"');

        if (strBegin == NULL)
        {
            free(command);
            free(response);
            return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
        }

        while ((readIndex < length) && (readIndex < strlen(strBegin)))
        {
            readDest[readIndex] = strBegin[1 + readIndex];
            readIndex += 1;
        }
    }

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::socketReadUDP(int socket, int length, char *readDest)
{
    char *command;
    char *response;
    char *strBegin;
    int readIndex = 0;
    SARA_R5_error_t err;

    command = sara_r5_calloc_char(strlen(SARA_R5_READ_UDP_SOCKET) + 16);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_READ_UDP_SOCKET, socket, length);

    response = sara_r5_calloc_char(length + strlen(SARA_R5_READ_UDP_SOCKET) + 128);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

	  if (err == SARA_R5_ERROR_SUCCESS)
    {
      // Find the third double-quote. This needs to be improved to collect other data.
      if (_printDebug == true) _debugPort->print("UDP READ: {");
      if (_printDebug == true) _debugPort->print(response);
      if (_printDebug == true) _debugPort->println("}");

      strBegin = strchr(response, '\"');
		  strBegin = strchr(strBegin+1, '\"');
		  strBegin = strchr(strBegin+1, '\"');

      if (strBegin == NULL)
      {
          free(command);
          free(response);
          return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
      }

      while ((readIndex < length) && (readIndex < strlen(strBegin)))
      {
          readDest[readIndex] = strBegin[1 + readIndex];
          readIndex += 1;
      }
    }

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::socketListen(int socket, unsigned int port)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_LISTEN_SOCKET) + 9);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_LISTEN_SOCKET, socket, port);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

//Issues command to get last socket error, then prints to serial. Also updates rx/backlog buffers.
int SARA_R5::socketGetLastError()
{
	SARA_R5_error_t err;
	char *command;
	char *response;
	int errorCode = -1;

	command=sara_r5_calloc_char(64);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;

  response=sara_r5_calloc_char(128);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

	sprintf(command, "%s", SARA_R5_GET_ERROR);

	err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
									SARA_R5_STANDARD_RESPONSE_TIMEOUT);

	if (err == SARA_R5_ERROR_SUCCESS)
  {
		sscanf(response, "+USOER: %d", &errorCode);
	}

	free(command);
	free(response);

	return errorCode;
}

IPAddress SARA_R5::lastRemoteIP(void)
{
    return _lastRemoteIP;
}

SARA_R5_error_t SARA_R5::resetHTTPprofile(int profile)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_HTTP_PROFILE, profile);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPserverIPaddress(int profile, IPAddress address)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 32);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%d.%d.%d.%d\"", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_SERVER_IP,
            address[0], address[1], address[2], address[3]);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPserverName(int profile, String server)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12 + server.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\"", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_SERVER_NAME,
            server.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPusername(int profile, String username)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12 + username.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\"", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_USERNAME,
            username.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPpassword(int profile, String password)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12 + password.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\"", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_PASSWORD,
            password.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPauthentication(int profile, boolean authenticate)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,%d", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_AUTHENTICATION,
            authenticate);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPserverPort(int profile, int port)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,%d", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_SERVER_PORT,
            port);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setHTTPsecure(int profile, boolean secure)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROFILE) + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,%d", SARA_R5_HTTP_PROFILE, profile, SARA_R5_HTTP_OP_CODE_SECURE,
            secure);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::ping(String remote_host, int retry, int p_size,
                          unsigned long timeout, int ttl)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_PING_COMMAND) + 48 +
            remote_host.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=\"%s\",%d,%d,%d,%d", SARA_R5_PING_COMMAND,
            remote_host.c_str(), retry, p_size, timeout, ttl);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::sendHTTPGET(int profile, String path, String responseFilename)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_COMMAND) + 24 +
            path.length() + responseFilename.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\",\"%s\"", SARA_R5_HTTP_COMMAND, profile, SARA_R5_HTTP_COMMAND_GET,
            path.c_str(), responseFilename.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::sendHTTPPOSTdata(int profile, String path, String responseFilename,
            String data, SARA_R5_http_content_types_t httpContentType)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_HTTP_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_COMMAND) + 24 +
            path.length() + responseFilename.length() + data.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\",\"%s\",\"%s\",%d", SARA_R5_HTTP_COMMAND, profile, SARA_R5_HTTP_COMMAND_POST_DATA,
            path.c_str(), responseFilename.c_str(), data.c_str(), httpContentType);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::getHTTPprotocolError(int profile, int *error_class, int *error_code)
{
  SARA_R5_error_t err;
  char *command;
  char *response;

  int rprofile, eclass, ecode;

  command = sara_r5_calloc_char(strlen(SARA_R5_HTTP_PROTOCOL_ERROR) + 4);
  if (command == NULL)
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  sprintf(command, "%s=%d", SARA_R5_HTTP_PROTOCOL_ERROR, profile);

  response = sara_r5_calloc_char(48);
  if (response == NULL)
  {
      free(command);
      return SARA_R5_ERROR_OUT_OF_MEMORY;
  }

  err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

  if (err == SARA_R5_ERROR_SUCCESS)
  {
      if (sscanf(response, "\r\n+UHTTPER: %d,%d,%d\r\n",
                 &rprofile, &eclass, &ecode) == 3)
      {
          *error_class = eclass;
          *error_code = ecode;
      }
      else err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

  free(command);
  free(response);
  return err;
}

SARA_R5_error_t SARA_R5::setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, int value)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_PSD_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_CONFIG) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,%d", SARA_R5_MESSAGE_PDP_CONFIG, profile, parameter,
            value);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, SARA_R5_pdp_protocol_type_t value)
{
    return (setPDPconfiguration(profile, parameter, (int)value));
}

SARA_R5_error_t SARA_R5::setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, String value)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_PSD_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_CONFIG) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%s\"", SARA_R5_MESSAGE_PDP_CONFIG, profile, parameter,
            value.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, IPAddress value)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_PSD_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_CONFIG) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d,\"%d.%d.%d.%d\"", SARA_R5_MESSAGE_PDP_CONFIG, profile, parameter,
            value[0], value[1], value[2], value[3]);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::performPDPaction(int profile, SARA_R5_pdp_actions_t action)
{
    SARA_R5_error_t err;
    char *command;

    if (profile >= SARA_R5_NUM_PSD_PROFILES)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_ACTION) + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_MESSAGE_PDP_ACTION, profile, action);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::activatePDPcontext(boolean status, int cid)
{
    SARA_R5_error_t err;
    char *command;

    if (cid >= SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS)
      return SARA_R5_ERROR_ERROR;

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_CONTEXT_ACTIVATE) + 12);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (cid == -1)
      sprintf(command, "%s=%d", SARA_R5_MESSAGE_PDP_CONTEXT_ACTIVATE, status);
    else
      sprintf(command, "%s=%d,%d", SARA_R5_MESSAGE_PDP_CONTEXT_ACTIVATE, status, cid);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    return err;
}

boolean SARA_R5::isGPSon(void)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    boolean on = false;

    command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_POWER) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_GNSS_POWER);

    response = sara_r5_calloc_char(24);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_10_SEC_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Example response: "+UGPS: 0" for off "+UGPS: 1,0,1" for on
        // Search for a ':' followed by a '1' or ' 1'
        char * pch1 = strchr(response, ':');
        if (pch1 != NULL)
        {
          char * pch2 = strchr(response, '1');
          if ((pch2 != NULL) && ((pch2 == pch1 + 1) || (pch2 == pch1 + 2)))
            on = true;
        }
    }

    free(command);
    free(response);

    return on;
}

SARA_R5_error_t SARA_R5::gpsPower(boolean enable, gnss_system_t gnss_sys, gnss_aiding_mode_t gnss_aiding)
{
    SARA_R5_error_t err;
    char *command;
    boolean gpsState;

    // Don't turn GPS on/off if it's already on/off
    gpsState = isGPSon();
    if ((enable && gpsState) || (!enable && !gpsState))
    {
        return SARA_R5_ERROR_SUCCESS;
    }

    // GPS power management
    command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_POWER) + 10); // gnss_sys could be up to three digits
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (enable)
    {
        sprintf(command, "%s=1,%d,%d", SARA_R5_GNSS_POWER, gnss_aiding, gnss_sys);
    }
    else
    {
        sprintf(command, "%s=0", SARA_R5_GNSS_POWER);
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, 10000);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnableClock(boolean enable)
{
    // AT+UGZDA=<0,1>
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetClock(struct ClockData *clock)
{
    // AT+UGZDA?
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnableFix(boolean enable)
{
    // AT+UGGGA=<0,1>
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetFix(struct PositionData *pos)
{
    // AT+UGGGA?
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnablePos(boolean enable)
{
    // AT+UGGLL=<0,1>
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetPos(struct PositionData *pos)
{
    // AT+UGGLL?
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnableSat(boolean enable)
{
    // AT+UGGSV=<0,1>
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetSat(uint8_t *sats)
{
    // AT+UGGSV?
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnableRmc(boolean enable)
{
    // AT+UGRMC=<0,1>
    SARA_R5_error_t err;
    char *command;

    // ** Don't call gpsPower here. It causes problems for +UTIME and the PPS signal **
    // ** Call isGPSon and gpsPower externally if required **
    // if (!isGPSon())
    // {
    //     err = gpsPower(true);
    //     if (err != SARA_R5_ERROR_SUCCESS)
    //     {
    //         return err;
    //     }
    // }

    command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_GPRMC) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_GNSS_GPRMC, enable ? 1 : 0);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, SARA_R5_10_SEC_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetRmc(struct PositionData *pos, struct SpeedData *spd,
                                         struct ClockData *clk, boolean *valid)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *rmcBegin;

    command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_GPRMC) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_GNSS_GPRMC);

    response = sara_r5_calloc_char(96);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response, SARA_R5_10_SEC_TIMEOUT);
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Fast-forward response string to $GPRMC starter
        rmcBegin = strstr(response, "$GPRMC");
        if (rmcBegin == NULL)
        {
            err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
        }
        else
        {
            *valid = parseGPRMCString(rmcBegin, pos, clk, spd);
        }
    }

    free(command);
    free(response);
    return err;
}

SARA_R5_error_t SARA_R5::gpsEnableSpeed(boolean enable)
{
    // AT+UGVTG=<0,1>
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsGetSpeed(struct SpeedData *speed)
{
    // AT+UGVTG?
    SARA_R5_error_t err = SARA_R5_ERROR_SUCCESS;
    return err;
}

SARA_R5_error_t SARA_R5::gpsRequest(unsigned int timeout, uint32_t accuracy,
                                          boolean detailed)
{
    // AT+ULOC=2,<useCellLocate>,<detailed>,<timeout>,<accuracy>
    SARA_R5_error_t err;
    char *command;

    // This function will only work if the GPS module is initially turned off.
    if (isGPSon())
    {
        gpsPower(false);
    }

    if (timeout > 999)
        timeout = 999;
    if (accuracy > 999999)
        accuracy = 999999;

    command = sara_r5_calloc_char(strlen(SARA_R5_GNSS_REQUEST_LOCATION) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=2,3,%d,%d,%d", SARA_R5_GNSS_REQUEST_LOCATION,
            detailed ? 1 : 0, timeout, accuracy);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, SARA_R5_10_SEC_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::getFileContents(String filename, String *contents)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *contentsPtr;

    command = sara_r5_calloc_char(strlen(SARA_R5_FILE_SYSTEM_READ_FILE) + 8 + filename.length());
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=\"%s\"", SARA_R5_FILE_SYSTEM_READ_FILE, filename.c_str());

    response = sara_r5_calloc_char(1072); // Hopefully this should be way more than enough?! (1024 + 48 extra)
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return err;
    }

    // Response format: \r\n+URDFILE: "filename",36,"these bytes are the data of the file"\r\n\r\nOK\r\n
    contentsPtr = strchr(response, '\"'); // Find the third quote
    contentsPtr = strchr(contentsPtr+1, '\"');
    contentsPtr = strchr(contentsPtr+1, '\"');

    if (contentsPtr == NULL)
    {
        free(command);
        free(response);
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }

    boolean keepGoing = true;
    int bytesRead = 0;

    if (_printDebug == true) _debugPort->print(F("getFileContents: file contents are \""));

    while (keepGoing)
    {
      char c = *(++contentsPtr); // Increment contentsPtr then copy file char into c
      if ((c == '\0') || (c == '\"') || (bytesRead == 1024))
      {
        keepGoing = false;
      }
      else
      {
        bytesRead++;
        contents->concat(c); // Append c to contents
        if (_printDebug == true) _debugPort->print(c);
      }
    }
    if (_printDebug == true)
    {
      _debugPort->println(F("\""));
      _debugPort->print(F("getFileContents: total bytes read: "));
      _debugPort->println(bytesRead);
    }

    free(command);
    free(response);

    return SARA_R5_ERROR_SUCCESS;
}

/////////////
// Private //
/////////////

SARA_R5_error_t SARA_R5::init(unsigned long baud,
                                    SARA_R5::SARA_R5_init_type_t initType)
{
    SARA_R5_error_t err;

    //If we have recursively called init too many times, bail
    _currentInitDepth++;
    if (_currentInitDepth == _maxInitDepth)
    {
        if (_printDebug == true) _debugPort->println(F("Module failed to init. Exiting."));
        return (SARA_R5_ERROR_NO_RESPONSE);
    }

    if (_printDebug == true) _debugPort->println(F("Begin module init."));

    beginSerial(baud); // Begin serial

    if (initType == SARA_R5_INIT_AUTOBAUD)
    {
        if (_printDebug == true) _debugPort->println(F("Attempting autobaud connection to module."));
        if (autobaud(baud) != SARA_R5_ERROR_SUCCESS)
        {
            return init(baud, SARA_R5_INIT_RESET);
        }
    }
    else if (initType == SARA_R5_INIT_RESET)
    {
        if (_printDebug == true) _debugPort->println(F("Power cycling module."));
        powerOn();
        delay(1000);
        if (at() != SARA_R5_ERROR_SUCCESS)
        {
            return init(baud, SARA_R5_INIT_AUTOBAUD);
        }
    }

    // Use disable echo to test response
    err = enableEcho(false);

    if (err != SARA_R5_ERROR_SUCCESS)
    {
        if (_printDebug == true) _debugPort->println(F("Module failed echo test."));
        return init(baud, SARA_R5_INIT_AUTOBAUD);
    }

    if (_printDebug == true) _debugPort->println(F("Module responded successfully."));

    _baud = baud;
    setGpioMode(GPIO1, NETWORK_STATUS);
    //setGpioMode(GPIO2, GNSS_SUPPLY_ENABLE);
    setGpioMode(GPIO6, TIME_PULSE_OUTPUT);
    setSMSMessageFormat(SARA_R5_MESSAGE_FORMAT_TEXT);
    autoTimeZone(true);
    for (int i = 0; i < SARA_R5_NUM_SOCKETS; i++)
    {
        socketClose(i, 100);
    }

    return SARA_R5_ERROR_SUCCESS;
}

void SARA_R5::invertPowerPin(boolean invert)
{
  _invertPowerPin = invert;
}

void SARA_R5::powerOn(void)
{
  if (_powerPin >= 0)
  {
    if (_invertPowerPin) // Set the pin state before making it an output
      digitalWrite(_powerPin, HIGH);
    else
      digitalWrite(_powerPin, LOW);
    pinMode(_powerPin, OUTPUT);
    if (_invertPowerPin) // Set the pin state
      digitalWrite(_powerPin, HIGH);
    else
      digitalWrite(_powerPin, LOW);
    delay(SARA_R5_POWER_PULSE_PERIOD);
    pinMode(_powerPin, INPUT); // Return to high-impedance, rely on (e.g.) SARA module internal pull-up
    delay(2000);               //Wait before sending AT commands to module. 100 is too short.
    if (_printDebug == true) _debugPort->println(F("Power cycle complete."));
  }
}

void SARA_R5::hwReset(void)
{
  if (_resetPin >= 0)
  {
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);
    delay(SARA_R5_RESET_PULSE_PERIOD);
    pinMode(_resetPin, INPUT); // Return to high-impedance, rely on SARA module internal pull-up
  }
}

SARA_R5_error_t SARA_R5::functionality(SARA_R5_functionality_t function)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_FUNC) + 4);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_COMMAND_FUNC, function);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_3_MIN_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::setMNOprofile(mobile_network_operator_t mno, boolean autoReset, boolean urcNotification)
{
    SARA_R5_error_t err;
    char *command;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_MNO) + 9);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (mno == MNO_SIM_ICCID) // Only add autoReset and urcNotification if mno is MNO_SIM_ICCID
      sprintf(command, "%s=%d,%d,%d", SARA_R5_COMMAND_MNO, (uint8_t)mno, (uint8_t) autoReset, (uint8_t) urcNotification);
    else
      sprintf(command, "%s=%d", SARA_R5_COMMAND_MNO, (uint8_t)mno);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::getMNOprofile(mobile_network_operator_t *mno)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    mobile_network_operator_t o;
    mobile_network_operator_t d;
    int r;
    int u;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_MNO) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_COMMAND_MNO);

    response = sara_r5_calloc_char(48);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return err;
    }

    int ret = sscanf(response, "\r\n+UMNOPROF: %d,%d,%d,%d", &o, &d, &r, &u);
		if (ret >= 1)
		{
			if (_printDebug == true)
      {
        _debugPort->print("getMNOprofile: MNO is: ");
        _debugPort->println(o);
      }
      *mno = o;
		}
    else
    {
      err = SARA_R5_ERROR_INVALID;
    }

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::waitForResponse(const char *expectedResponse, const char *expectedError, uint16_t timeout)
{
  unsigned long timeIn;
  boolean found = false;
  int responseIndex = 0, errorIndex = 0;
  int backlogIndex = strlen(saraResponseBacklog);

  timeIn = millis();

  while ((!found) && ((timeIn + timeout) > millis()))
  {
    if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
    {
      char c = readChar();
      if (_printDebug == true) _debugPort->print((String)c);
      if (c == expectedResponse[responseIndex])
      {
          if (++responseIndex == strlen(expectedResponse))
          {
              found = true;
          }
      }
      else
      {
          responseIndex = 0;
      }
  		if (c == expectedError[errorIndex])
      {
  			if (++errorIndex == strlen(expectedError))
        {
  				found = true;
  			}
  		}
      else
      {
  			errorIndex = 0;
  		}
  		//This is a global array that holds the backlog of any events
  		//that came in while waiting for response. To be processed later within bufferedPoll().
  		saraResponseBacklog[backlogIndex++] = c;
    }
  }

	pruneBacklog();

	if (found == true)
  {
		if (errorIndex > 0)
    {
			return SARA_R5_ERROR_ERROR;
		}
    else if (responseIndex > 0)
    {
			return SARA_R5_ERROR_SUCCESS;
		}
	}

  return SARA_R5_ERROR_NO_RESPONSE;
}

SARA_R5_error_t SARA_R5::sendCommandWithResponse(
    const char *command, const char *expectedResponse, char *responseDest,
    unsigned long commandTimeout, boolean at)
{
  boolean found = false;
  int index = 0;
  int destIndex = 0;
  unsigned int charsRead = 0;

  if (_printDebug == true) _debugPort->print("Send Command: ");
  if (_printDebug == true) _debugPort->println(String(command));

	int backlogIndex = sendCommand(command, at);//Sending command needs to dump data to backlog buffer as well.
	unsigned long timeIn = millis();

	while ((!found) && ((timeIn + commandTimeout) > millis()))
  {
		if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
    {
			char c = readChar();
      if (_printDebug == true) _debugPort->print((String)c);
			if (responseDest != NULL)
      {
				responseDest[destIndex++] = c;
			}
			charsRead++;
			if (c == expectedResponse[index])
      {
				if (++index == strlen(expectedResponse))
        {
					found = true;
				}
			}
      else
      {
				index = 0;
			}
			//This is a global array that holds the backlog of any events
			//that came in while waiting for response. To be processed later within bufferedPoll().
			saraResponseBacklog[backlogIndex++] = c;
		}
	}

	pruneBacklog();

	if (found)
  {
      return SARA_R5_ERROR_SUCCESS;
  }
  else if (charsRead == 0)
  {
      return SARA_R5_ERROR_NO_RESPONSE;
  }
  else
  {
      return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }
}

// Send a custom command with an expected (potentially partial) response, store entire response
SARA_R5_error_t SARA_R5::sendCustomCommandWithResponse(const char *command, const char *expectedResponse,
                                           char *responseDest, unsigned long commandTimeout, boolean at)
{
  return sendCommandWithResponse(command, expectedResponse, responseDest, commandTimeout, at);
}

int SARA_R5::sendCommand(const char *command, boolean at)
{
  int backlogIndex = strlen(saraResponseBacklog);

  unsigned long timeIn = micros();
	if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
  {
		while (micros()-timeIn < rxWindowUS && backlogIndex < RXBuffSize) //May need to escape on newline?
    {
			if (hwAvailable() > 0) //hwAvailable can return -1 if the serial port is NULL
      {
				char c = readChar();
				saraResponseBacklog[backlogIndex++] = c;
				timeIn = micros();
			}
		}
	}

  if (at)
    {
        hwPrint(SARA_R5_COMMAND_AT);
        hwPrint(command);
        hwPrint("\r");
    }
    else
    {
        hwPrint(command);
    }

    return backlogIndex;
}

SARA_R5_error_t SARA_R5::parseSocketReadIndication(int socket, int length)
{
    SARA_R5_error_t err;
    char *readDest;

    if ((socket < 0) || (length < 0))
    {
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }

    readDest = sara_r5_calloc_char(length + 1);
    if (readDest == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;

    err = socketRead(socket, length, readDest);
    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(readDest);
        return err;
    }

    if (_socketReadCallback != NULL)
    {
        _socketReadCallback(socket, String(readDest));
    }

    free(readDest);
    return SARA_R5_ERROR_SUCCESS;
}

SARA_R5_error_t SARA_R5::parseSocketReadIndicationUDP(int socket, int length)
{
	SARA_R5_error_t err;
	char* readDest;

	if ((socket < 0) || (length < 0))
  {
      return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
  }

	readDest = sara_r5_calloc_char(length + 1);
	if (readDest == NULL)
  {
		return SARA_R5_ERROR_OUT_OF_MEMORY;
	}

	err = socketReadUDP(socket, length, readDest);
	if (err != SARA_R5_ERROR_SUCCESS)
  {
    free(readDest);
		return err;
	}

	if (_socketReadCallback != NULL)
  {
		_socketReadCallback(socket, String(readDest));
	}

	free(readDest);
	return SARA_R5_ERROR_SUCCESS;
}

SARA_R5_error_t SARA_R5::parseSocketListenIndication(IPAddress localIP, IPAddress remoteIP)
{
    _lastLocalIP = localIP;
    _lastRemoteIP = remoteIP;
    return SARA_R5_ERROR_SUCCESS;
}

SARA_R5_error_t SARA_R5::parseSocketCloseIndication(String *closeIndication)
{
    SARA_R5_error_t err;
    int search;
    int socket;

    search = closeIndication->indexOf("UUSOCL: ") + strlen("UUSOCL: ");

    // Socket will be first integer, should be single-digit number between 0-6:
    socket = closeIndication->substring(search, search + 1).toInt();

    if (_socketCloseCallback != NULL)
    {
        _socketCloseCallback(socket);
    }

    return SARA_R5_ERROR_SUCCESS;
}

size_t SARA_R5::hwPrint(const char *s)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->print(s);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->print(s);
    }
#endif

    return (size_t)0;
}

size_t SARA_R5::hwWriteData(const char* buff, int len)
{
	if (_hardSerial != NULL)
  {
		return _hardSerial->write((const uint8_t *)buff, len);
	}
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
  else if (_softSerial != NULL)
  {
    return _softSerial->write((const uint8_t *)buff, len);
  }
#endif
    return (size_t)0;
}

size_t SARA_R5::hwWrite(const char c)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->write(c);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->write(c);
    }
#endif

    return (size_t)0;
}

int SARA_R5::readAvailable(char *inString)
{
    int len = 0;

    if (_hardSerial != NULL)
    {
        while (_hardSerial->available())
        {
            char c = (char)_hardSerial->read();
            if (inString != NULL)
            {
                inString[len++] = c;
            }
        }
        if (inString != NULL)
        {
            inString[len] = 0;
        }
        if (_printDebug == true) _debugPort->println(inString);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        while (_softSerial->available())
        {
            char c = (char)_softSerial->read();
            if (inString != NULL)
            {
                inString[len++] = c;
            }
        }
        if (inString != NULL)
        {
            inString[len] = 0;
        }
    }
#endif

    return len;
}

char SARA_R5::readChar(void)
{
    char ret;

    if (_hardSerial != NULL)
    {
        ret = (char)_hardSerial->read();
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        ret = (char)_softSerial->read();
    }
#endif

    return ret;
}

int SARA_R5::hwAvailable(void)
{
    if (_hardSerial != NULL)
    {
        return _hardSerial->available();
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        return _softSerial->available();
    }
#endif

    return -1;
}

void SARA_R5::beginSerial(unsigned long baud)
{
    if (_hardSerial != NULL)
    {
        _hardSerial->begin(baud);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        _softSerial->begin(baud);
    }
#endif
    delay(100);
}

void SARA_R5::setTimeout(unsigned long timeout)
{
    if (_hardSerial != NULL)
    {
        _hardSerial->setTimeout(timeout);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        _softSerial->setTimeout(timeout);
    }
#endif
}

bool SARA_R5::find(char *target)
{
    bool found = false;
    if (_hardSerial != NULL)
    {
        found = _hardSerial->find(target);
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    else if (_softSerial != NULL)
    {
        found = _softSerial->find(target);
    }
#endif
    return found;
}

SARA_R5_error_t SARA_R5::autobaud(unsigned long desiredBaud)
{
    SARA_R5_error_t err = SARA_R5_ERROR_INVALID;
    int b = 0;

    while ((err != SARA_R5_ERROR_SUCCESS) && (b < NUM_SUPPORTED_BAUD))
    {
        beginSerial(SARA_R5_SUPPORTED_BAUD[b++]);
        setBaud(desiredBaud);
        delay(200);
        beginSerial(desiredBaud);
        err = at();
    }
    if (err == SARA_R5_ERROR_SUCCESS)
    {
        beginSerial(desiredBaud);
    }
    return err;
}

char *SARA_R5::sara_r5_calloc_char(size_t num)
{
    return (char *)calloc(num, sizeof(char));
}

//This prunes the backlog of non-actionable events. If new actionable events are added, you must modify the if statement.
void SARA_R5::pruneBacklog()
{
	char* event;
	int pruneLen = 0;
	char pruneBuffer[RXBuffSize];
	memset(pruneBuffer, 0, RXBuffSize);

	event = strtok(saraResponseBacklog, "\r\n");
	while (event != NULL) //If event actionable, add to pruneBuffer.
  {
		if (strstr(event, "+UUSORD:") != NULL
			|| strstr(event, "+UUSOLI:") != NULL
			|| strstr(event, "+UUSOCL:") != NULL
			|| strstr(event, "+UUSORF:") != NULL)
      {
			   strcat(pruneBuffer, event);
			   strcat(pruneBuffer, "\r\n"); //strtok blows away delimiter, but we want that for later.
		  }
		event = strtok(NULL, "\r\n");
	}
	memset(saraResponseBacklog, 0, RXBuffSize);//Clear out backlog buffer.
	strcpy(saraResponseBacklog, pruneBuffer);

	if (strlen(saraResponseBacklog) > 0) //Handy for debugging new parsing.
  {
		if (_printDebug == true) _debugPort->println("PRUNING SAVED: ");
		if (_printDebug == true) _debugPort->println(saraResponseBacklog);
		if (_printDebug == true) _debugPort->println("fin.");
	}

	free(event);
}

// GPS Helper Functions:

// Read a source string until a delimiter is hit, store the result in destination
char *SARA_R5::readDataUntil(char *destination, unsigned int destSize,
                           char *source, char delimiter)
{

    char *strEnd;
    size_t len;

    strEnd = strchr(source, delimiter);

    if (strEnd != NULL)
    {
        len = strEnd - source;
        memset(destination, 0, destSize);
        memcpy(destination, source, len);
    }

    return strEnd;
}

boolean SARA_R5::parseGPRMCString(char *rmcString, PositionData *pos,
                                ClockData *clk, SpeedData *spd)
{
    char *ptr, *search;
    unsigned long tTemp;
    char tempData[TEMP_NMEA_DATA_SIZE];

    // if (_printDebug == true)
    // {
    //   _debugPort->println(F("parseGPRMCString: rmcString: "));
    //   _debugPort->println(rmcString);
    // }

    // Fast-forward test to first value:
    ptr = strchr(rmcString, ',');
    ptr++; // Move ptr past first comma

    // If the next character is another comma, there's no time data
    // Find time:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    // Next comma should be present and not the next position
    if ((search != NULL) && (search != ptr))
    {
        pos->utc = atof(tempData); // Extract hhmmss.ss as float
        tTemp = pos->utc; // Convert to unsigned long (discard the digits beyond the decimal point)
        clk->time.ms = ((unsigned int)(pos->utc * 100)) % 100; // Extract the milliseconds
        clk->time.hour = tTemp / 10000;
        tTemp -= ((unsigned long)clk->time.hour * 10000);
        clk->time.minute = tTemp / 100;
        tTemp -= ((unsigned long)clk->time.minute * 100);
        clk->time.second = tTemp;
    }
    else
    {
        pos->utc = 0.0;
        clk->time.hour = 0;
        clk->time.minute = 0;
        clk->time.second = 0;
    }
    ptr = search + 1; // Move pointer to next value

    // Find status character:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    // Should be a single character: V = Data invalid, A = Data valid
    if ((search != NULL) && (search == ptr + 1))
    {
        pos->status = *ptr; // Assign char at ptr to status
    }
    else
    {
        pos->status = 'X'; // Made up very bad status
    }
    ptr = search + 1;

    // Find latitude:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        pos->lat = atof(tempData); // Extract ddmm.mmmmm as float
        unsigned long lat_deg = pos->lat / 100; // Extract the degrees
        pos->lat -= (float)lat_deg * 100.0; // Subtract the degrees leaving only the minutes
        pos->lat /= 60.0; // Convert minutes into degrees
        pos->lat += (float)lat_deg; // Finally add the degrees back on again
    }
    else
    {
        pos->lat = 0.0;
    }
    ptr = search + 1;

    // Find latitude hemishpere
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search == ptr + 1))
    {
        if (*ptr == 'S') // Is the latitude South
        pos->lat *= -1.0; // Make lat negative
    }
    ptr = search + 1;

    // Find longitude:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
      pos->lon = atof(tempData); // Extract dddmm.mmmmm as float
      unsigned long lon_deg = pos->lon / 100; // Extract the degrees
      pos->lon -= (float)lon_deg * 100.0; // Subtract the degrees leaving only the minutes
      pos->lon /= 60.0; // Convert minutes into degrees
      pos->lon += (float)lon_deg; // Finally add the degrees back on again
    }
    else
    {
        pos->lon = 0.0;
    }
    ptr = search + 1;

    // Find longitude hemishpere
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search == ptr + 1))
    {
        if (*ptr == 'W') // Is the longitude West
        pos->lon *= -1.0; // Make lon negative
    }
    ptr = search + 1;

    // Find speed
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        spd->speed = atof(tempData); // Extract speed over ground in knots
        spd->speed *= 0.514444; // Convert to m/s
    }
    else
    {
        spd->speed = 0.0;
    }
    ptr = search + 1;

    // Find course over ground
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        spd->cog = atof(tempData);
    }
    else
    {
        spd->cog = 0.0;
    }
    ptr = search + 1;

    // Find date
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        tTemp = atol(tempData);
        clk->date.day = tTemp / 10000;
        tTemp -= ((unsigned long)clk->date.day * 10000);
        clk->date.month = tTemp / 100;
        tTemp -= ((unsigned long)clk->date.month * 100);
        clk->date.year = tTemp;
    }
    else
    {
        clk->date.day = 0;
        clk->date.month = 0;
        clk->date.year = 0;
    }
    ptr = search + 1;

    // Find magnetic variation in degrees:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        spd->magVar = atof(tempData);
    }
    else
    {
        spd->magVar = 0.0;
    }
    ptr = search + 1;

    // Find magnetic variation direction
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search == ptr + 1))
    {
        if (*ptr == 'W') // Is the magnetic variation West
        spd->magVar *= -1.0; // Make magnetic variation negative
    }
    ptr = search + 1;

    // Find position system mode
    // Possible values for posMode: N = No fix, E = Estimated/Dead reckoning fix, A = Autonomous GNSS fix,
    //                              D = Differential GNSS fix, F = RTK float, R = RTK fixed
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, '*');
    if ((search != NULL) && (search = ptr + 1))
    {
        pos->mode = *ptr;
    }
    else
    {
        pos->mode = 'X';
    }
    ptr = search + 1;

    if (pos->status == 'A')
    {
        return true;
    }
    return false;
}
