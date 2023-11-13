/*
 * Copyright 2022 by Michael Ammann (@mazgch)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef __LTE_H__
#define __LTE_H__

#include <base64.h>
#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h>

#include "HW.h"
#include "CONFIG.h"
#include "UBXFILE.h"

// MNO_GLOBAL is the factory-programmed default
// If you are in Europe, you may find no operators unless you choose MNO_STD_EUROPE
const mobile_network_operator_t MOBILE_NETWORK_OPERATOR = MNO_GLOBAL;

String CONFIG_VALUE_SIMPIN = ""; // Set the SIM PIN here if needed

String CONFIG_VALUE_LTEAPN = ""; // Define the APN here if needed

// Set this to (e.g.) SARA_R5_PSD_PROTOCOL_IPV4V6_V4_PREF if needed. O2 (UK) returns strPdpType "IP" but expects "IPV4V6_V4_PREF"
SARA_R5_pdp_protocol_type_t preferredPdpProtocol = SARA_R5_PSD_PROTOCOL_IPV4;
  
const int LTE_1S_RETRY            =        1000;  //!< standard 1s retry
const int LTE_DETECT_RETRY        =        5000;  //!< delay between detect attempts
const int LTE_CHECKSIM_RETRY      =       60000;  //!< delay between SIM Card check attempts, SIM detection may be disables and you need to restart
const int LTE_ACTIVATION_RETRY    =       10000;  //!< delay between activation attempts
const int LTE_PROVISION_RETRY     =       60000;  //!< delay between provisioning attempts, provisioning may consume data
const int LTE_CONNECT_RETRY       =       10000;  //!< delay between server connection attempts to correction severs 
const int LTE_MQTTCMD_DELAY       =         100;  //!< the client is not happy if multiple commands are sent too fast

const int LTE_POWER_ON_PULSE        =      2000;  //!< Power on pulse width (2s works for for SARA, LARA and LENA)
const int LTE_POWER_ON_WAITTIME     =      4000;  //!< Dont't do anything duing this time after the power on pulse
const int LTE_POWER_ON_WAITTIME_MAX =     10000;  //!< If the device does not respond until this time, it has failed
const int LTE_POWER_ON_WAITSIMREADY =      4000;  //!< It usually takes a few seconds to detect the SIM card. 

const int LTE_PSD_PROFILE         =           0;  //!< The packet switched data profile used
const int LTE_HTTP_PROFILE        =           0;  //!< The HTTP profile used duing provisioning
const int LTE_SEC_PROFILE_HTTP    =           1;  //!< The security profile used for the HTTP connections druing provisioning
const int LTE_SEC_PROFILE_MQTT    =           0;  //!< The security profile used for the MQTT connections
const char* FILE_REQUEST          =  "req.json";  //!< Temporarly file name used for the request during HTTP GET transations (ZTP)  
const char* FILE_RESP             = "resp.json";  //!< Temporarly file name used for the response during HTTP POST and GET transations (AWS/ZTP)
const char* SEC_ROOT_CA           ="aws-rootCA";  //!< Temporarly file name used when injecting the ROOT CA
const char* SEC_CLIENT_CERT       =   "pp-cert";  //!< Temporarly file name used when injecting the client certificate
const char* SEC_CLIENT_KEY        =    "pp-key";  //!< Temporarly file name used when injecting the client keys

const uint16_t HTTPS_PORT         =         443;  //!< The HTTPS default port

const int LTE_BAUDRATE            =      115200;  //!< baudrates 230400, 460800 or 921600 cause issues even when CTS/RTS is enabled

const char* LTE_TASK_NAME         =       "Lte";  //!< Lte task name
const int LTE_STACK_SIZE          =      4*1024;  //!< Lte task stack size
const int LTE_TASK_PRIO           =           1;  //!< Lte task priority
const int LTE_TASK_CORE           =           1;  //!< Lte task MCU code

// helper macros to handle the AT interface errors  
#define LTE_CHECK_INIT            int _step = 0; (void)_step; SARA_R5_error_t _err = SARA_R5_SUCCESS   //!< init variable
#define LTE_CHECK_OK              (SARA_R5_SUCCESS == _err)                               //!< record the return result
#define LTE_CHECK(x)              if (SARA_R5_SUCCESS == _err) _step = x, _err            //!< interim evaluate
#define LTE_CHECK_EVAL(txt)       if (SARA_R5_SUCCESS != _err) log_e(txt ", AT sequence failed at step %d with error %d", _step, _err) //!< final verdict and log_e report
  
extern class LTE Lte; //!< Forward declaration of class

/** This class encapsulates all LTE functions. 
*/
class LTE : public SARA_R5 {

public:

  /** constructor
   *  \note we do not pass the pins to the class as we prefer to be in control of them 
   */
  LTE() : SARA_R5{ PIN_INVALID/*LTE_PWR_ON*/, PIN_INVALID/*LTE_RESET*/, 3/*retries*/ } { 
    state = INIT;
    hwInit();
  }

  /** initialize the object, this spins of a worker task. 
   */
  void init(void) {
    xTaskCreatePinnedToCore(task, LTE_TASK_NAME, LTE_STACK_SIZE, this, LTE_TASK_PRIO, NULL, LTE_TASK_CORE);
  }

