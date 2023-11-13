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
 
#ifndef __UBXFILE_H__
#define __UBXFILE_H__

#include <cbuf.h> 

const int UBXSERIAL_BUFFER_SIZE   =      0*1024;  //!< Size of circular buffer, typically AT modem gets bursts upto 9kB of MQTT data, but 2kB is also fine

/** older versions of ESP32_Arduino do not yet support flow control, but we need this for the modem. 
 *  The following flag will make sure code is added for this,
 */
#include "driver/uart.h" // for flow control
#if !defined(HW_FLOWCTRL_CTS_RTS) || !defined(ESP_ARDUINO_VERSION) || !defined(ESP_ARDUINO_VERSION_VAL)
 #define UBXSERIAL_OVERRIDE_FLOWCONTROL  
#elif (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(2, 0, 3))
 #define UBXSERIAL_OVERRIDE_FLOWCONTROL
#endif

/** This class encapsulates all UBXSERIAL functions. the class can be used as alternative to a 
 *  normal Serial port, but add full RX and TX logging capability. 
*/
class UBXSERIAL : public HardwareSerial {
public:

  /** constructor
   *  \param size      the circular buffer size
   *  \param uart_num  the hardware uart number
   */
  UBXSERIAL(size_t size, uint8_t uart_num) 
        : HardwareSerial{uart_num}, buffer{size} {
    mutex = xSemaphoreCreateMutex(); // Previously, this was inherited from class UBXFILE
    setRxBufferSize(256);
  }

  // --------------------------------------------------------------------------------------
  // STREAM interface: https://github.com/arduino/ArduinoCore-API/blob/master/api/Stream.h
  // --------------------------------------------------------------------------------------
 
  /** The character written is also passed into the circular buffer
   *  \param ch  character to write
   *  \return    the bytes written
   */ 
  size_t write(uint8_t ch) override {
    if (pdTRUE == xSemaphoreTake(mutex, portMAX_DELAY)) {
      if (buffer.size() > 1) { 
        buffer.write((const char*)&ch, 1);
      }
      xSemaphoreGive(mutex);
    }
    return HardwareSerial::write(ch);
  }
  
  /** All data written is also passed into the circular buffer
   *  \param ptr   pointer to buffer to write
   *  \param size  number of bytes in ptr to write
   *  \return      the bytes written
   */ 
  size_t write(const uint8_t *ptr, size_t size) override {
    if (pdTRUE == xSemaphoreTake(mutex, portMAX_DELAY)) {
      if (buffer.size() > 1) { 
        buffer.write((const char*)ptr, size);
      } 
      xSemaphoreGive(mutex);
    }
    return HardwareSerial::write(ptr, size);  
  }

  /** The character read is also passed in also passed into the circular buffer.
   *  \return  the character read
   */ 
  int read(void) override {
    int ch = HardwareSerial::read();
    if (-1 != ch) {
      if (pdTRUE == xSemaphoreTake(mutex, portMAX_DELAY)) {
        if (buffer.size() > 1) { 
          buffer.write((const char*)&ch, 1);
        } 
        xSemaphoreGive(mutex);
      }
    }
    return ch;
  }
  
#ifdef UBXSERIAL_OVERRIDE_FLOWCONTROL
  // The arduino_esp32 core has a bug that some pins are swapped in the setPins function. 
  // PR https://github.com/espressif/arduino-esp32/pull/6816#pullrequestreview-987757446 was issued
  // We will override as we cannot rely on that bugfix being applied in the users environment. 

  // extend the flow control API while on older arduino_ESP32 revisions
  // we keep the API forward compatible so that when the new platform is released it just works
  void setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin) {
    uart_set_pin((uart_port_t)_uart_nr, txPin, rxPin, rtsPin, ctsPin);
  }
  
  void setHwFlowCtrlMode(uint8_t mode, uint8_t threshold) {
    uart_set_hw_flow_ctrl((uart_port_t)_uart_nr, (uart_hw_flowcontrol_t) mode, threshold);
  }
  
 #ifndef HW_FLOWCTRL_CTS_RTS
  #define HW_FLOWCTRL_CTS_RTS UART_HW_FLOWCTRL_CTS_RTS
 #endif
#endif

protected:
  
  SemaphoreHandle_t mutex;  //!< protects cbuf from concurnet access by tasks. 
  cbuf buffer;              //!< the circular local buffer
};

UBXSERIAL UbxSerial(UBXSERIAL_BUFFER_SIZE, UART_NUM_1); //!< The global UBXSERIAL peripherial object (replaces Serial1)

#endif // __UBXFILE_H__
