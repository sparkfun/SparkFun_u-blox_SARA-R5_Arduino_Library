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

#ifndef SPARKFUN_SARA_R5_ARDUINO_LIBRARY_H
#define SPARKFUN_SARA_R5_ARDUINO_LIBRARY_H

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#ifdef ARDUINO_ARCH_AVR                    // Arduino AVR boards (Uno, Pro Micro, etc.)
#define SARA_R5_SOFTWARE_SERIAL_ENABLED // Enable software serial
#endif

#ifdef ARDUINO_ARCH_SAMD                    // Arduino SAMD boards (SAMD21, etc.)
#define SARA_R5_SOFTWARE_SERIAL_ENABLEDx // Disable software serial
#endif

#ifdef ARDUINO_ARCH_APOLLO3                // Arduino Apollo boards (Artemis module, RedBoard Artemis, etc)
#define SARA_R5_SOFTWARE_SERIAL_ENABLED // Enable software serial
#endif

#ifdef ARDUINO_ARCH_STM32                  // STM32 based boards (Disco, Nucleo, etc)
#define SARA_R5_SOFTWARE_SERIAL_ENABLED // Enable software serial
#endif

#ifdef ARDUINO_ARCH_ESP32                  // ESP32 based boards
#define SARA_R5_SOFTWARE_SERIAL_ENABLEDx // Disable software serial
#endif

#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
#include <SoftwareSerial.h>
#endif

#include <IPAddress.h>

#define SARA_R5_POWER_PIN -1 // Default to no pin
#define SARA_R5_RESET_PIN -1

// Timing
#define SARA_R5_STANDARD_RESPONSE_TIMEOUT 1000
#define SARA_R5_10_SEC_TIMEOUT 10000
#define SARA_R5_2_MIN_TIMEOUT 120000
#define SARA_R5_3_MIN_TIMEOUT 180000
#define SARA_R5_SET_BAUD_TIMEOUT 500
#define SARA_R5_POWER_PULSE_PERIOD 3200
#define SARA_R5_RESET_PULSE_PERIOD 10000
#define SARA_R5_IP_CONNECT_TIMEOUT 130000
#define SARA_R5_POLL_DELAY 1
#define SARA_R5_SOCKET_WRITE_TIMEOUT 10000