  // Inject MGA (AssistNow) data directly to the internal GNSS using Send of UBX string +UGUBX
  SARA_R5_error_t injectMGA(uint8_t *buf, size_t len) {
    if (module.startsWith("SARA-R510M8S")) {

      size_t dataPtr = 0;     // Pointer into buf
      size_t bytesPushed = 0;      // Keep count
    
      while (dataPtr < len) // Keep going until we have processed all the bytes
      {
        // Start by checking the validity of the packet being pointed to
        bool dataIsOK = true;
    
        dataIsOK &= (*(buf + dataPtr + 0) == 0xB5);   // Check for 0xB5
        dataIsOK &= (*(buf + dataPtr + 1) == 0x62);   // Check for 0x62
        dataIsOK &= (*(buf + dataPtr + 2) == 0x13); // Check for class UBX-MGA
    
        size_t packetLength = ((size_t) * (buf + dataPtr + 4)) | (((size_t) * (buf + dataPtr + 5)) << 8); // Extract the length
    
        uint8_t checksumA = 0;
        uint8_t checksumB = 0;
        // Calculate the checksum bytes
        // Keep going until the end of the packet is reached (payloadPtr == (dataPtr + packetLength))
        // or we reach the end of the AssistNow data (payloadPtr == len)
        for (size_t payloadPtr = dataPtr + ((size_t)2); (payloadPtr < (dataPtr + packetLength + ((size_t)6))) && (payloadPtr < len); payloadPtr++)
        {
          checksumA += *(buf + payloadPtr);
          checksumB += checksumA;
        }
        // Check the checksum bytes
        dataIsOK &= (checksumA == *(buf + dataPtr + packetLength + ((size_t)6)));
        dataIsOK &= (checksumB == *(buf + dataPtr + packetLength + ((size_t)7)));
    
        dataIsOK &= ((dataPtr + packetLength + ((size_t)8)) <= len); // Check we haven't overrun
    
        // If the data is valid, push it
        if (dataIsOK)
        {
          SARA_R5_error_t err;
          
          char *command;
          const char ugubx[] = "+UGUBX=\""; // Include the first quote
          command = (char *)calloc(strlen(ugubx) + 3 + ((packetLength + 8) * 2), sizeof(char));
          if (command == nullptr)
            return SARA_R5_ERROR_OUT_OF_MEMORY;
          sprintf(command, "%s", ugubx);

          size_t ptr = strlen(ugubx);
          for (size_t i = 0; i < packetLength + 8; i++)
            sprintf(&command[ptr++], "%02X", *(buf + dataPtr + i));
          sprintf(&command[ptr], "\"");
          
          err = sendCommandWithResponse(command, SARA_R5_RESPONSE_OK,
                                        nullptr, SARA_R5_STANDARD_RESPONSE_TIMEOUT);
          free(command);
  
          if (err == SARA_R5_ERROR_SUCCESS) {
            bytesPushed += packetLength + ((size_t)8); // Increment bytesPushed if the push was successful
            log_i("packet ID 0x%02X length %d", *(buf + dataPtr + 3), packetLength);  
          }
          else {
            log_e("send UBX fail!");
          }
  
          dataPtr += packetLength + ((size_t)8); // Point to the next message
        }
        else
        {
          // The data was invalid. Send a debug message and then try to find the next 0xB5
          log_w("bad data - ignored! dataPtr is %d", dataPtr);
    
          while ((dataPtr < len) && (*(buf + ++dataPtr) != 0xB5))
          {
            ; // Increment dataPtr until we are pointing at the next 0xB5 - or we reach the end of the data
          }
        }
      }
    
      if (bytesPushed == len)
        return (SARA_R5_ERROR_SUCCESS);
    }
    return (SARA_R5_ERROR_ERROR);
  }
  
protected:
 
  // -----------------------------------------------------------------------
  // MQTT / PointPerfect
  // -----------------------------------------------------------------------

  std::vector<String> topics; //!< vector with current subscribed topics
  String subTopic;            //!< requested topic to be subscribed (needed by the callback) 
  String unsubTopic;          //!< requested topic to be un-subscribed (needed by the callback)
  int mqttMsgs;               //!< remember the number of messages pending indicated by the URC

  //! this helper deals with some AT commands that are not yet implemted in LENA-R8 and throw a warning
  SARA_R5_error_t LTE_IGNORE_LENA(SARA_R5_error_t err) { 
      if ((err != SARA_R5_SUCCESS) && module.startsWith("LENA-R8")) {
        log_w("AT command error ignored due to LENA-R8 IP Status");
        err = SARA_R5_SUCCESS;
      }
      return err; 
  }
  
