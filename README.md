# FABI-addons
This repository contains various external addons which can be connected to a FABI3.


## ext_general_adapter

This general adapter PCB can be used to connect various devices to the FABI3 (_highlighted internal signal name_)

* 2 external buttons via jackplugs (_EXT1/EXT2_)
* 2 buttons via JST PH2.0 connectors (_EXT1/EXT2_)
* OLED via I2C (_SDA/SCL_)
* pressure sensors with the PCB edge connector, e.g. DPS310/MPRLS (_SDA/SCL_)
* force sensor with NAU7802 (red Würth connector) (_SDA/SCL/EXT1 as IRQ_)


## fabi_addon_hid

An adapter for connecting USB HID devices to the RJ25 (Ext) connector of the FABI. 
This allows using standard USB mice, gamepads or joysticks as Bluetooth mouse device, change sensitivity profiles and much more.


## fabi_addon_joystick

External analog joystick, using a [JH-D202X-R4 Joystick module](https://protosupplies.com/product/jh-d202x-r2-r4-joystick-5k/). It is possible to use this addon with or without a 0.96" OLED (SSD1306, the same as in the FABI3).