// ## Suported AT Commands
// ### General
const char SARA_R5_COMMAND_AT[] = "AT";      // AT "Test"
const char SARA_R5_COMMAND_ECHO[] = "E";     // Local Echo
const char SARA_R5_COMMAND_MANU_ID[] = "+CGMI"; // Manufacturer identification
const char SARA_R5_COMMAND_MODEL_ID[] = "+CGMM"; // Model identification
const char SARA_R5_COMMAND_FW_VER_ID[] = "+CGMR"; // Firmware version identification
const char SARA_R5_COMMAND_SERIAL_NO[] = "+CGSN"; // Product serial number
const char SARA_R5_COMMAND_IMEI[] = "+CSN"; // IMEI identification
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
// ### SIM
const char SARA_R5_SIM_STATE[] = "+USIMSTAT";
// ### SMS
const char SARA_R5_MESSAGE_FORMAT[] = "+CMGF"; // Set SMS message format
const char SARA_R5_SEND_TEXT[] = "+CMGS";      // Send SMS message
const char SARA_R5_NEW_MESSAGE_IND[] = "+CNMI"; // New [SMS] message indication
const char SARA_R5_PREF_MESSAGE_STORE[] = "+CPMS"; // Preferred message storage
const char SARA_R5_READ_TEXT_MESSAGE[] = "+CMGR"; // Read message
// V24 control and V25ter (UART interface)
const char SARA_R5_FLOW_CONTROL[] = "&K"; // Flow control
const char SARA_R5_COMMAND_BAUD[] = "+IPR"; // Baud rate
// ### Packet switched data services
const char SARA_R5_MESSAGE_PDP_DEF[] = "+CGDCONT"; // Packet switched Data Profile context definition
const char SARA_R5_MESSAGE_PDP_CONFIG[] = "+UPSD"; // Packet switched Data Profile configuration
const char SARA_R5_MESSAGE_PDP_ACTION[] = "+UPSDA"; // Perform the action for the specified PSD profile
const char SARA_R5_MESSAGE_PDP_CONTEXT_ACTIVATE[] = "+CGACT"; // Activates or deactivates the specified PDP context
const char SARA_R5_MESSAGE_ENTER_PPP[] = "D";
// ### GPIO
const char SARA_R5_COMMAND_GPIO[] = "+UGPIOC"; // GPIO Configuration
// ### IP
const char SARA_R5_CREATE_SOCKET[] = "+USOCR";  // Create a new socket
const char SARA_R5_CLOSE_SOCKET[] = "+USOCL";   // Close a socket
const char SARA_R5_CONNECT_SOCKET[] = "+USOCO"; // Connect to server on socket
const char SARA_R5_WRITE_SOCKET[] = "+USOWR";   // Write data to a socket
const char SARA_R5_WRITE_UDP_SOCKET[] = "+USOST"; // Write data to a UDP socket
const char SARA_R5_READ_SOCKET[] = "+USORD";    // Read from a socket
const char SARA_R5_READ_UDP_SOCKET[] = "+USORF"; // Read UDP data from a socket
const char SARA_R5_LISTEN_SOCKET[] = "+USOLI";  // Listen for connection on socket
const char SARA_R5_GET_ERROR[] = "+USOER"; // Get last socket error.
// ### Ping
const char SARA_R5_PING_COMMAND[] = "+UPING"; // Ping
// ### HTTP
const char SARA_R5_HTTP_PROFILE[] = "+UHTTP"; // Configure the HTTP profile. Up to 4 different profiles can be defined
const char SARA_R5_HTTP_COMMAND[] = "+UHTTPC"; // Trigger the specified HTTP command
const char SARA_R5_HTTP_PROTOCOL_ERROR[] = "+UHTTPER"; // Retrieves the error class and code of the latest HTTP operation on the specified HTTP profile.
// ### GNSS
const char SARA_R5_GNSS_POWER[] = "+UGPS"; // GNSS power management configuration
const char SARA_R5_GNSS_ASSISTED_IND[] = "+UGIND"; // Assisted GNSS unsolicited indication
const char SARA_R5_GNSS_REQUEST_LOCATION[] = "+ULOC"; // Ask for localization information
const char SARA_R5_GNSS_GPRMC[] = "+UGRMC"; // Ask for localization information
const char SARA_R5_GNSS_REQUEST_TIME[] = "+UTIME"; // Ask for time information from cellular modem (CellTime)
const char SARA_R5_GNSS_TIME_INDICATION[] = "+UTIMEIND"; // Time information request status unsolicited indication
const char SARA_R5_GNSS_TIME_CONFIGURATION[] = "+UTIMECFG"; // Sets time configuration
const char SARA_R5_GNSS_CONFIGURE_SENSOR[] = "+ULOCGNSS"; // Configure GNSS sensor
const char SARA_R5_GNSS_CONFIGURE_LOCATION[] = "+ULOCCELL"; // Configure cellular location sensor (CellLocate®)
// ### File System
const char SARA_R5_FILE_SYSTEM_READ_FILE[] = "+URDFILE"; // Read a file
// ### Response
const char SARA_R5_RESPONSE_OK[] = "OK\r\n";
const char SARA_R5_RESPONSE_ERROR[] = "ERROR\r\n";

// CTRL+Z and ESC ASCII codes for SMS message sends
const char ASCII_CTRL_Z = 0x1A;
const char ASCII_ESC = 0x1B;

// NMEA data size - used by parseGPRMCString
#define TEMP_NMEA_DATA_SIZE 16

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

// Flow control definitions for AT&K
// Note: SW (XON/XOFF) flow control is not supported on the SARA_R5
typedef enum
{
  SARA_R5_DISABLE_FLOW_CONTROL = 0,
  SARA_R5_ENABLE_FLOW_CONTROL = 3
} SARA_R5_flow_control_t;

// The standard Europe profile should be used as the basis for all other MNOs in Europe outside of Vodafone
// and Deutsche Telekom. However, there may be changes that need to be applied to the module for proper
// operation with any given European MNO such as attach type, RAT preference, band selection, etc. Please
// consult with the preferred network provider.
typedef enum
{
    MNO_INVALID = -1,
    MNO_SW_DEFAULT = 0, // Undefined / regulatory
    MNO_SIM_ICCID = 1,
    MNO_ATT = 2,        // AT&T
    MNO_VERIZON = 3,
    MNO_TELSTRA = 4,
    MNO_TMO = 5,        // T-Mobile US
    MNO_CT = 6,         // China Telecom
    MNO_SPRINT = 8,
    MNO_VODAFONE = 19,
    MNO_NTT_DOCOMO = 20,
    MNO_TELUS = 21,
    MNO_SOFTBANK = 28,
    MNO_DT = 31,        // Deutsche Telekom
    MNO_US_CELLULAR = 32,
    MNO_SKT = 39,
    MNO_GLOBAL = 90,
    MNO_STD_EUROPE = 100,
    MNO_STD_EU_NOEPCO = 101
} mobile_network_operator_t;

