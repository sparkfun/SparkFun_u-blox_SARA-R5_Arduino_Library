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

#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
#include <SoftwareSerial.h>
#endif

#include <IPAddress.h>

#define SARA_R5_POWER_PIN -1
#define SARA_R5_RESET_PIN -1

typedef enum
{
    MNO_INVALID = -1,
    MNO_SW_DEFAULT = 0,
    MNO_SIM_ICCD = 1,
    MNO_ATT = 2,
    MNO_VERIZON = 3,
    MNO_TELSTRA = 4,
    MNO_TMO = 5,
    MNO_CT = 6
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

class SARA_R5 : public Print
{
public:
    //  Constructor
    SARA_R5(int powerPin = SARA_R5_POWER_PIN, int resetPin = SARA_R5_RESET_PIN, uint8_t maxInitDepth = 9);

    // Begin -- initialize BT module and ensure it's connected
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    boolean begin(SoftwareSerial &softSerial, unsigned long baud = 9600);
#endif
    boolean begin(HardwareSerial &hardSerial, unsigned long baud = 9600);

    // Debug prints
    void enableDebugging(Stream &debugPort = Serial); //Turn on debug printing. If user doesn't specify then Serial will be used.

    // Loop polling and polling setup
    boolean bufferedPoll(void);
	  boolean processReadEvent(char* event);
    boolean poll(void);
    void setSocketReadCallback(void (*socketReadCallback)(int, String));
    void setSocketCloseCallback(void (*socketCloseCallback)(int));
    void setGpsReadCallback(void (*gpsRequestCallback)(ClockData time,
                                                       PositionData gps, SpeedData spd, unsigned long uncertainty));

    // Direct write/print to cell serial port
    virtual size_t write(uint8_t c);
    virtual size_t write(const char *str);
    virtual size_t write(const char *buffer, size_t size);

    // General AT Commands
    SARA_R5_error_t at(void);
    SARA_R5_error_t enableEcho(boolean enable = true);
    String imei(void);
    String imsi(void);
    String ccid(void);

    // Control and status AT commands
    SARA_R5_error_t reset(void);
    String clock(void);
    // TODO: Return a clock struct
    SARA_R5_error_t clock(uint8_t *y, uint8_t *mo, uint8_t *d,
                             uint8_t *h, uint8_t *min, uint8_t *s, uint8_t *tz);
    SARA_R5_error_t autoTimeZone(boolean enable);

    // Network service AT commands
    int8_t rssi(void);
    SARA_R5_registration_status_t registration(void);
    boolean setNetwork(mobile_network_operator_t mno);
    mobile_network_operator_t getNetwork(void);
    typedef enum
    {
        PDP_TYPE_INVALID = -1,
        PDP_TYPE_IP = 0,
        PDP_TYPE_NONIP = 1,
        PDP_TYPE_IPV4V6 = 2,
        PDP_TYPE_IPV6 = 3
    } SARA_R5_pdp_type;
    SARA_R5_error_t setAPN(String apn, uint8_t cid = 1, SARA_R5_pdp_type pdpType = PDP_TYPE_IP);
    SARA_R5_error_t getAPN(String *apn, IPAddress *ip);

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
    SARA_R5_error_t getOperator(String *oper);
    SARA_R5_error_t deregisterOperator(void);

    // SMS -- Short Messages Service
    SARA_R5_error_t setSMSMessageFormat(SARA_R5_message_format_t textMode = SARA_R5_MESSAGE_FORMAT_TEXT);
    SARA_R5_error_t sendSMS(String number, String message);

    // V24 Control and V25ter (UART interface) AT commands
    SARA_R5_error_t setBaud(unsigned long baud);

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
    SARA_R5_error_t setGpioMode(SARA_R5_gpio_t gpio, SARA_R5_gpio_mode_t mode);
    SARA_R5_gpio_mode_t getGpioMode(SARA_R5_gpio_t gpio);

    // IP Transport Layer
    int socketOpen(SARA_R5_socket_protocol_t protocol, unsigned int localPort = 0);
    SARA_R5_error_t socketClose(int socket, int timeout = 1000);
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

private:
    HardwareSerial *_hardSerial;
#ifdef SARA_R5_SOFTWARE_SERIAL_ENABLED
    SoftwareSerial *_softSerial;
#endif

    Stream *_debugPort;			 //The stream to send debug messages to if enabled. Usually Serial.
    boolean _printDebug = false; //Flag to print debugging variables

    int _powerPin;
    int _resetPin;
    unsigned long _baud;
    IPAddress _lastRemoteIP;
    IPAddress _lastLocalIP;
    uint8_t _maxInitDepth;
    uint8_t _currentInitDepth = 0;

    void (*_socketReadCallback)(int, String);
    void (*_socketCloseCallback)(int);
    void (*_gpsRequestCallback)(ClockData, PositionData, SpeedData, unsigned long);

    typedef enum
    {
        SARA_R5_INIT_STANDARD,
        SARA_R5_INIT_AUTOBAUD,
        SARA_R5_INIT_RESET
    } SARA_R5_init_type_t;

    typedef enum
    {
        MINIMUM_FUNCTIONALITY = 0,
        FULL_FUNCTIONALITY = 1,
        SILENT_RESET = 15,
        SILENT_RESET_W_SIM = 16
    } SARA_R5_functionality_t;

    SARA_R5_error_t init(unsigned long baud, SARA_R5_init_type_t initType = SARA_R5_INIT_STANDARD);

    void powerOn(void);

    void hwReset(void);

    SARA_R5_error_t functionality(SARA_R5_functionality_t function = FULL_FUNCTIONALITY);

    SARA_R5_error_t setMno(mobile_network_operator_t mno);
    SARA_R5_error_t getMno(mobile_network_operator_t *mno);

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
};

#endif //SPARKFUN_SARA_R5_ARDUINO_LIBRARY_H