  /** Connect to the Thingstream PointPerfect server using the credentials from ZTP process
   *  \param id  the client ID for this device
   */
  void mqttConnect(String id) {
    String rootCa = AWS_CERT_CA;
    String broker = AWS_IOT_ENDPOINT;
    String cert = AWS_CERT_CRT;
    String key = AWS_CERT_PRIVATE;
    // disconncect must fail here, so that we can connect 
    setMQTTCommandCallback(mqttCallbackStatic); // callback will advance state
    // make sure the client is disconnected here
    if (SARA_R5_SUCCESS == disconnectMQTT()) {
      log_i("forced disconnect"); // if this sucessful it means were were still connected. 
    } else {
      log_i("connect to \"%s:%d\" as client \"%s\"", broker.c_str(), MQTT_BROKER_PORT, id.c_str());
      LTE_CHECK_INIT;
      LTE_CHECK(1)  = setSecurityManager(SARA_R5_SEC_MANAGER_OPCODE_IMPORT, SARA_R5_SEC_MANAGER_ROOTCA,         SEC_ROOT_CA,     rootCa);
      LTE_CHECK(2)  = setSecurityManager(SARA_R5_SEC_MANAGER_OPCODE_IMPORT, SARA_R5_SEC_MANAGER_CLIENT_CERT,    SEC_CLIENT_CERT, cert);
      LTE_CHECK(3)  = setSecurityManager(SARA_R5_SEC_MANAGER_OPCODE_IMPORT, SARA_R5_SEC_MANAGER_CLIENT_KEY,     SEC_CLIENT_KEY,  key);
      LTE_CHECK(4)  = LTE_IGNORE_LENA( resetSecurityProfile(LTE_SEC_PROFILE_MQTT) );
      LTE_CHECK(5)  = configSecurityProfile(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_CERT_VAL_LEVEL,     SARA_R5_SEC_PROFILE_CERTVAL_OPCODE_YESNOURL);
      LTE_CHECK(6)  = configSecurityProfile(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_TLS_VER,            SARA_R5_SEC_PROFILE_TLS_OPCODE_VER1_2);
      LTE_CHECK(7)  = configSecurityProfile(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_CYPHER_SUITE,       SARA_R5_SEC_PROFILE_SUITE_OPCODE_PROPOSEDDEFAULT);
      LTE_CHECK(8)  = configSecurityProfileString(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_ROOT_CA,      SEC_ROOT_CA);
      LTE_CHECK(9)  = configSecurityProfileString(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_CLIENT_CERT,  SEC_CLIENT_CERT);
      LTE_CHECK(10) = configSecurityProfileString(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_CLIENT_KEY,   SEC_CLIENT_KEY);
      LTE_CHECK(11) = configSecurityProfileString(LTE_SEC_PROFILE_MQTT, SARA_R5_SEC_PROFILE_PARAM_SNI,          broker);
      LTE_CHECK(12) = nvMQTT(SARA_R5_MQTT_NV_RESTORE);
      LTE_CHECK(13) = setMQTTclientId(id);
      LTE_CHECK(14) = setMQTTserver(broker, MQTT_BROKER_PORT);
      LTE_CHECK(15) = setMQTTsecure(true, LTE_SEC_PROFILE_MQTT);
      LTE_CHECK(16) = connectMQTT();
      LTE_CHECK_EVAL("setup and connect");
      mqttMsgs = 0;
      topics.clear();
      subTopic = "";
      unsubTopic = "";
    }
  }

  /** Disconnect and cleanup the MQTT connection
   *  \return true if already disconnected (no need to wait for the callback) 
   */
  bool mqttStop(void) {
    SARA_R5_error_t err = disconnectMQTT();
    if (SARA_R5_SUCCESS == err) {
      log_i("disconnect");
    } else {
      log_e("disconnect, failed with error %d", err);
    }
    return SARA_R5_SUCCESS != err;
  }
  
  /** The MQTT task is responsible for:
   *  1) subscribing to topics
   *  2) unsubscribing from topics 
   *  3) read MQTT data from the modem and inject it into the GNSS receiver
   */
  void mqttTask(void) {
    /* The LTE modem has difficulties subscribing/unsubscribing more than one topic at the same time
     * We can only start one operation at a time wait for the URC and add a extra delay before we can 
     * do the next operation.
     */
    bool busy = (0 < subTopic.length()) || (0 < unsubTopic.length());
    if (!busy) {
      std::vector<String> newTopics = Config.getTopics();
      // loop through new topics and subscribe to the first topic that is not in our curent topics list. 
      for (auto it = newTopics.begin(); (it != newTopics.end()) && !busy; it = std::next(it)) {
        String topic = *it;
        std::vector<String>::iterator pos = std::find(topics.begin(), topics.end(), topic);
        if (pos == topics.end()) {
          SARA_R5_error_t err = subscribeMQTTtopic(0,topic);
          if (SARA_R5_SUCCESS == err) {
            log_d("subscribe requested topic \"%s\" qos %d", topic.c_str(), 0);
            subTopic = topic;
          } else {
            log_e("subscribe request topic \"%s\" qos %d, failed with error %d", topic.c_str(), 0, err);
          }
          busy = true;
        }
      }
      // loop through current topics and unsubscribe to the first topic that is not in the new topics list. 
      for (auto it = topics.begin(); (it != topics.end()) && !busy; it = std::next(it)) {
        String topic = *it;
        std::vector<String>::iterator pos = std::find(newTopics.begin(), newTopics.end(), topic);
        if (pos == newTopics.end()) {
          SARA_R5_error_t err = unsubscribeMQTTtopic(topic);
          if (SARA_R5_SUCCESS == err) {
            log_d("unsubscribe requested topic \"%s\"", topic.c_str());
            unsubTopic = topic;
          } else {
            log_e("unsubscribe request topic \"%s\", failed with error %d", topic.c_str(), err);
          }
          busy = true;
        }
      }
      if (!busy && (0 < mqttMsgs)) {
        // at this point we are properly subscribed to the needed topics and can now read data
        log_d("read request %d msg", mqttMsgs);
        // The MQTT API does not allow getting the size before actually reading the data. So we 
        // have to allocate a big enough buffer. PointPerfect may send upto 9kB on the MGA topic.
        uint8_t *buf = new uint8_t[MQTT_MAX_MSG_SIZE];
        if (buf != NULL) {
          String topic;
          int len = -1;
          int qos = -1;
          SARA_R5_error_t err = readMQTT(&qos, &topic, buf, MQTT_MAX_MSG_SIZE, &len);
          if (SARA_R5_SUCCESS == err) {
            mqttMsgs = 0; // expect a URC afterwards
            const char* strTopic = topic.c_str();
            log_i("topic \"%s\" read %d bytes", strTopic, len);
            // if we detect data from a topic, then why not unsubscribe from it. 
            std::vector<String>::iterator pos = std::find(topics.begin(), topics.end(), topic);
            if (pos == Lte.topics.end()) {
              log_e("getting data from an unexpected topic \"%s\"", strTopic);
              if (!busy) {
                err = unsubscribeMQTTtopic(topic);
                if (SARA_R5_SUCCESS == err) {
                  log_d("unsubscribe requested for unexpected topic \"%s\"", strTopic);
                  unsubTopic = topic;
                } else {
                  log_e("unsubscribe request for unexpected topic \"%s\", failed with error %d", topic.c_str(), err);
                }
                busy = true;
              }
            } else {
              // anything else can be sent to the GNSS as is
              len = injectMGA(buf, (size_t)len);
            }
          } else {
            log_e("read failed with error %d", err);
          }
          // we need to free the buffer, as the inject function took a copy with only the required size
          delete [] buf;
        }
      }
    }
  }
    