typedef enum
{
    SARA_R5_ERROR_INVALID = -1,        // -1
    SARA_R5_ERROR_SUCCESS = 0,         // 0
    SARA_R5_ERROR_OUT_OF_MEMORY,       // 1
    SARA_R5_ERROR_TIMEOUT,             // 2
    SARA_R5_ERROR_UNEXPECTED_PARAM,    // 3
    SARA_R5_ERROR_UNEXPECTED_RESPONSE, // 4
    SARA_R5_ERROR_NO_RESPONSE,         // 5
    SARA_R5_ERROR_DEREGISTERED,        // 6
	  SARA_R5_ERROR_ERROR				         // 7
} SARA_R5_error_t;
#define SARA_R5_SUCCESS SARA_R5_ERROR_SUCCESS

typedef enum
{
    SARA_R5_REGISTRATION_INVALID = -1,
    SARA_R5_REGISTRATION_NOT_REGISTERED = 0,
    SARA_R5_REGISTRATION_HOME = 1,
    SARA_R5_REGISTRATION_SEARCHING = 2,
    SARA_R5_REGISTRATION_DENIED = 3,
    SARA_R5_REGISTRATION_UNKNOWN = 4,
    SARA_R5_REGISTRATION_ROAMING = 5,
    SARA_R5_REGISTRATION_HOME_SMS_ONLY = 6,
    SARA_R5_REGISTRATION_ROAMING_SMS_ONLY = 7,
    SARA_R5_REGISTRATION_EMERGENCY_SERV_ONLY = 8,
    SARA_R5_REGISTRATION_HOME_CSFB_NOT_PREFERRED = 9,
    SARA_R5_REGISTRATION_ROAMING_CSFB_NOT_PREFERRED = 10
} SARA_R5_registration_status_t;

struct DateData
{
    uint8_t day;
    uint8_t month;
    unsigned int year;
};

struct TimeData
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    unsigned int ms;
    uint8_t tzh;
    uint8_t tzm;
};

struct ClockData
{
    struct DateData date;
    struct TimeData time;
};

struct PositionData
{
    float utc;
    float lat; // Degrees: +/- 90
    float lon; // Degrees: +/- 180
    float alt;
    char mode;
    char status;
};

struct SpeedData
{
    float speed; // m/s
    float cog; // Degrees
    float magVar; // Degrees
};

struct operator_stats
{
    uint8_t stat;
    String shortOp;
    String longOp;
    unsigned long numOp;
    uint8_t act;
};

typedef enum
{
    SARA_R5_TCP = 6,
    SARA_R5_UDP = 17
} SARA_R5_socket_protocol_t;

typedef enum
{
    SARA_R5_MESSAGE_FORMAT_PDU = 0,
    SARA_R5_MESSAGE_FORMAT_TEXT = 1
} SARA_R5_message_format_t;

typedef enum
{
    SARA_R5_UTIME_MODE_STOP = 0,
    SARA_R5_UTIME_MODE_PPS,
    SARA_R5_UTIME_MODE_ONE_SHOT,
    SARA_R5_UTIME_MODE_EXT_INT
} SARA_R5_utime_mode_t;

typedef enum
{
    SARA_R5_UTIME_SENSOR_NONE = 0,
    SARA_R5_UTIME_SENSOR_GNSS_LTE = 1,
    SARA_R5_UTIME_SENSOR_LTE
} SARA_R5_utime_sensor_t;

typedef enum
{
    SARA_R5_UTIME_URC_CONFIGURATION_DISABLED = 0,
    SARA_R5_UTIME_URC_CONFIGURATION_ENABLED
} SARA_R5_utime_urc_configuration_t;

typedef enum
{
    SARA_R5_SIM_NOT_PRESENT = 0,
    SARA_R5_SIM_PIN_NEEDED,
    SARA_R5_SIM_PIN_BLOCKED,
    SARA_R5_SIM_PUK_BLOCKED,
    SARA_R5_SIM_NOT_OPERATIONAL,
    SARA_R5_SIM_RESTRICTED,
    SARA_R5_SIM_OPERATIONAL
    //SARA_R5_SIM_PHONEBOOK_READY, // Not reported by SARA-R5
    //SARA_R5_SIM_USIM_PHONEBOOK_READY, // Not reported by SARA-R5
    //SARA_R5_SIM_TOOLKIT_REFRESH_SUCCESSFUL, // Not reported by SARA-R5
    //SARA_R5_SIM_TOOLKIT_REFRESH_UNSUCCESSFUL, // Not reported by SARA-R5
    //SARA_R5_SIM_PPP_CONNECTION_ACTIVE, // Not reported by SARA-R5
    //SARA_R5_SIM_VOICE_CALL_ACTIVE, // Not reported by SARA-R5
    //SARA_R5_SIM_CSD_CALL_ACTIVE // Not reported by SARA-R5
} SARA_R5_sim_states_t;

