/*

  SARA-R5 Example
  ===============

  Assist Now via MQTT

  This example demonstrates how to request u-blox PointPerfect AssistNow Online data from Thingstream
  using MQTT, and push it to the SARA-R510M8S internal GNSS. It also demonstrates how quickly the internal
  GNSS can achieve a 3D fix from a cold start with help from AssistNow.

  AssistNow Online data is available via HTTP GET using a PointPerfect AssistNow Token. See Example12 for
  more details. But here we are using MQTT, using the SARA-R5's built-in MQTT commands. This means we need
  a Client Certificate and Client Key, plus the Amazon root Server Certificate to be able to connect and
  subscribe to the /pp/ubx/mga AssistNow topic. See:
  https://developer.thingstream.io/guides/location-services/pointperfect-getting-started
  Specifically:
  https://developer.thingstream.io/guides/location-services/pointperfect-getting-started#h.vn9iuugch1ah
  A free PointPerfect Developer plan will provide capped access to the IP stream - and allow you to
  download the MQTT credentials:
  https://portal.thingstream.io/pricing/laas/laaspointperfect

  Log in to ThingStream, create a Location Thing, select the PointPerfect Developer plan, activate the
  Location Thing, navigate to the Credentials tab, download the Client Key, Client Certificate and
  Amazon Root Certificate, paste all three into secrets.h.

  The PDP profile is read from NVM. Please make sure you have run examples 4 & 7 previously to set up the profile.

  This example is written for the SparkFun Asset Tracker Carrier Board with ESP32 MicroMod Processor Board:
  https://www.sparkfun.com/products/17272
  https://www.sparkfun.com/products/16781

  This example contains a stripped down version of Michael Ammann (@mazgch)'s HPG Solution:
  https://github.com/mazgch/hpg
  Specifically, LTE.h:
  https://github.com/mazgch/hpg/blob/main/software/LTE.h

  We have written this example for the ESP32 Processor Board because Michael's code: is written using
  ESP32 mbed tasks; and uses ESP32 log_ for diagnostic messages, instead of Serial.print.
  This means you can limit the messages to Error, Warn, Debug, Info by selecting the appropriate
  Core Debug Level in the board options.

  **************************************************************************************************
  * Important Note:                                                                                *
  *                                                                                                *
  * This example pulls kBytes of correction data from Thingstream.                                 *
  * Depending on your location and service provider, the data rate may exceed the allowable        *
  * rates for LTE-M or NB-IoT.                                                                     *
  * Worst case, your service provider may throttle or block the connection - now or in the future. *
  **************************************************************************************************
  
  Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Adapted by: Paul Clark
  Date: November 10th 2023

  Licence:

  The code written by Michael Ammann is:
 
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

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <SparkFun_u-blox_SARA-R5_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_u-blox_SARA-R5_Arduino_Library

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Michael Ammann (@mazgch)'s HPG Solution:
// https://github.com/mazgch/hpg

#include "HW.h"
#include "CONFIG.h"
#include "UBXFILE.h"
#include "LTE.h"

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void setup()
{
  
  delay(1000); // Wait for the ESP32 to start up

  Serial.begin(115200); // Start the serial console

  Serial.println(F("SARA-R5 Example"));

  log_i("-------------------------------------------------------------------");

  String hwName = Config.getDeviceName();
  log_i("SARA-R5_Example17_AssistNow_MQTT %s (%s)", Config.getDeviceTitle().c_str(), hwName.c_str());  
  espVersion();

  //Lte.enableDebugging(UbxSerial);
  //Lte.enableAtDebugging(Serial); // we use UbxSerial for data logging instead
  Lte.init();  // LTE runs in a task
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void loop()
{
  // Nothing to do here! Everything is performed by tasks...
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// ====================================================================================
// Helpers
// ====================================================================================

#ifndef ESP_ARDUINO_VERSION
  #include <core_version.h>
#endif

/** Print the version number of the Arduino and ESP core. 
 */
void espVersion(void) {
#ifndef ESP_ARDUINO_VERSION
  log_i("Version IDF %s Arduino_esp32 %s", esp_get_idf_version(), ARDUINO_ESP32_RELEASE);
#else
  log_i("Version IDF %s Arduino_esp32 %d.%d.%d", esp_get_idf_version(),
        ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
#endif
}