  /** The MQTT callback processes the URC and is responsible for advancing the state. 
   *  \param command  the MQTT command
   *  \param request  the response code 1 = sucess, 0 = error
   */
  void mqttCallback(int command, int result) {
    log_d("%d command %d result %d", command, result);
    if (result == 0) {
      int code, code2;
      SARA_R5_error_t err = getMQTTprotocolError(&code, &code2);
      if (SARA_R5_SUCCESS == err) {
        log_e("command %d protocol error code %d code2 %d", command, code, code2);  
      } else {
        log_e("command %d protocol error failed with error", command, err);
      }
    } else { 
      switch (command) {
        case SARA_R5_MQTT_COMMAND_LOGIN:
          if (state != ONLINE) {
            log_e("login wrong state");
          } else {
            log_i("login");
            setState(MQTT, LTE_MQTTCMD_DELAY);
          }
          break;
        case SARA_R5_MQTT_COMMAND_LOGOUT:
          if ((state != MQTT) && (state != ONLINE)) {
            log_e("logout wrong state");
          } else {
            log_i("logout");
            mqttMsgs = 0;
            topics.clear();
            subTopic = "";
            unsubTopic = "";
            setState(ONLINE, LTE_MQTTCMD_DELAY);
          }
          break;
        case SARA_R5_MQTT_COMMAND_SUBSCRIBE:
          if (state != MQTT) {
            log_e("subscribe wrong state");
          } else if (!subTopic.length()) {
            log_e("subscribe result %d but no topic", result);
          } else {
            log_i("subscribe result %d topic \"%s\"", result, subTopic.c_str());
            topics.push_back(subTopic);
            subTopic = "";
            setState(MQTT, LTE_MQTTCMD_DELAY);
          }  
          break;
        case SARA_R5_MQTT_COMMAND_UNSUBSCRIBE:
          if (state != MQTT) {
            log_e("unsubscribe wrong state");
          } else if (!unsubTopic.length()) {
            log_e("unsubscribe result %d but no topic", result);
          } else {
            std::vector<String>::iterator pos = std::find(topics.begin(), topics.end(), unsubTopic);
            if (pos == topics.end()) {
              topics.erase(pos);
              log_i("unsubscribe result %d topic \"%s\"", result, unsubTopic.c_str());
              unsubTopic = "";
              setState(MQTT, LTE_MQTTCMD_DELAY);
            } else {
              log_e("unsubscribe result %d topic \"%s\" but topic not in list", result, unsubTopic.c_str());
            }
          } 
          break;
        case SARA_R5_MQTT_COMMAND_READ: 
          if (state != MQTT) {
            log_e("read wrong state");
          } else {
            log_d("read result %d", result);
            mqttMsgs = result;
            setState(MQTT, LTE_MQTTCMD_DELAY);
          }
          break;
        default:
          break;
      }
    }
  }
  //! static callback helper, regCallback will do the real work
  static void mqttCallbackStatic(int command, int result) {
    Lte.mqttCallback(command, result);
  }
  

  // -----------------------------------------------------------------------
  // LTE 
  // -----------------------------------------------------------------------
  
  String module; //!< a string holding the module name (SARA-R5, LARA-R6 or LENA-R8)