const int RXBuffSize = 2056;
const int rxWindowUS = 1000;

#define SARA_R5_NUM_PSD_PROFILES 6 // Number of supported PSD profiles
#define SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS 11 // Number of supported PDP context identifiers
#define SARA_R5_NUM_HTTP_PROFILES 4 // Number of supported HTTP profiles

typedef enum
{
    SARA_R5_HTTP_OP_CODE_SERVER_IP = 0,
    SARA_R5_HTTP_OP_CODE_SERVER_NAME,
    SARA_R5_HTTP_OP_CODE_USERNAME,
    SARA_R5_HTTP_OP_CODE_PASSWORD,
    SARA_R5_HTTP_OP_CODE_AUTHENTICATION,
    SARA_R5_HTTP_OP_CODE_SERVER_PORT,
    SARA_R5_HTTP_OP_CODE_SECURE,
    SARA_R5_HTTP_OP_CODE_REQUEST_TIMEOUT,
    SARA_R5_HTTP_OP_CODE_ADD_CUSTOM_HEADERS = 9
} SARA_R5_http_op_codes_t;

typedef enum
{
    SARA_R5_HTTP_COMMAND_HEAD = 0,
    SARA_R5_HTTP_COMMAND_GET,
    SARA_R5_HTTP_COMMAND_DELETE,
    SARA_R5_HTTP_COMMAND_PUT,
    SARA_R5_HTTP_COMMAND_POST_FILE,
    SARA_R5_HTTP_COMMAND_POST_DATA,
    SARA_R5_HTTP_COMMAND_GET_FOTA = 100
} SARA_R5_http_commands_t;

typedef enum
{
    SARA_R5_HTTP_CONTENT_APPLICATION_X_WWW = 0,
    SARA_R5_HTTP_CONTENT_TEXT_PLAIN,
    SARA_R5_HTTP_CONTENT_APPLICATION_OCTET,
    SARA_R5_HTTP_CONTENT_MULTIPART_FORM,
    SARA_R5_HTTP_CONTENT_APPLICATION_JSON,
    SARA_R5_HTTP_CONTENT_APPLICATION_XML,
    SARA_R5_HTTP_CONTENT_USER_DEFINED
} SARA_R5_http_content_types_t;

typedef enum
{
    SARA_R5_PSD_CONFIG_PARAM_PROTOCOL = 0,
    SARA_R5_PSD_CONFIG_PARAM_APN,
    //SARA_R5_PSD_CONFIG_PARAM_USERNAME, // Not allowed on SARA-R5
    //SARA_R5_PSD_CONFIG_PARAM_PASSWORD, // Not allowed on SARA-R5
    SARA_R5_PSD_CONFIG_PARAM_DNS1 = 4,
    SARA_R5_PSD_CONFIG_PARAM_DNS2,
    //SARA_R5_PSD_CONFIG_PARAM_AUTHENTICATION, // Not allowed on SARA-R5
    //SARA_R5_PSD_CONFIG_PARAM_IP_ADDRESS, // Not allowed on SARA-R5
    //SARA_R5_PSD_CONFIG_PARAM_DATA_COMPRESSION, // Not allowed on SARA-R5
    //SARA_R5_PSD_CONFIG_PARAM_HEADER_COMPRESSION, // Not allowed on SARA-R5
    SARA_R5_PSD_CONFIG_PARAM_MAP_TO_CID = 100
} SARA_R5_pdp_configuration_parameter_t;

typedef enum
{
    SARA_R5_PSD_PROTOCOL_IPV4 = 0,
    SARA_R5_PSD_PROTOCOL_IPV6,
    SARA_R5_PSD_PROTOCOL_IPV4V6_V4_PREF,
    SARA_R5_PSD_PROTOCOL_IPV4V6_V6_PREF
} SARA_R5_pdp_protocol_type_t;

typedef enum
{
  SARA_R5_PSD_ACTION_RESET = 0,
  SARA_R5_PSD_ACTION_STORE,
  SARA_R5_PSD_ACTION_LOAD,
  SARA_R5_PSD_ACTION_ACTIVATE,
  SARA_R5_PSD_ACTION_DEACTIVATE
} SARA_R5_pdp_actions_t;

typedef enum
{
    MINIMUM_FUNCTIONALITY = 0, // (disable both transmit and receive RF circuits by deactivating both CS and PS services)
    FULL_FUNCTIONALITY = 1,
    AIRPLANE_MODE = 4,
    SIM_TOOLKIT_ENABLE_DEDICATED = 6,
    SIM_TOOLKIT_DISABLE_DEDICATED = 7,
    SIM_TOOLKIT_ENABLE_RAW = 9,
    FAST_SAFE_POWER_OFF = 10,
    //SILENT_RESET_WITHOUT_SIM = 15, // Not supported on SARA-R5
    SILENT_RESET_WITH_SIM = 16
    //MINIMUM_FUNCTIONALITY = 19, // Not supported on SARA-R5
    //DEEP_LOW_POWER_STATE = 127 // Not supported on SARA-R5
} SARA_R5_functionality_t;

