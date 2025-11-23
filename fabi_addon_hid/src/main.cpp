
/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <Arduino.h>
#include <Wire.h>

#include "driver/gpio.h"
#include "usb_hid_host.h"

#define OUTPUT_UNIFIED_HID_DATA
// #define DEBUG_OUTPUT_I2C


// I2C Configuration
#define FABI_I2C_ADDON_ADDR 0x37  // I2C address of generic FABI I2C addon devices

// data structure for FABI I2C transmission

#define FABI_I2C_CMD_GET_CAPS     0xFF    // command to request device capabilities bitmask

#define FABI_I2C_CAP_XY        (1<<0)  // bitmask for 1st data field in FABI generic sensor I2C report: int16_t x / int16_t y  (4 bytes)
#define FABI_I2C_CAP_PRESSURE  (1<<1)  // bitmask for 3rd data field in FABI generic sensor I2C report: int16_t pressure       (2 bytes)
#define FABI_I2C_CAP_BUTTONS   (1<<2)  // bitmask for 4th data field in FABI generic sensor I2C report: uint16_t buttons       (2 bytes)
  
// Define supported capabilities for this device
const uint8_t DEVICE_CAPABILITIES = FABI_I2C_CAP_XY | FABI_I2C_CAP_BUTTONS;
// const uint8_t DEVICE_CAPABILITIES = FABI_I2C_CAP_XY | FABI_I2C_CAP_PRESSURE | FABI_I2C_CAP_BUTTONS;   // mimick FABI addOn with pressure sensor

typedef struct {
  uint8_t reportType;      // first byte contains flags (bitmask) for transferred data fields
  int16_t x;
  int16_t y;
  uint16_t pressure;
  uint16_t button_states;
} i2c_fabi_data_t;


i2c_fabi_data_t fabiI2CPacket = { DEVICE_CAPABILITIES , 0, 0, 0, 0};

// Global to store the currently requested data mask (default to all)
uint8_t requested_report_mask = DEVICE_CAPABILITIES;
bool is_caps_request = false;

/**
 * @brief Update I2C mouse data from USB hid data report
 */
void update_hidData (unified_hidData_t *hidData) {

  #ifdef OUTPUT_UNIFIED_HID_DATA
      printf("X: %06d\tY: %06d\tscroll: %02d\tbuttons:|%c|%c|%c|\n",
          hidData->x_displacement,
          hidData->y_displacement,
          hidData->scroll_wheel,
          (hidData->buttons.button1 ? 'L' : ' '),
          (hidData->buttons.button3 ? 'M' : ' '),
          (hidData->buttons.button2 ? 'R' : ' ')
          );
      fflush(stdout);
  #endif

  // Accumulate movements if previous data hasn't been read
  fabiI2CPacket.x += hidData->x_displacement;
  fabiI2CPacket.y += hidData->y_displacement;

  // update pressure value and button states
  // fabiI2CPacket.pressure = hidData->pressure;

  fabiI2CPacket.button_states &= 0xfff8; 
  if (hidData->buttons.button1) fabiI2CPacket.button_states |= 0x01; // Left
  if (hidData->buttons.button2) fabiI2CPacket.button_states |= 0x02; // Right  
  if (hidData->buttons.button3) fabiI2CPacket.button_states |= 0x04; // Middle

  if (hidData->scroll_wheel>0) fabiI2CPacket.button_states |= 0x08; // Scroll Up
  if (hidData->scroll_wheel<0) fabiI2CPacket.button_states |= 0x10; // Scroll Down

}

/**
 * @brief I2C slave request callback - sends fabi packet based on requested mask
 */
void onI2CRequest() {
  if (is_caps_request) {
    // Master requested capabilities
    Wire.write(DEVICE_CAPABILITIES);
    return;
  }

  // Send the Report Type Header (echo the mask so master knows format)
  #ifdef DEBUG_OUTPUT_I2C
    Serial.printf("sending header: 0x%02X\n", requested_report_mask);
  #endif
  Wire.write(requested_report_mask);

  // Send requested data fields
  if (requested_report_mask & FABI_I2C_CAP_XY) {
    Wire.write((uint8_t*)&fabiI2CPacket.x, sizeof(fabiI2CPacket.x));
    Wire.write((uint8_t*)&fabiI2CPacket.y, sizeof(fabiI2CPacket.y));

    // Reset accumulated x and y 
    fabiI2CPacket.x = 0; 
    fabiI2CPacket.y = 0; 
  }

  if (requested_report_mask & FABI_I2C_CAP_PRESSURE) {
    Wire.write((uint8_t*)&fabiI2CPacket.pressure, sizeof(fabiI2CPacket.pressure));
  }

  if (requested_report_mask & FABI_I2C_CAP_BUTTONS) {
    Wire.write((uint8_t*)&fabiI2CPacket.button_states, sizeof(fabiI2CPacket.button_states));
    // Clear scroll bits (0x08 = Up, 0x10 = Down) only if read
    fabiI2CPacket.button_states &= ~(0x08 | 0x10);
  }
} 


/**
 * @brief I2C slave receive callback - updates the requested data mask
 */
void onI2CReceive(int numBytes) {
  if (Wire.available() > 0) {
    // The master sends a command byte: either a request mask or the GET_CAPS command
    uint8_t cmd = Wire.read();
    
    if (cmd == FABI_I2C_CMD_GET_CAPS) {
      is_caps_request = true;
      #ifdef DEBUG_OUTPUT_I2C
      Serial.println("I2C Master requested CAPABILITIES");
      #endif
    } else {
      is_caps_request = false;
      // Only allow requesting supported capabilities
      requested_report_mask = cmd & DEVICE_CAPABILITIES;
      #ifdef DEBUG_OUTPUT_I2C
        Serial.printf("I2C Master requested report mask: 0x%02X\n", requested_report_mask);
      #endif
    }
    
    // Discard any extra bytes
    while (Wire.available()) {
      Wire.read();
    }
  }
}


/**
 * @brief Initialize I2C slave interface
 */
void init_i2c_slave() {
  // Initialize I2C as slave
  // Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.begin(FABI_I2C_ADDON_ADDR);
  Wire.setClock(400000);  // use 400kHz I2C clock

  // Set callback functions
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

  #ifdef DEBUG_OUTPUT_I2C
    Serial.printf("I2C slave initialized at address 0x%02X", FABI_I2C_SLAVE_ADDR);
  #endif
}


void setup() { 
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    init_i2c_slave();

    // register mouse report callback handler
    register_hidData_callback(update_hidData);

    //start main USB/HID task
    start_usb_host(); 
}

void loop() {
  /*
  //  demo: mimick pressure value updates 
  static uint16_t pressure = 0;
  delay(500);
  pressure += 10;
  if (pressure > 1023) pressure = 0;
  fabiI2CPacket.pressure = pressure;
  Serial.printf("Pressure updated to %d\n", fabiI2CPacket.pressure);
  */
}