  /** detect the LTE modem
   *  \return  the detection status
   */
  bool lteDetect(void) {
    bool ok = hwReady();
    if (ok) {
      module = getModelID();
      String version = getFirmwareVersion();
      log_i("config manufacturer \"%s\" model=\"%s\" version=\"%s\"", 
          getManufacturerID().c_str(), module.c_str(), version.c_str());
      if ((version.toDouble() < 0.13) && module.startsWith("LARA-R6")) {
        log_e("LARA-R6 firmware %s has MQTT limitations, please update firmware", version.c_str());
      }
      else if ((version.toDouble() < 2.00) && module.startsWith("LENA-R8")) {
        log_e("LENA-R8 firmware %s has limitations, please update firmware", version.c_str());
      }
#if ((HW_TARGET == UBLOX_XPLR_HPG2_C214) || (HW_TARGET == MAZGCH_HPG_SOLUTION_V09))
      // enableSIMDetectAndHotswap();
#endif
      // wait for the SIM to get ready ... this can take a while (<4s)
      SARA_R5_error_t err = SARA_R5_ERROR_ERROR;
      for (int i = 0; i < LTE_POWER_ON_WAITSIMREADY/100; i ++) {
        err = getSimStatus(NULL);
        if (SARA_R5_ERROR_ERROR != err)
          break;
        delay(100);
      }
      if (SARA_R5_ERROR_ERROR == err) {
        log_e("SIM card not found, err %d", err);
      }
    }    
    return ok;
  }
  
  /** initialize the LTE modem and report useful information
   *  \return  initialisation process sucess
   */
  bool lteInit(void) {
    String code;
    LTE_CHECK_INIT;
    LTE_CHECK(1) = getSimStatus(&code); 
    if (LTE_CHECK_OK && code.equals("SIM PIN")) {
      if (CONFIG_VALUE_SIMPIN.length()) {
        LTE_CHECK(2) = setSimPin(CONFIG_VALUE_SIMPIN.c_str());
        LTE_CHECK(3) = getSimStatus(&code); // retry get the SIM status
        LTE_CHECK_EVAL("SIM card initialisation");
      }
    }
    if (LTE_CHECK_OK) {
      if (code.equals("READY")) {
        log_i("SIM card status \"%s\" CCID=\"%s\"", code.c_str(), getCCID().c_str());
        String subNo = getSubscriberNo();
        int from = subNo.indexOf(",\"");
        int to = (-1 != from) ? subNo.indexOf("\"", from + 2) : -1;
        subNo = ((-1 != from) && (-1 != to)) ? subNo.substring(from + 2, to) : "";
        log_i("IMEI=\"%s\" IMSI=\"%s\" subscriber=\"%s\"", getIMEI().c_str(), getIMSI().c_str(), subNo.c_str());
        // configure the MNO profile 
        if (!module.startsWith("LENA-R8")) {
          if (!setNetworkProfile(MOBILE_NETWORK_OPERATOR)) {
            log_e("detect setting network profile for MNO %d failed", MOBILE_NETWORK_OPERATOR);
          }
        }       
        // register the callbacks 
        LTE_CHECK_INIT;
        LTE_CHECK(1) = setEpsRegistrationCallback(epsRegCallbackStatic);
        LTE_CHECK(2) = setRegistrationCallback(regCallbackStatic);
        // set the APn
        if (CONFIG_VALUE_LTEAPN.length()) {
          LTE_CHECK(3) = setAPN(CONFIG_VALUE_LTEAPN);
        }
        LTE_CHECK_EVAL("callback and apn config");
        return true;
      }
      else {
        log_w("SIM card status \"%s\"", code.c_str());
      }
    }
    return false;
  } 

  //! helper look up table for string conversion of registration status
  const char *REG_STATUS_LUT[11] = { 
    "not registered", 
    "home", 
    "searching", 
    "denied", 
    "unknown", 
    "roaming", 
    "home sms only", 
    "roaming sms only", 
    "emergency service only",
    "home cfsb not preferred", 
    "roaming cfsb not preferred" 
  };
  
  //! helper look up table for string conversion of network activation
  const char *REG_ACT_LUT[10] = { 
    "GSM", 
    "GSM COMPACT", 
    "UTRAN", 
    "GSM/GPRS + EDGE", 
    "UTRAN + HSDPA", 
    "UTRAN + HSUPA", 
    "UTRAN + HSDPA + HSUPA", 
    "E-UTRAN", 
    "EC-GSM-IoT (A/Gb mode)", 
    "E-UTRAN (NB-S1 mode)" 
  }; 

  //! helper to safely convert the string with above tables
  #define REG_LUT(l, x) ((((unsigned int)x) <= (sizeof(l)/sizeof(*l))) ? l[x] : "unknown")

  /** check if we are registered
   *  \return registration success 
   */
  bool lteRegistered(void) {
    SARA_R5_registration_status_t status = registration(true); // EPS
    const char* statusText = REG_LUT(REG_STATUS_LUT, status);
    if ((status == SARA_R5_REGISTRATION_HOME) || (status == SARA_R5_REGISTRATION_ROAMING)) {
      String op = "";
      getOperator(&op);
      log_i("registered %d(%s) operator \"%s\" rssi %d clock \"%s\"", 
              status, statusText, op.c_str(), rssi(), clock().c_str());
      // AT+UPSV?
      // AT+CLCK="SC",2
      return true;
    }
    else {
      log_d("EPS registration status %d(%s), waiting ...", status, statusText);
    }
    return false;
  }