class SARA_R5 : public Print
{
public:

    char saraRXBuffer[RXBuffSize];
    char saraResponseBacklog[RXBuffSize];

    //  Constructor
    SARA_R5(int powerPin = SARA_R5_POWER_PIN, int resetPin = SARA_R5_RESET_PIN, uint8_t maxInitDepth = 9);

    // Begin -- initialize BT module and ensure it's connected
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    boolean begin(SoftwareSerial &softSerial, unsigned long baud = 9600);
#endif
    boolean begin(HardwareSerial &hardSerial, unsigned long baud = 9600);

    // Debug prints
    void enableDebugging(Stream &debugPort = Serial); //Turn on debug printing. If user doesn't specify then Serial will be used.

    // Invert the polarity of the power pin - if required
    // Normally the SARA's power pin is pulled low and released to toggle the power
    // But the Asset Tracker needs this to be pulled high and released instead
    void invertPowerPin(boolean invert = false);

    // Loop polling and polling setup
    boolean bufferedPoll(void);
	  boolean processReadEvent(char* event);
    boolean poll(void);
    void setSocketReadCallback(void (*socketReadCallback)(int, String));
    void setSocketCloseCallback(void (*socketCloseCallback)(int));
    void setGpsReadCallback(void (*gpsRequestCallback)(ClockData time,
                                                       PositionData gps, SpeedData spd, unsigned long uncertainty));
    void setSIMstateReportCallback(void (*simStateRequestCallback)(SARA_R5_sim_states_t state));
    void setPSDActionCallback(void (*psdActionRequestCallback)(int result, IPAddress ip));
    void setPingCallback(void (*pingRequestCallback)(int retry, int p_size, String remote_hostname, IPAddress ip, int ttl, long rtt));
    void setHTTPCommandCallback(void (*httpCommandRequestCallback)(int profile, int command, int result));

    // Direct write/print to cell serial port
    virtual size_t write(uint8_t c);
    virtual size_t write(const char *str);
    virtual size_t write(const char *buffer, size_t size);

    // General AT Commands
    SARA_R5_error_t at(void);
    SARA_R5_error_t enableEcho(boolean enable = true);
    String getManufacturerID(void);
    String getModelID(void);
    String getFirmwareVersion(void);
    String getSerialNo(void);
    String getIMEI(void);
    String getIMSI(void);
    String getCCID(void);
    String getSubscriberNo(void);
    String getCapabilities(void);

    // Control and status AT commands
    SARA_R5_error_t reset(void);
    String clock(void);
    // TODO: Return a clock struct
    SARA_R5_error_t clock(uint8_t *y, uint8_t *mo, uint8_t *d,
                             uint8_t *h, uint8_t *min, uint8_t *s, uint8_t *tz);
    SARA_R5_error_t autoTimeZone(boolean enable);
    SARA_R5_error_t setUtimeMode(SARA_R5_utime_mode_t mode = SARA_R5_UTIME_MODE_PPS, SARA_R5_utime_sensor_t sensor = SARA_R5_UTIME_SENSOR_GNSS_LTE);
    SARA_R5_error_t getUtimeMode(SARA_R5_utime_mode_t *mode, SARA_R5_utime_sensor_t *sensor);
    SARA_R5_error_t setUtimeIndication(SARA_R5_utime_urc_configuration_t config = SARA_R5_UTIME_URC_CONFIGURATION_ENABLED);
    SARA_R5_error_t getUtimeIndication(SARA_R5_utime_urc_configuration_t *config);
    SARA_R5_error_t setUtimeConfiguration(int32_t offsetNanoseconds = 0, int32_t offsetSeconds = 0);
    SARA_R5_error_t getUtimeConfiguration(int32_t *offsetNanoseconds, int32_t *offsetSeconds);

    // Network service AT commands
    int8_t rssi(void);
    SARA_R5_registration_status_t registration(void);
    boolean setNetworkProfile(mobile_network_operator_t mno, boolean autoReset = false, boolean urcNotification = false);
    mobile_network_operator_t getNetworkProfile(void);
    typedef enum
    {
        PDP_TYPE_INVALID = -1,
        PDP_TYPE_IP = 0,
        PDP_TYPE_NONIP = 1,
        PDP_TYPE_IPV4V6 = 2,
        PDP_TYPE_IPV6 = 3
    } SARA_R5_pdp_type;
    SARA_R5_error_t setAPN(String apn, uint8_t cid = 1, SARA_R5_pdp_type pdpType = PDP_TYPE_IP); // Set the Access Point Name
    SARA_R5_error_t getAPN(int cid, String *apn, IPAddress *ip); // Return the apn and IP address for the chosen context identifier

