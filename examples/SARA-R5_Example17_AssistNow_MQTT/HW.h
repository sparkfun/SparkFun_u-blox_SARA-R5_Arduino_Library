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
 
#ifndef __HW_H__
#define __HW_H__

/** The target is used to enable conditional code thoughout this application 
 */

#define SPARKFUN_MICROMOD_ASSET_TRACKER     41 //!< Choose Sparkfun ESP32 Arduino / Sparkfun ESP32 MicroMod

#define SPARKFUN_RTK_EVERYWHERE             51 //!< Select ESP32 / ESP32 Wrover Module

#define HW_TARGET SPARKFUN_MICROMOD_ASSET_TRACKER

/** the pins are defined here for each hardware target 
 */
enum HW_PINS {  
    // Standard pins
    BOOT        =  0, 
    CDC_RX      = RX,  CDC_TX         = TX,
#if (HW_TARGET == SPARKFUN_RTK_EVERYWHERE)
    LED         =  2,
    CAN_RX      = -1,  CAN_TX         = -1,
    I2C_SDA     = 21,  I2C_SCL        = 22,
#elif (HW_TARGET == SPARKFUN_MICROMOD_ASSET_TRACKER)
    LED         =  2,
    CAN_RX      = -1,  CAN_TX         = -1,
    I2C_SDA     = 21,  I2C_SCL        = 22,
#else
  #error unknown board target
#endif

#if (HW_TARGET == SPARKFUN_MICROMOD_ASSET_TRACKER)
    // LTE (DCE)
    LTE_RESET   = -1,  LTE_PWR_ON     = G2,  LTE_ON      = G6,  LTE_INT = G5, 
    LTE_TXI    = TX1,  LTE_RXO       = RX1,  LTE_RTS     = -1,  LTE_CTS = -1, 
    LTE_RI      = G4,  LTE_DSR        = -1,  LTE_DCD     = -1,  LTE_DTR = -1,
    LTE_PWR_ON_ACTIVE = HIGH, LTE_ON_ACTIVE = LOW,
   
    // Power supply
    VIN         = 39,  V33_EN         = -1, V33_EN_ACTIVE = HIGH,
    
    // Micro SD card
    MICROSD_SCK = SCK, MICROSD_SDI  = MISO, MICROSD_SDO = MOSI, 
    MICROSD_DET = -1,  MICROSD_PWR_EN = G1,  
    MICROSD_CS  = G0,
    MICROSD_DET_REMOVED = HIGH, MICROSD_PWR_EN_ACTIVE = LOW,

    REQUIRED_GPIO_PIN = -1, REQUIRED_GPIO_PIN_ACTIVE = HIGH,

#elif (HW_TARGET == SPARKFUN_RTK_EVERYWHERE)
    // LTE (DCE)
    LTE_RESET   = -1,  LTE_PWR_ON     = 26,  LTE_ON      =  5,  LTE_INT = -1, 
    LTE_TXI     = 13,  LTE_RXO        = 14,  LTE_RTS     = -1,  LTE_CTS = -1, 
    LTE_RI      = -1,  LTE_DSR        = -1,  LTE_DCD     = -1,  LTE_DTR = -1,
    LTE_NI      = 34,
    LTE_PWR_ON_ACTIVE = HIGH, LTE_ON_ACTIVE = HIGH,
   
    // Power supply
    VIN         = -1,  V33_EN         = 32, V33_EN_ACTIVE = HIGH,
    
    // Micro SD card
    MICROSD_SCK = SCK, MICROSD_SDI  = MISO, MICROSD_SDO = MOSI, 
    MICROSD_DET = 36,  MICROSD_PWR_EN = -1,  
    MICROSD_CS  = 4,
    MICROSD_DET_REMOVED = LOW, MICROSD_PWR_EN_ACTIVE = LOW,

    // Required GPIO pin - on SPARKFUN_RTK_EVERYWHERE this is the WizNet W5500 CS
    REQUIRED_GPIO_PIN = 27, REQUIRED_GPIO_PIN_ACTIVE = HIGH,

#endif
    PIN_INVALID = -1
};

class HW {
  
public:

  /** constructor
   */
  HW(){
    hwInit();
  }

  void hwInit(void) {
    // Do any top-level hardware initialization here:
    // Initialize any required GPIO pins
    if (PIN_INVALID != REQUIRED_GPIO_PIN) {
      digitalWrite(REQUIRED_GPIO_PIN, REQUIRED_GPIO_PIN_ACTIVE);
      pinMode(REQUIRED_GPIO_PIN, OUTPUT);
      digitalWrite(REQUIRED_GPIO_PIN, REQUIRED_GPIO_PIN_ACTIVE);
    }
    // Turn on the 3.3V regulator - if present
    if (PIN_INVALID != V33_EN) {
      digitalWrite(V33_EN, V33_EN_ACTIVE);
      pinMode(V33_EN, OUTPUT);
      digitalWrite(V33_EN, V33_EN_ACTIVE);
    }
    log_i("Hardware initialized");
  }

};

HW Hardware; //!< The global HW object

#endif // __HW_H__