  /** evaluate registration status and report it
   *  \param status     the registration status
   *  \param tacLac     the tac or lac 
   *  \param ci         the ci 
   *  \param Act        the network activation
   *  \param strTacLac  a string indicating if tacLac is either tac or lac. 
   */
  void regCallback(SARA_R5_registration_status_t status, unsigned int tacLac, unsigned int ci, int Act, const char* strTacLac)
  {
    const char* actText = REG_LUT(REG_ACT_LUT, Act);
    const char* statusText = REG_LUT(REG_STATUS_LUT, status);
    log_d("status %d(%s) %s \"%04X\" ci \"%08X\" Act %d(%s)", status, statusText, strTacLac, tacLac, ci, Act, actText); /*unused*/ (void)actText; (void)statusText;
    if (((status == SARA_R5_REGISTRATION_HOME) || (status == SARA_R5_REGISTRATION_ROAMING)) && (state < REGISTERED)) {
      setState(REGISTERED);
    } else if ((status == SARA_R5_REGISTRATION_SEARCHING) && (state >= REGISTERED)) {
      setState(WAITREGISTER);
    }
  }
  //! static callback helper, regCallback will do the real work
  static void epsRegCallbackStatic(SARA_R5_registration_status_t status, unsigned int tac, unsigned int ci, int Act) {
    Lte.regCallback(status, tac, ci, Act, "tac");
  }
  //! static callback helper, regCallback will do the real work
  static void regCallbackStatic(SARA_R5_registration_status_t status, unsigned int lac, unsigned int ci, int Act) {
    Lte.regCallback(status, lac, ci, Act, "lac");
  }
  
  /** make sure the modem is activated properly. Some modem do this automatically other need some help. 
   *  \return  activaion sucess 
   */
  bool lteActivate(void) {
    if (module.startsWith("LARA-R6")) {
      return true;
    } else if (module.startsWith("LENA-R8")) {
      String apn;
      IPAddress ip(0, 0, 0, 0);
      SARA_R5_pdp_type pdpType = PDP_TYPE_INVALID;
      LTE_CHECK_INIT;
      LTE_CHECK(1) = getAPN(0, &apn, &ip, &pdpType);
      // on LENA-R8 we need to move a IP context 0 to a different context id other than 0
      // ok if these commands fail as the context could be already active ... 
      if (LTE_CHECK_OK && (0 < apn.length()) && (pdpType != PDP_TYPE_NONIP)) {
        setAPN(apn, 1 /* LENA only has contexts 0 to 7, do not use SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS-1 */ , pdpType);
      }
      activatePDPcontext(true);           
      LTE_CHECK_EVAL("LTE activate context");
      if (LTE_CHECK_OK) {
        return true;
      }
    } else /* SARA-R5 */ {
      performPDPaction(LTE_PSD_PROFILE, SARA_R5_PSD_ACTION_DEACTIVATE);
      for (int cid = 0; cid < SARA_R5_NUM_PDP_CONTEXT_IDENTIFIERS; cid++)
      {
        String apn;
        IPAddress ip(0, 0, 0, 0);
        SARA_R5_pdp_type pdpType = PDP_TYPE_INVALID;
        LTE_CHECK_INIT;
        LTE_CHECK(1) = getAPN(cid, &apn, &ip, &pdpType);
        if (SARA_R5_PSD_PROTOCOL_IPV4 != preferredPdpProtocol)
          pdpType = (SARA_R5_pdp_type)preferredPdpProtocol; // Override the PDP Protocol if desired
        if ((LTE_CHECK_OK) && (0 < apn.length()) && (PDP_TYPE_INVALID != pdpType))
        {
          // Activate the profile
          log_i("activate profile for apn \"%s\" with IP %s pdp %d", apn.c_str(), ip.toString().c_str(), pdpType);                
          setPSDActionCallback(psdCallbackStatic);
          LTE_CHECK(2) = setPDPconfiguration(LTE_PSD_PROFILE, SARA_R5_PSD_CONFIG_PARAM_PROTOCOL, pdpType);
          LTE_CHECK(3) = setPDPconfiguration(LTE_PSD_PROFILE, SARA_R5_PSD_CONFIG_PARAM_MAP_TO_CID, cid);
          LTE_CHECK(4) = performPDPaction(LTE_PSD_PROFILE, SARA_R5_PSD_ACTION_ACTIVATE);
          LTE_CHECK_EVAL("profile activation");
          if (LTE_CHECK_OK) {
            return true; // abort the loop as we found a good profile
          }
        }
      }
    }
    return false;
  }

  /** the packet switched data activation callback
   *  \param profile  the psd profile
   *  \param ip       the ip address for this profile
   */
  static void psdCallbackStatic(int profile, IPAddress ip) {
    log_d("psdCallback profile %d  IP %s", profile, ip.toString().c_str());
    if (profile == LTE_PSD_PROFILE) {
      Lte.setState(ONLINE);
    }
  }
  
  // -----------------------------------------------------------------------
  // STATEMACHINE
  // -----------------------------------------------------------------------

  //! states of the statemachine 
  typedef enum { 
    INIT = 0, 
    CHECKSIM, 
    SIMREADY, 
    WAITREGISTER, 
    REGISTERED, 
    ONLINE, 
    MQTT, 
    NTRIP, 
    NUM 
  } STATE;
  //! string conversion helper table, must be aligned and match with STATE
  const char* STATE_LUT[STATE::NUM] = {
    "init", 
    "check sim", 
    "sim ready", 
    "wait register", 
    "registered",
    "online",
    "mqtt", 
    "ntrip"
  }; 
  STATE state;            //!< the current state
  int32_t ttagNextTry;    //!< time tag when to call the state machine again