    // SIM
    // Status report Mode:
    // Bit   States reported
    // 0     Reports the (U)SIM initialization status (<state>'s from 0 to 6 may be reported)
    // 1     Reports the (U)SIM phonebook initialization status (<state>'s from 7 to 8 may be reported)
    // 2     Reports the (U)SIM toolkit REFRESH proactive command execution result (<state>'s from 9 to 13 may be reported)
    // Note: For the SARA-R5: <state>=7, 8, 9, 10, 11, 12 and 13 are not reported.
    SARA_R5_error_t setSIMstateReportingMode(int mode);
    SARA_R5_error_t getSIMstateReportingMode(int *mode);

    typedef enum
    {
        L2P_DEFAULT,
        L2P_PPP,
        L2P_M_HEX,
        L2P_M_RAW_IP,
        L2P_M_OPT_PPP
    } SARA_R5_l2p_t;
    SARA_R5_error_t enterPPP(uint8_t cid = 1, char dialing_type_char = 0,
                                unsigned long dialNumber = 99, SARA_R5_l2p_t l2p = L2P_DEFAULT);

    uint8_t getOperators(struct operator_stats *op, int maxOps = 3);
    SARA_R5_error_t registerOperator(struct operator_stats oper);
    SARA_R5_error_t automaticOperatorSelection();
    SARA_R5_error_t getOperator(String *oper);
    SARA_R5_error_t deregisterOperator(void);

    // SMS -- Short Messages Service
    SARA_R5_error_t setSMSMessageFormat(SARA_R5_message_format_t textMode = SARA_R5_MESSAGE_FORMAT_TEXT);
    SARA_R5_error_t sendSMS(String number, String message);
    SARA_R5_error_t getPreferredMessageStorage(int *used, int *total, String memory = "ME");
    SARA_R5_error_t readSMSmessage(int location, String *unread, String *from, String *dateTime, String *message);

    // V24 Control and V25ter (UART interface) AT commands
    SARA_R5_error_t setBaud(unsigned long baud);
    SARA_R5_error_t setFlowControl(SARA_R5_flow_control_t value = SARA_R5_ENABLE_FLOW_CONTROL);

    // GPIO
    // GPIO pin map
    typedef enum
    {
        GPIO1 = 16,
        GPIO2 = 23,
        GPIO3 = 24,
        GPIO4 = 25,
        GPIO5 = 42,
        GPIO6 = 19
    } SARA_R5_gpio_t;
    // GPIO pin modes
    typedef enum
    {
        GPIO_MODE_INVALID = -1,
        GPIO_OUTPUT = 0,
        GPIO_INPUT,
        NETWORK_STATUS,
        GNSS_SUPPLY_ENABLE,
        GNSS_DATA_READY,
        GNSS_RTC_SHARING,
        JAMMING_DETECTION,
        SIM_CARD_DETECTION,
        HEADSET_DETECTION,
        GSM_TX_BURST_INDICATION,
        MODULE_STATUS_INDICATION,
        MODULE_OPERATING_MODE_INDICATION,
        I2S_DIGITAL_AUDIO_INTERFACE,
        SPI_SERIAL_INTERFACE,
        MASTER_CLOCK_GENRATION,
        UART_INTERFACE,
        WIFI_ENABLE,
        RING_INDICATION = 18,
        LAST_GASP_ENABLE,
        EXTERNAL_GNSS_ANTENNA,
        TIME_PULSE_GNSS,
        TIME_PULSE_OUTPUT,
        TIMESTAMP,
        FAST_POWER_OFF,
        LWM2M_PULSE,
        HARDWARE_FLOW_CONTROL,
        ANTENNA_TUNING,
        EXT_GNSS_TIME_PULSE,
        EXT_GNSS_TIMESTAMP,
        DTR_MODE,
        KHZ_32768_OUT = 32,
        PAD_DISABLED = 255
    } SARA_R5_gpio_mode_t;
    SARA_R5_error_t setGpioMode(SARA_R5_gpio_t gpio, SARA_R5_gpio_mode_t mode, int value = 0);
    SARA_R5_gpio_mode_t getGpioMode(SARA_R5_gpio_t gpio);

