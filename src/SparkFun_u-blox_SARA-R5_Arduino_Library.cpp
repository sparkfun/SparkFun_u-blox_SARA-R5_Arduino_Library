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

#include <SparkFun_SARA_R5_Arduino_Library.h>

#define SARA_R5_STANDARD_RESPONSE_TIMEOUT 1000
#define SARA_R5_SET_BAUD_TIMEOUT 500
#define SARA_R5_POWER_PULSE_PERIOD 3200
#define SARA_R5_RESET_PULSE_PERIOD 10000
#define SARA_R5_IP_CONNECT_TIMEOUT 60000
#define SARA_R5_POLL_DELAY 1
#define SARA_R5_SOCKET_WRITE_TIMEOUT 10000

// ## Suported AT Commands
// ### General
const char SARA_R5_COMMAND_AT[] = "AT";      // AT "Test"
const char SARA_R5_COMMAND_ECHO[] = "E";     // Local Echo
const char SARA_R5_COMMAND_MANU_ID[] = "+CGMI"; // Manufacturer identification
const char SARA_R5_COMMAND_MODEL_ID[] = "+CGMM"; // Model identification
const char SARA_R5_COMMAND_FW_VER_ID[] = "+CGMR"; // Firmware version identification
const char SARA_R5_COMMAND_IMEI[] = "+CGSN"; // IMEI identification
const char SARA_R5_COMMAND_IMSI[] = "+CIMI"; // IMSI identification
const char SARA_R5_COMMAND_CCID[] = "+CCID"; // SIM CCID
const char SARA_R5_COMMAND_REQ_CAP[] = "+GCAP"; // Request capabilities list
// ### Control and status
const char SARA_R5_COMMAND_POWER_OFF[] = "+CPWROFF"; // Module switch off
const char SARA_R5_COMMAND_FUNC[] = "+CFUN";    // Functionality (reset, etc.)
const char SARA_R5_COMMAND_CLOCK[] = "+CCLK";   // Real-time clock
const char SARA_R5_COMMAND_AUTO_TZ[] = "+CTZU"; // Automatic time zone update
const char SARA_R5_COMMAND_TZ_REPORT[] = "+CTZR"; // Time zone reporting
// ### Network service
const char SARA_R5_COMMAND_CNUM[] = "+CNUM"; // Subscriber number
const char SARA_R5_SIGNAL_QUALITY[] = "+CSQ";
const char SARA_R5_OPERATOR_SELECTION[] = "+COPS";
const char SARA_R5_REGISTRATION_STATUS[] = "+CREG";
const char SARA_R5_READ_OPERATOR_NAMES[] = "+COPN";
const char SARA_R5_COMMAND_MNO[] = "+UMNOPROF"; // MNO (mobile network operator) Profile
// ### SMS
const char SARA_R5_MESSAGE_FORMAT[] = "+CMGF"; // Set SMS message format
const char SARA_R5_SEND_TEXT[] = "+CMGS";      // Send SMS message
// V24 control and V25ter (UART interface)
const char SARA_R5_COMMAND_BAUD[] = "+IPR"; // Baud rate
// ### Packet switched data services
const char SARA_R5_MESSAGE_PDP_DEF[] = "+CGDCONT";
const char SARA_R5_MESSAGE_ENTER_PPP[] = "D";
// ### GPIO
const char SARA_R5_COMMAND_GPIO[] = "+UGPIOC"; // GPIO Configuration
// ### IP
const char SARA_R5_CREATE_SOCKET[] = "+USOCR";  // Create a new socket
const char SARA_R5_CLOSE_SOCKET[] = "+USOCL";   // Close a socket
const char SARA_R5_CONNECT_SOCKET[] = "+USOCO"; // Connect to server on socket
const char SARA_R5_WRITE_SOCKET[] = "+USOWR";   // Write data to a socket
const char SARA_R5_READ_SOCKET[] = "+USORD";    // Read from a socket
const char SARA_R5_LISTEN_SOCKET[] = "+USOLI";  // Listen for connection on socket
// ### GPS
const char SARA_R5_GPS_POWER[] = "+UGPS";
const char SARA_R5_GPS_GPRMC[] = "+UGRMC";
const char SARA_R5_GPS_REQUEST_LOCATION[] = "+ULOC";

const char SARA_R5_RESPONSE_OK[] = "OK\r\n";

// CTRL+Z and ESC ASCII codes for SMS message sends
const char ASCII_CTRL_Z = 0x1A;
const char ASCII_ESC = 0x1B;

#define NOT_AT_COMMAND false
#define AT_COMMAND true