  /** advance the state and report transitions
   *  \param newState  the new state
   *  \param delay     schedule delay
   */
  void setState(STATE newState, int32_t delay = 0) {
    if (state != newState) {
      log_i("state change %d(%s)", newState, STATE_LUT[newState]);
      state = newState;
    }
    ttagNextTry = millis() + delay; 
  }
  
  /* FreeRTOS static task function, will just call the objects task function  
   * \param pvParameters the Lte object (this)
   */
  static void task(void * pvParameters) {
    ((LTE*) pvParameters)->task();
  }

  /** This task handling the whole LTE state machine, here is where different activities are scheduled 
   *  and where the code decides what actions to perform.  
   */
  void task(void) {
    if (!lteDetect()) {
      log_w("LARA-R6/SARA-R5/LENA-R8 not detected, check wiring");
    } else {
      setState(CHECKSIM);  
    }
    while(true) {
      if ((PIN_INVALID != LTE_ON) && (state != INIT)) {
        // detect if LTE was turned off
        if (LTE_ON_ACTIVE != digitalRead(LTE_ON)) {
          UbxSerial.end();
          setState(INIT, LTE_DETECT_RETRY);
        }
      }
        
      if (state != INIT) {
        SARA_R5::poll();
      }
      
      int32_t now = millis();
      if (0 >= (ttagNextTry - now)) {
        ttagNextTry = now + LTE_1S_RETRY;
        String id = MQTT_CLIENT_ID;
        bool useMqtt  = true;
        switch (state) {
          case INIT:
            ttagNextTry = now + LTE_DETECT_RETRY;
            if (lteDetect()) {
              setState(CHECKSIM);
            } 
            break;    
          case CHECKSIM:
            ttagNextTry = now + LTE_CHECKSIM_RETRY;
            if (lteInit()) {
              setState(WAITREGISTER);
            }
            break;
          case WAITREGISTER:
            if (lteRegistered()) {
              setState(REGISTERED);
            }
            break;
          case REGISTERED:
            ttagNextTry = now + LTE_ACTIVATION_RETRY;
            if (lteActivate()) {
              setState(ONLINE);
            }
            break;
          case ONLINE:
            if (useMqtt) {
              {
                ttagNextTry = now + LTE_CONNECT_RETRY;
                mqttConnect(id); // callback will advance the state
              }
            }
            break;
          case MQTT:
            if (!useMqtt || (0 == id.length())) {
              if (mqttStop()) {
                setState(ONLINE);
              }
            } else {
              mqttTask();
            }
            break;
          default:
            setState(INIT);
            break;
        }
      }
      vTaskDelay(30);
    }
  }

  // -----------------------------------------------------------------------
  // HARDWARE 
  // -----------------------------------------------------------------------
  
  /** The modem is connected using different GPIOs and full 8 wire Serial port.    
   *  this function makes sure that at boot all the pins are properly configured.
   *  The pins are defined in the HW.h file and depend on the board compiled for. 
   */
  void hwInit(void) {
    // The current SARA-R5 library is setting the LTE_RESET and LTE_PWR_ON pins to input after using them, 
    // This is not ideal for the v0.8/v0.9 hardware. Therefore it is prefered to control the LTE_RESET 
    // and LTE_PWR_ON externally in our code here. We also like to have better control of the pins to 
    // support different variants of LTE modems.
         
    // Module     Power On time   Power Off time   Reset
    // SARA-R5    0.1/1-2s        23s + 1.5sReset  0.1s
    // LARA-R6    0.15-3.2s       >1.5s            0.05-6s (10s emergency)
    // LENA-R8    >2s             >3.1s            0.05s
    
    // The LTE_RESET pin is active LOW, HIGH = idle, LOW = in reset
    // The LTE_RESET pin unfortunately does not have a pull up resistor on the v0.8/v0.9 hardware
    if (PIN_INVALID != LTE_RESET) {
      digitalWrite(LTE_RESET, HIGH);
      pinMode(LTE_RESET, OUTPUT);
      digitalWrite(LTE_RESET, HIGH);
    }
    // The LTE_PWR_ON pin is usually active HIGH, LOW = idle, defined by LTE_PWR_ON_ACTIVE, active timings see table above
    // The LTE_PWR_ON pin has a external pull low resistor on the board. 
    if (PIN_INVALID != LTE_PWR_ON) {
      digitalWrite(LTE_PWR_ON, !LTE_PWR_ON_ACTIVE);
      pinMode(LTE_PWR_ON, OUTPUT);
      digitalWrite(LTE_PWR_ON, !LTE_PWR_ON_ACTIVE);
    }
    if (PIN_INVALID != LTE_TXI) {
      digitalWrite(LTE_TXI, HIGH);
      pinMode(LTE_TXI, OUTPUT);
      digitalWrite(LTE_TXI, HIGH);
    }
    if (PIN_INVALID != LTE_RTS) {
      digitalWrite(LTE_RTS, LOW);
      pinMode(LTE_RTS, OUTPUT);
      digitalWrite(LTE_RTS, LOW);
    }
    if (PIN_INVALID != LTE_DTR) {
      digitalWrite(LTE_DTR, LOW);
      pinMode(LTE_DTR, OUTPUT);
      digitalWrite(LTE_DTR, LOW);
    } 
    // init all other pins here
    if (PIN_INVALID != LTE_ON)  pinMode(LTE_ON,  INPUT);
    if (PIN_INVALID != LTE_RXO) pinMode(LTE_RXO, INPUT);
    if (PIN_INVALID != LTE_CTS) pinMode(LTE_CTS, INPUT);
    if (PIN_INVALID != LTE_DSR) pinMode(LTE_DSR, INPUT);
    if (PIN_INVALID != LTE_DCD) pinMode(LTE_DCD, INPUT); 
    if (PIN_INVALID != LTE_RI)  pinMode(LTE_RI,  INPUT);
    if (PIN_INVALID != LTE_INT) pinMode(LTE_INT, INPUT);
  }