    // IP Transport Layer
    int socketOpen(SARA_R5_socket_protocol_t protocol, unsigned int localPort = 0);
    SARA_R5_error_t socketClose(int socket, int timeout = SARA_R5_2_MIN_TIMEOUT);
    SARA_R5_error_t socketConnect(int socket, const char *address, unsigned int port);
    SARA_R5_error_t socketWrite(int socket, const char *str);
    SARA_R5_error_t socketWrite(int socket, String str);
    SARA_R5_error_t socketWriteUDP(int socket, const char *address, int port, const char *str, int len = -1);
	  SARA_R5_error_t socketWriteUDP(int socket, String address, int port, String str, int len = -1);
    SARA_R5_error_t socketRead(int socket, int length, char *readDest);
    SARA_R5_error_t socketReadUDP(int socket, int length, char *readDest);
    SARA_R5_error_t socketListen(int socket, unsigned int port);
    int socketGetLastError();
    IPAddress lastRemoteIP(void);

    // Ping
    SARA_R5_error_t ping(String remote_host, int retry = 4, int p_size = 32, unsigned long timeout = 5000, int ttl = 32);

    // HTTP
    SARA_R5_error_t resetHTTPprofile(int profile); // Reset the HTTP profile. Note: The configured HTTP profile parameters are not saved in the non volatile memory.
    SARA_R5_error_t setHTTPserverIPaddress(int profile, IPAddress address); // Default: empty string
    SARA_R5_error_t setHTTPserverName(int profile, String server); // Default: empty string
    SARA_R5_error_t setHTTPusername(int profile, String username); // Default: empty string
    SARA_R5_error_t setHTTPpassword(int profile, String password); // Default: empty string
    SARA_R5_error_t setHTTPauthentication(int profile, boolean authenticate); // Default: no authentication
    SARA_R5_error_t setHTTPserverPort(int profile, int port); // Default: 80
    SARA_R5_error_t setHTTPsecure(int profile, boolean secure); // Default: disabled (HTTP on port 80). Set to true for HTTPS on port 443
    // TO DO: Add custom request headers
    SARA_R5_error_t getHTTPprotocolError(int profile, int *error_class, int *error_code); // Read the most recent HTTP protocol error for this profile
    SARA_R5_error_t sendHTTPGET(int profile, String path, String responseFilename);
    SARA_R5_error_t sendHTTPPOSTdata(int profile, String path, String responseFilename, String data, SARA_R5_http_content_types_t httpContentType);

    // Packet Switched Data
    SARA_R5_error_t setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, int value); // Set parameters in the chosen PSD profile
    SARA_R5_error_t setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, SARA_R5_pdp_protocol_type_t value); // Set parameters in the chosen PSD profile
    SARA_R5_error_t setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, String value); // Set parameters in the chosen PSD profile
    SARA_R5_error_t setPDPconfiguration(int profile, SARA_R5_pdp_configuration_parameter_t parameter, IPAddress value); // Set parameters in the chosen PSD profile
    SARA_R5_error_t performPDPaction(int profile, SARA_R5_pdp_actions_t action); // Performs the requested action for the specified PSD profile.
    SARA_R5_error_t activatePDPcontext(boolean status, int cid = -1); // Activates or deactivates the specified PDP context. Default to all (cid = -1)

    // GPS
    typedef enum
    {
        GNSS_SYSTEM_GPS = 1,
        GNSS_SYSTEM_SBAS = 2,
        GNSS_SYSTEM_GALILEO = 4,
        GNSS_SYSTEM_BEIDOU = 8,
        GNSS_SYSTEM_IMES = 16,
        GNSS_SYSTEM_QZSS = 32,
        GNSS_SYSTEM_GLONASS = 64
    } gnss_system_t;
    typedef enum
    {
        GNSS_AIDING_MODE_NONE = 0,
        GNSS_AIDING_MODE_AUTOMATIC = 1,
        GNSS_AIDING_MODE_ASSISTNOW_OFFLINE = 2,
        GNSS_AIDING_MODE_ASSISTNOW_ONLINE = 4,
        GNSS_AIDING_MODE_ASSISTNOW_AUTONOMOUS = 8
    } gnss_aiding_mode_t;
    boolean isGPSon(void);
    SARA_R5_error_t gpsPower(boolean enable = true,
                                gnss_system_t gnss_sys = GNSS_SYSTEM_GPS,
                                gnss_aiding_mode_t gnss_aiding = GNSS_AIDING_MODE_AUTOMATIC);
    SARA_R5_error_t gpsEnableClock(boolean enable = true);
    SARA_R5_error_t gpsGetClock(struct ClockData *clock);
    SARA_R5_error_t gpsEnableFix(boolean enable = true);
    SARA_R5_error_t gpsGetFix(float *lat, float *lon,
                                 unsigned int *alt, uint8_t *quality, uint8_t *sat);
    SARA_R5_error_t gpsGetFix(struct PositionData *pos);
    SARA_R5_error_t gpsEnablePos(boolean enable = true);
    SARA_R5_error_t gpsGetPos(struct PositionData *pos);
    SARA_R5_error_t gpsEnableSat(boolean enable = true);
    SARA_R5_error_t gpsGetSat(uint8_t *sats);
    SARA_R5_error_t gpsEnableRmc(boolean enable = true);
    SARA_R5_error_t gpsGetRmc(struct PositionData *pos, struct SpeedData *speed,
                                 struct ClockData *clk, boolean *valid);
    SARA_R5_error_t gpsEnableSpeed(boolean enable = true);
    SARA_R5_error_t gpsGetSpeed(struct SpeedData *speed);

    SARA_R5_error_t gpsRequest(unsigned int timeout, uint32_t accuracy, boolean detailed = true);

    // File system
    SARA_R5_error_t getFileContents(String filename, String *contents);

    // Functionality
    SARA_R5_error_t functionality(SARA_R5_functionality_t function = FULL_FUNCTIONALITY);

    // Send a custom command with an expected (potentially partial) response, store entire response
    SARA_R5_error_t sendCustomCommandWithResponse(const char *command, const char *expectedResponse,
                                               char *responseDest, unsigned long commandTimeout = SARA_R5_STANDARD_RESPONSE_TIMEOUT, boolean at = true);