#define SARA_R5_NUM_SOCKETS 6

#define NUM_SUPPORTED_BAUD 6
const unsigned long SARA_R5_SUPPORTED_BAUD[NUM_SUPPORTED_BAUD] =
    {
        115200,
        9600,
        19200,
        38400,
        57600,
        230400};
#define SARA_R5_DEFAULT_BAUD_RATE 115200

char lteShieldRXBuffer[128];

static boolean parseGPRMCString(char *rmcString, PositionData *pos, ClockData *clk, SpeedData *spd);

SARA_R5::SARA_R5(uint8_t powerPin, uint8_t resetPin)
{
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    _softSerial = NULL;
#endif
    _hardSerial = NULL;
    _baud = 0;
    _resetPin = resetPin;
    _powerPin = powerPin;
    _socketReadCallback = NULL;
    _socketCloseCallback = NULL;
    _lastRemoteIP = {0, 0, 0, 0};
    _lastLocalIP = {0, 0, 0, 0};

    memset(lteShieldRXBuffer, 0, 128);
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

boolean SARA_R5::poll(void)
{
    int avail = 0;
    char c = 0;
    bool handled = false;

    memset(lteShieldRXBuffer, 0, 128);

    if (hwAvailable())
    {
        while (c != '\n')
        {
            if (hwAvailable())
            {
                c = readChar();
                lteShieldRXBuffer[avail++] = c;
            }
        }
        {
            int socket, length;
            if (sscanf(lteShieldRXBuffer, "+UUSORD: %d,%d", &socket, &length) == 2)
            {
                parseSocketReadIndication(socket, length);
                handled = true;
            }
        }
        {
            int socket, listenSocket;
            unsigned int port, listenPort;
            IPAddress remoteIP, localIP;

            if (sscanf(lteShieldRXBuffer,
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

            if (sscanf(lteShieldRXBuffer,
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
            unsigned int latH, lonH, altU, speedU, trackU;
            char latL[10], lonL[10];

            if (strstr(lteShieldRXBuffer, "+UULOC"))
            {
                // Found a Location string!
                scanNum = sscanf(lteShieldRXBuffer,
                                 "+UULOC: %hhu/%hhu/%u,%hhu:%hhu:%hhu.%u,%u.%[^,],%u.%[^,],%u,%lu,%u,%u,*%s",
                                 &clck.date.day, &clck.date.month, &clck.date.year,
                                 &clck.time.hour, &clck.time.minute, &clck.time.second, &clck.time.ms,
                                 &latH, latL, &lonH, lonL, &altU, &uncertainty,
                                 &speedU, &trackU);
                if (scanNum < 13)
                    return false; // Break out if we didn't find enough

                gps.lat = (float)latH + ((float)atol(latL) / pow(10, strlen(latL)));
                gps.lon = (float)lonH + ((float)atol(lonL) / pow(10, strlen(lonL)));
                gps.alt = (float)altU;
                if (scanNum == 15) // If detailed response, get speed data
                {
                    spd.speed = (float)speedU;
                    spd.track = (float)trackU;
                }

                if (_gpsRequestCallback != NULL)
                {
                    _gpsRequestCallback(clck, gps, spd, uncertainty);
                }
            }
        }

        if ((handled == false) && (strlen(lteShieldRXBuffer) > 2))
        {
            //Serial.println("Poll: " + String(lteShieldRXBuffer));
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

String SARA_R5::imei(void)
{
    char *response;
    char imeiResponse[16];
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
    ;
}

String SARA_R5::imsi(void)
{
    char *response;
    char imsiResponse[16];
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

String SARA_R5::ccid(void)
{
    char *response;
    char ccidResponse[21];
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

SARA_R5_error_t SARA_R5::reset(void)
{
    SARA_R5_error_t err;

    err = functionality(SILENT_RESET);
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
        return "";

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
    }

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
        return -1;

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
        return SARA_R5_REGISTRATION_INVALID;

    if (sscanf(response, "\r\n+CREG: %*d,%d", &status) != 1)
    {
        status = SARA_R5_REGISTRATION_INVALID;
    }
    free(command);
    free(response);
    return (SARA_R5_registration_status_t)status;
}

boolean SARA_R5::setNetwork(mobile_network_operator_t mno)
{
    mobile_network_operator_t currentMno;

    // Check currently set MNO
    if (getMno(&currentMno) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }
    if (currentMno == mno)
    {
        return true;
    }

    if (functionality(MINIMUM_FUNCTIONALITY) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    if (setMno(mno) != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    if (reset() != SARA_R5_ERROR_SUCCESS)
    {
        return false;
    }

    return true;
}

mobile_network_operator_t SARA_R5::getNetwork(void)
{
    mobile_network_operator_t mno;
    SARA_R5_error_t err;

    err = getMno(&mno);
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
    }
    sprintf(command, "%s=%d,\"%s\",\"%s\"", SARA_R5_MESSAGE_PDP_DEF,
            cid, pdpStr, apn.c_str());

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::getAPN(String *apn, IPAddress *ip)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    char *searchPtr;
    int ipOctets[4];

    command = sara_r5_calloc_char(strlen(SARA_R5_MESSAGE_PDP_DEF) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_MESSAGE_PDP_DEF);

    response = sara_r5_calloc_char(128);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Example: +CGDCONT: 1,"IP","hologram","10.170.241.191",0,0,0,0
        searchPtr = strstr(response, "+CGDCONT: ");
        if (searchPtr != NULL)
        {
            searchPtr += strlen("+CGDCONT: ");
            // Search to the third double-quote
            for (int i = 0; i < 3; i++)
            {
                searchPtr = strchr(++searchPtr, '\"');
            }
            if (searchPtr != NULL)
            {
                // Fill in the APN:
                searchPtr = strchr(searchPtr, '\"'); // Move to first quote
                while ((*(++searchPtr) != '\"') && (*searchPtr != '\0'))
                {
                    apn->concat(*(searchPtr));
                }
                // Now get the IP:
                if (searchPtr != NULL)
                {
                    int scanned = sscanf(searchPtr, "\",\"%d.%d.%d.%d\"",
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
        else
        {
            err = SARA_R5_ERROR_UNEXPECTED_RESPONSE;
        }
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

    response = sara_r5_calloc_char(maxOps * 48 + 16);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    // AT+COPS maximum response time is 3 minutes (180000 ms)
    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  180000);

    // Sample responses:
    // +COPS: (3,"Verizon Wireless","VzW","311480",8),,(0,1,2,3,4),(0,1,2)
    // +COPS: (1,"313 100","313 100","313100",8),(2,"AT&T","AT&T","310410",8),(3,"311 480","311 480","311480",8),,(0,1,2,3,4),(0,1,2)

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
                                  180000);

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
                                  180000);

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
                //Serial.println("Operator: " + *oper);
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
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

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
                                      SARA_R5_STANDARD_RESPONSE_TIMEOUT);
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
                                      NULL, 180000, NOT_AT_COMMAND);
    }
    else
    {
        err = SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    free(messageCStr);

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

SARA_R5_error_t SARA_R5::setGpioMode(SARA_R5_gpio_t gpio,
                                           SARA_R5_gpio_mode_t mode)
{
    SARA_R5_error_t err;
    char *command;

    // Example command: AT+UGPIOC=16,2
    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_GPIO) + 7);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_COMMAND_GPIO, gpio, mode);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

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
    if (gpioStart == NULL)
        return GPIO_MODE_INVALID; // If not found return invalid
    sscanf(gpioStart, "%*d,%d\r\n", &gpioMode);

    free(command);
    free(response);

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

    response = sara_r5_calloc_char(24);
    if (response == NULL)
    {
        free(command);
        return -1;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err != SARA_R5_ERROR_SUCCESS)
    {
        free(command);
        free(response);
        return -1;
    }

    responseStart = strstr(response, "+USOCR");
    if (responseStart == NULL)
    {
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

    command = sara_r5_calloc_char(strlen(SARA_R5_CLOSE_SOCKET) + 10);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_CLOSE_SOCKET, socket);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, timeout);

    free(command);

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
    SARA_R5_error_t err;

    command = sara_r5_calloc_char(strlen(SARA_R5_WRITE_SOCKET) + 8);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d,%d", SARA_R5_WRITE_SOCKET, socket, strlen(str));

    err = sendCommandWithResponse(command, "@", NULL,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    hwPrint(str);

    err = waitForResponse(SARA_R5_RESPONSE_OK, SARA_R5_SOCKET_WRITE_TIMEOUT);

    free(command);
    return err;
}

SARA_R5_error_t SARA_R5::socketWrite(int socket, String str)
{
    return socketWrite(socket, str.c_str());
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

    response = sara_r5_calloc_char(length + strlen(SARA_R5_READ_SOCKET) + 24);
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

IPAddress SARA_R5::lastRemoteIP(void)
{
    return _lastRemoteIP;
}

boolean SARA_R5::gpsOn(void)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    boolean on = false;

    command = sara_r5_calloc_char(strlen(SARA_R5_GPS_POWER) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_GPS_POWER);

    response = sara_r5_calloc_char(24);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response,
                                  SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    if (err == SARA_R5_ERROR_SUCCESS)
    {
        // Example response: "+UGPS: 0" for off "+UGPS: 1,0,1" for on
        // May be too lazy/simple, but just search for a '1'
        if (strchr(response, '1') != NULL)
            on = true;
    }

    free(command);
    free(response);

    return on;
}

SARA_R5_error_t SARA_R5::gpsPower(boolean enable, gnss_system_t gnss_sys)
{
    SARA_R5_error_t err;
    char *command;
    boolean gpsState;

    // Don't turn GPS on/off if it's already on/off
    gpsState = gpsOn();
    if ((enable && gpsState) || (!enable && !gpsState))
    {
        return SARA_R5_ERROR_SUCCESS;
    }

    command = sara_r5_calloc_char(strlen(SARA_R5_GPS_POWER) + 8);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    if (enable)
    {
        sprintf(command, "%s=1,0,%d", SARA_R5_GPS_POWER, gnss_sys);
    }
    else
    {
        sprintf(command, "%s=0", SARA_R5_GPS_POWER);
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

    if (!gpsOn())
    {
        err = gpsPower(true);
        if (err != SARA_R5_ERROR_SUCCESS)
        {
            return err;
        }
    }

    command = sara_r5_calloc_char(strlen(SARA_R5_GPS_GPRMC) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_GPS_GPRMC, enable ? 1 : 0);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, 10000);

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

    command = sara_r5_calloc_char(strlen(SARA_R5_GPS_GPRMC) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_GPS_GPRMC);

    response = sara_r5_calloc_char(96);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, response, 10000);
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
    if (gpsOn())
    {
        gpsPower(false);
    }

    if (timeout > 999)
        timeout = 999;
    if (accuracy > 999999)
        accuracy = 999999;

    command = sara_r5_calloc_char(strlen(SARA_R5_GPS_REQUEST_LOCATION) + 24);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=2,3,%d,%d,%d", SARA_R5_GPS_REQUEST_LOCATION,
            detailed ? 1 : 0, timeout, accuracy);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK, NULL, 10000);

    free(command);
    return err;
}

/////////////
// Private //
/////////////

SARA_R5_error_t SARA_R5::init(unsigned long baud,
                                    SARA_R5::SARA_R5_init_type_t initType)
{
    SARA_R5_error_t err;

    beginSerial(baud); // Begin serial

    if (initType == SARA_R5_INIT_AUTOBAUD)
    {
        if (autobaud(baud) != SARA_R5_ERROR_SUCCESS)
        {
            return init(baud, SARA_R5_INIT_RESET);
        }
    }
    else if (initType == SARA_R5_INIT_RESET)
    {
        powerOn();
        if (at() != SARA_R5_ERROR_SUCCESS)
        {
            return init(baud, SARA_R5_INIT_AUTOBAUD);
        }
    }

    // Use disable echo to test response
    err = enableEcho(false);

    if (err != SARA_R5_ERROR_SUCCESS)
        return init(baud, SARA_R5_INIT_AUTOBAUD);

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

void SARA_R5::powerOn(void)
{
  if (_powerPin >= 0)
  {
    pinMode(_powerPin, OUTPUT);
    digitalWrite(_powerPin, LOW);
    delay(SARA_R5_POWER_PULSE_PERIOD);
    pinMode(_powerPin, INPUT); // Return to high-impedance, rely on SARA module internal pull-up
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
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);

    return err;
}

SARA_R5_error_t SARA_R5::setMno(mobile_network_operator_t mno)
{
    SARA_R5_error_t err;
    char *command;
    char *response;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_MNO) + 3);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s=%d", SARA_R5_COMMAND_MNO, (uint8_t)mno);

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  NULL, SARA_R5_STANDARD_RESPONSE_TIMEOUT);

    free(command);
    free(response);

    return err;
}

SARA_R5_error_t SARA_R5::getMno(mobile_network_operator_t *mno)
{
    SARA_R5_error_t err;
    char *command;
    char *response;
    const char *mno_keys = "0123456"; // Valid MNO responses
    int i;

    command = sara_r5_calloc_char(strlen(SARA_R5_COMMAND_MNO) + 2);
    if (command == NULL)
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    sprintf(command, "%s?", SARA_R5_COMMAND_MNO);

    response = sara_r5_calloc_char(24);
    if (response == NULL)
    {
        free(command);
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }

    err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                  response, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
    if (err != SARA_R5_ERROR_SUCCESS)
        return err;

    i = strcspn(response, mno_keys); // Find first occurence of MNO key
    if (i == strlen(response))
    {
        *mno = MNO_INVALID;
        return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
    }
    *mno = (mobile_network_operator_t)(response[i] - 0x30); // Convert to integer

    free(command);
    free(response);

    return err;
}

/*SARA_R5_error_t SARA_R5::sendCommandWithResponseAndTimeout(const char * command,
    char * expectedResponse, uint16_t commandTimeout, boolean at)
{
    unsigned long timeIn = millis();
    char * response;

    sendCommand(command, at);

    // Wait until we've receved the requested number of characters
    while (hwAvailable() < strlen(expectedResponse))
    {
        if (millis() > timeIn + commandTimeout)
        {
            return SARA_R5_ERROR_TIMEOUT;
        }
    }
    response = sara_r5_calloc_char(hwAvailable() + 1);
    if (response == NULL)
    {
        return SARA_R5_ERROR_OUT_OF_MEMORY;
    }
    readAvailable(response);

    // Check for expected response
    if (strcmp(response, expectedResponse) == 0)
    {
        return SARA_R5_ERROR_SUCCESS;
    }
    return SARA_R5_ERROR_UNEXPECTED_RESPONSE;
}*/

SARA_R5_error_t SARA_R5::waitForResponse(const char *expectedResponse, uint16_t timeout)
{
    unsigned long timeIn;
    boolean found = false;
    int index = 0;

    timeIn = millis();

    while ((!found) && (timeIn + timeout > millis()))
    {
        if (hwAvailable())
        {
            char c = readChar();
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
        }
    }
    return found ? SARA_R5_ERROR_SUCCESS : SARA_R5_ERROR_UNEXPECTED_RESPONSE;
}

SARA_R5_error_t SARA_R5::sendCommandWithResponse(
    const char *command, const char *expectedResponse, char *responseDest,
    unsigned long commandTimeout, boolean at)
{
    unsigned long timeIn;
    boolean found = false;
    int index = 0;
    int destIndex = 0;
    unsigned int charsRead = 0;

    //Serial.print("Command: ");
    //Serial.println(String(command));
    sendCommand(command, at);
    //Serial.print("Response: ");
    timeIn = millis();
    while ((!found) && (timeIn + commandTimeout > millis()))
    {
        if (hwAvailable())
        {
            char c = readChar();
            //Serial.write(c);
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
        }
    }
    //Serial.println();

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

boolean SARA_R5::sendCommand(const char *command, boolean at)
{
    readAvailable(NULL); // Clear out receive buffer before sending a new command

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

    return true;
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
        return err;

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
    }
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    if (_softSerial != NULL)
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

// GPS Helper Functions:

// Read a source string until a delimiter is hit, store the result in destination
static char *readDataUntil(char *destination, unsigned int destSize,
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

#define TEMP_NMEA_DATA_SIZE 16

static boolean parseGPRMCString(char *rmcString, PositionData *pos,
                                ClockData *clk, SpeedData *spd)
{
    char *ptr, *search;
    unsigned long tTemp;
    char tempData[TEMP_NMEA_DATA_SIZE];

    // Fast-forward test to first value:
    ptr = strchr(rmcString, ',');
    ptr++; // Move ptr past first comma

    // If the next character is another comma, there's no time data
    // Find time:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    // Next comma should be present and not the next position
    if ((search != NULL) && (search != ptr))
    {
        pos->utc = atof(tempData);
        tTemp = pos->utc;
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
    // Should be a single character
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
        pos->lat = atof(tempData);
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
        pos->latDir = *ptr; // Assign char at ptr to status
    }
    else
    {
        pos->latDir = 'X';
    }
    ptr = search + 1;

    // Find longitude:
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        pos->lon = atof(tempData);
    }
    else
    {
        pos->lon = 0.0;
    }
    ptr = search + 1;
    // Find latitude hemishpere
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search == ptr + 1))
    {
        pos->lonDir = *ptr; // Assign char at ptr to status
    }
    else
    {
        pos->lonDir = 'X';
    }
    ptr = search + 1;

    // Find speed
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        spd->speed = atof(tempData);
    }
    else
    {
        spd->speed = 0.0;
    }
    ptr = search + 1;
    // Find track
    search = readDataUntil(tempData, TEMP_NMEA_DATA_SIZE, ptr, ',');
    if ((search != NULL) && (search != ptr))
    {
        spd->track = atof(tempData);
    }
    else
    {
        spd->track = 0.0;
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
        spd->magVarDir = *ptr; // Assign char at ptr to status
    }
    else
    {
        spd->magVarDir = 'X';
    }
    ptr = search + 1;

    // Find position system mode
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