  /** The modem is started using a PWR_ON pin. This process takes quite some time and is a 
   *  critical phase. Duing the process it is a bad idea to try to talk to the modem too early.
   *  \return  true if hardware is ready. 
   */
  bool hwReady(void) {
    // turn on the module 
    #define DETECT_DELAY 100
    int pwrOnTime = -1; // will never trigger
    if (PIN_INVALID != LTE_PWR_ON) {
      if ((PIN_INVALID != LTE_ON) ? LTE_ON_ACTIVE != digitalRead(LTE_ON) : true) {
        log_i("LTE power on");
        digitalWrite(LTE_PWR_ON, LTE_PWR_ON_ACTIVE);
        pwrOnTime = LTE_POWER_ON_PULSE / DETECT_DELAY;
      }
    }
    bool ready = true;
    char lastCts = -1;
    char lastOn = -1;
    char lastRxo = -1;
    for (int i = 0; i < LTE_POWER_ON_WAITTIME_MAX / DETECT_DELAY; i ++) { 
      ready = (pwrOnTime < 0);
      if (i == pwrOnTime) {
        digitalWrite(LTE_PWR_ON, !LTE_PWR_ON_ACTIVE);
        log_d("LTE pin PWR_ON off(idle)"); 
        pwrOnTime = -1; // no more
        i = 0; // restart timer
      }
      if (PIN_INVALID != LTE_RXO) {
        char rxo = digitalRead(LTE_RXO);
        if (lastRxo != rxo) {
          log_d("LTE pin RXO %s", (rxo == LOW) ? "LOW(active)" : "HIGH(idle)"); 
          lastRxo = rxo;
        }
        ready = ready && (rxo == HIGH);
      }
      if (PIN_INVALID != LTE_ON) {
        char on = digitalRead(LTE_ON);
        if (on != lastOn) {
          log_d("LTE pin ON %s", (on == LTE_ON_ACTIVE) ? "on(active)" : "off(idle)"); 
          lastOn = on;
        }
        ready = ready && (on == LTE_ON_ACTIVE);
      }
      if (PIN_INVALID != LTE_CTS) {
        char cts = digitalRead(LTE_CTS);
        if (lastCts != cts) {
          log_d("LTE pin CTS %s", (cts == LOW) ? "LOW(idle)" : "HIGH(wait)"); 
          lastCts = cts;
        }
        ready = ready && (cts == LOW);
      }
      if (ready && (i > LTE_POWER_ON_WAITTIME / DETECT_DELAY)) {
        break;
      }  
      delay(DETECT_DELAY);
    }
    if (ready) {
      log_i("LTE ready");
    } else {
      log_w("not ready RXO PWRON CTS : %d %d %d != 1 0 0", lastRxo, lastOn, lastCts); 
    }
    if (ready) {
      #define PIN_TXT(pin) (PIN_INVALID == pin) ? "" : digitalRead(pin) == LOW ? " LOW" : " HIGH"
      log_d("baudrate %d pins RXo %d%s TXi %d%s CTSo %d%s RTSi %d%s", LTE_BAUDRATE, 
        LTE_RXO, PIN_TXT(LTE_RXO), LTE_TXI, PIN_TXT(LTE_TXI),
        LTE_CTS, PIN_TXT(LTE_CTS), LTE_RTS, PIN_TXT(LTE_RTS));
      ready = begin(UbxSerial, LTE_BAUDRATE);
    }
    return ready;
  }
  
  /** We need to sub class the Sparkfun class, and override this function as we are 
   *  using a four wire serial port with flow control that needs to be preserved.
   *  \param baud  the desired baud rate
   */
  void beginSerial(unsigned long baud) override
  {
    delay(100);
    UbxSerial.end();
    delay(10);
    log_d("LTE baudrate %d pins RXo %d TXi %d CTSo %d RTSi %d", baud, LTE_RXO, LTE_TXI, LTE_CTS, LTE_RTS);
    UbxSerial.begin(baud, SERIAL_8N1, LTE_RXO, LTE_TXI);
    if ((PIN_INVALID != LTE_RTS) && (PIN_INVALID != LTE_CTS)) {
      UbxSerial.setPins(LTE_RXO, LTE_TXI, LTE_CTS, LTE_RTS); 
      UbxSerial.setHwFlowCtrlMode(HW_FLOWCTRL_CTS_RTS, 64);
    }
    delay(100);
  }
};
    
LTE Lte; //!< The global LTE peripherial object

#endif // __LTE_H__