private:
    HardwareSerial *_hardSerial;
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    SoftwareSerial *_softSerial;
#endif

    Stream *_debugPort;			 //The stream to send debug messages to if enabled. Usually Serial.
    boolean _printDebug = false; //Flag to print debugging variables

    int _powerPin;
    int _resetPin;
    boolean _invertPowerPin = false;

    unsigned long _baud;
    IPAddress _lastRemoteIP;
    IPAddress _lastLocalIP;
    uint8_t _maxInitDepth;
    uint8_t _currentInitDepth = 0;

    void (*_socketReadCallback)(int, String);
    void (*_socketCloseCallback)(int);
    void (*_gpsRequestCallback)(ClockData, PositionData, SpeedData, unsigned long);
    void (*_simStateReportCallback)(SARA_R5_sim_states_t);
    void (*_psdActionRequestCallback)(int, IPAddress);
    void (*_pingRequestCallback)(int, int, String, IPAddress, int, long);
    void (*_httpCommandRequestCallback)(int, int, int);

    typedef enum
    {
        SARA_R5_INIT_STANDARD,
        SARA_R5_INIT_AUTOBAUD,
        SARA_R5_INIT_RESET
    } SARA_R5_init_type_t;

    SARA_R5_error_t init(unsigned long baud, SARA_R5_init_type_t initType = SARA_R5_INIT_STANDARD);

    void powerOn(void);

    void hwReset(void);

    SARA_R5_error_t setMNOprofile(mobile_network_operator_t mno, boolean autoReset = false, boolean urcNotification = false);
    SARA_R5_error_t getMNOprofile(mobile_network_operator_t *mno);

    // Wait for an expected response (don't send a command)
    SARA_R5_error_t waitForResponse(const char *expectedResponse, const char *expectedError, uint16_t timeout);

    // Send command with an expected (potentially partial) response, store entire response
    SARA_R5_error_t sendCommandWithResponse(const char *command, const char *expectedResponse,
                                               char *responseDest, unsigned long commandTimeout, boolean at = true);

    // Send a command -- prepend AT if at is true
    int sendCommand(const char *command, boolean at);

    SARA_R5_error_t parseSocketReadIndication(int socket, int length);
    SARA_R5_error_t parseSocketReadIndicationUDP(int socket, int length);
	  SARA_R5_error_t parseSocketListenIndication(IPAddress localIP, IPAddress remoteIP);
    SARA_R5_error_t parseSocketCloseIndication(String *closeIndication);

    // UART Functions
    size_t hwPrint(const char *s);
    size_t hwWriteData(const char* buff, int len);
    size_t hwWrite(const char c);
    int readAvailable(char *inString);
    char readChar(void);
    int hwAvailable(void);
    void beginSerial(unsigned long baud);
    void setTimeout(unsigned long timeout);
    bool find(char *target);

    SARA_R5_error_t autobaud(unsigned long desiredBaud);

    char *sara_r5_calloc_char(size_t num);

    void pruneBacklog(void);

    // GPS Helper functions
    char *readDataUntil(char *destination, unsigned int destSize, char *source, char delimiter);
    boolean parseGPRMCString(char *rmcString, PositionData *pos, ClockData *clk, SpeedData *spd);

};

#endif //SPARKFUN_SARA_R5_ARDUINO_LIBRARY_H
