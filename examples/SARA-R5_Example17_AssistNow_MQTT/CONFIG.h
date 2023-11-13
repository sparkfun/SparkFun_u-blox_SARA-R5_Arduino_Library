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
 
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <vector>
#include <mbedtls/base64.h>

#include "HW.h"
#include "secrets.h"

// -----------------------------------------------------------------------
// MQTT / PointPerfect settings 
// -----------------------------------------------------------------------

const char AWS_IOT_ENDPOINT[]             = "pp.services.u-blox.com";
const unsigned short MQTT_BROKER_PORT     =              8883;  //!< MQTTS port
const int MQTT_MAX_MSG_SIZE               =            9*1024;  //!< the max size of a MQTT pointperfect topic

#define MQTT_TOPIC_MGA                          "/pp/ubx/mga"   //!< GNSS assistance topic 

const char MQTT_TOPIC_MGA_GPS[]       = MQTT_TOPIC_MGA "/gps";  //!< GPS (US)
const char MQTT_TOPIC_MGA_GLO[]       = MQTT_TOPIC_MGA "/glo";  //!< Glonass (RU)
const char MQTT_TOPIC_MGA_GAL[]       = MQTT_TOPIC_MGA "/gal";  //!< Galileo (EU)
const char MQTT_TOPIC_MGA_BDS[]       = MQTT_TOPIC_MGA "/bds";  //!< Beidou (CN)

// -----------------------------------------------------------------------
// CONFIGURATION keys
// -----------------------------------------------------------------------

#define    CONFIG_DEVICE_TITLE                 "HPG solution"   //!< a used friendly name
#define    CONFIG_DEVICE_NAMEPREFIX                     "hpg"   //!< a hostname compatible prefix, only a-z, 0-9 and -

/** This class encapsulates all WLAN functions. 
*/
class CONFIG {
  
public:

  /** constructor
   */
  CONFIG() {
    // create a unique name from the mac 
    uint64_t mac = ESP.getEfuseMac();
    const char* p = (const char*)&mac;
    char str[64];
    sprintf(str, CONFIG_DEVICE_TITLE " - %02x%02x%02x", p[3], p[4], p[5]);
    title = str;
    sprintf(str, CONFIG_DEVICE_NAMEPREFIX "-%02x%02x%02x", p[3], p[4], p[5]);
    name = str;
  }

  /** get a name of the device
   *  \return the device name 
   */
  String getDeviceName(void)  { 
    return name;
  }    
  
  /** get a friendly name of the device
   *  \return the friendly device title 
   */
  String getDeviceTitle(void) { 
    return title; 
  } 
  
  /** get the topics to subscribe  
   *  \return  a vector with all the topics
   */
  std::vector<String> getTopics(void)
  { 
    std::vector<String> topics;
    topics.push_back(MQTT_TOPIC_MGA);
    //topics.push_back(MQTT_TOPIC_MGA_GPS);
    //topics.push_back(MQTT_TOPIC_MGA_GLO);
    //topics.push_back(MQTT_TOPIC_MGA_GAL);
    //topics.push_back(MQTT_TOPIC_MGA_BDS);
    return topics;
  }
  
protected:
  
  String title;               //!< the title of the device
  String name;                //!< the name of the device
};
   
CONFIG Config; //!< The global CONFIG object

#endif // __CONFIG_H__
