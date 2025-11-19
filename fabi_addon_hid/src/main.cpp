
/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <Arduino.h>
#include <Wire.h>

#include "driver/gpio.h"
#include "usb_hid_host.h"

#define OUTPUT_UNIFIED_HID_DATA

// I2C Configuration
#define FABI_I2C_SLAVE_ADDR 0xFA

// data structure for FABI I2C transmission

#define FABI_I2C_REPORT_X         (1<<0)  // bitmask for 1st data field in FABI generic sensor I2C report: int16_t x        (2 bytes)
#define FABI_I2C_REPORT_Y         (1<<1)  // bitmask for 2nd data field in FABI generic sensor I2C report: int16_t y        (2 bytes)
#define FABI_I2C_REPORT_PRESSURE  (1<<2)  // bitmask for 3rd data field in FABI generic sensor I2C report: int16_t pressure (2 bytes)
#define FABI_I2C_REPORT_BUTTONS   (1<<3)  // bitmask for 4th data field in FABI generic sensor I2C report: uint8_t buttons  (1 byte)

typedef struct {
  uint8_t reportType;      // first byte contains flags (bitmask) for transferred data fields
  int16_t x;
  int16_t y;
  uint16_t pressure;
  uint8_t button_states;
} i2c_fabi_data_t;


i2c_fabi_data_t fabiI2CPacket = { FABI_I2C_REPORT_X | FABI_I2C_REPORT_Y | FABI_I2C_REPORT_BUTTONS , 0, 0, 0, 0};


/**
 * @brief Update I2C mouse data from USB hid data report
 */
void update_hidData (unified_hidData_t *hidData) {

  /*
  // Calculate absolute position from displacement
  static int16_t x_pos = 0;
  static int16_t y_pos = 0;
  x_pos += hidData->x_displacement;
  y_pos += hidData->y_displacement;
  */ 

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

  fabiI2CPacket.button_states &= 0xf8; 
  if (hidData->buttons.button1) fabiI2CPacket.button_states |= 0x01; // Left
  if (hidData->buttons.button2) fabiI2CPacket.button_states |= 0x02; // Right  
  if (hidData->buttons.button3) fabiI2CPacket.button_states |= 0x04; // Middle

  if (hidData->scroll_wheel>0) fabiI2CPacket.button_states |= 0x08; // Scroll Up
  if (hidData->scroll_wheel<0) fabiI2CPacket.button_states |= 0x10; // Scroll Down

}


/**
 * @brief I2C slave request callback - sends fabi packet when master requests
 */
void onI2CRequest() {

   // Send the I2C data packet, first byte indicates which data fields are valid
  // Wire.write((uint8_t*)&fabiI2CPacket, sizeof(fabiI2CPacket));
  
  Wire.write((uint8_t*)&fabiI2CPacket.reportType, sizeof(fabiI2CPacket.reportType));  
  Wire.write((uint8_t*)&fabiI2CPacket.x, sizeof(fabiI2CPacket.x));  
  Wire.write((uint8_t*)&fabiI2CPacket.y, sizeof(fabiI2CPacket.y));
  Wire.write((uint8_t*)&fabiI2CPacket.pressure, sizeof(fabiI2CPacket.pressure));
  Wire.write(&fabiI2CPacket.button_states, sizeof(fabiI2CPacket.button_states));

  // Reset accumulated movement data after sending 
  fabiI2CPacket.x = 0;
  fabiI2CPacket.y = 0;
  fabiI2CPacket.button_states &= ~0x08; // clear scroll up
  fabiI2CPacket.button_states &= ~0x10; // clear scroll down
}

/**
 * @brief I2C slave receive callback (not used in this application)
 */
void onI2CReceive(int numBytes) {
  // Clear receive buffer (not used for mouse data)
  while (Wire.available()) {
    Wire.read();
  }
}

/**
 * @brief Initialize I2C slave interface
 */
void init_i2c_slave() {
  // Initialize I2C as slave
  // Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.begin(FABI_I2C_SLAVE_ADDR);
  Wire.setClock(400000);  // use 400kHz I2C clock

  // Set callback functions
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);
  
  printf("I2C slave initialized at address 0x%02X", FABI_I2C_SLAVE_ADDR);
  
}


void setup() { 
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    init_i2c_slave();

    // register mouse report callback handler
    register_hidData_callback(update_hidData);

    //start main USB/HID task
    start_usb_host(); }

void loop() {
  delay(1000);
  Serial.print("*");
}