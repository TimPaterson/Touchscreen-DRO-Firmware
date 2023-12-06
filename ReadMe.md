# Firmware for Smart Touchscreen DRO Project
### Latest Updates - Release v.5
- The LT7683 graphics chip is now supported. 
[BuyDisplay.com](https://www.buydisplay.com/serial-spi-i2c-10-1-inch-tft-lcd-module-dislay-w-ra8876-optl-touch-panel)
is using this chip in place of the RA8876.
- The capacitive touch panel (CTP) is now supported (as well as the original resistive touch panel). The 
CTP costs only slightly more and gives more precise results, unaffected by touch pressure. Like a phone
touchscreen, it will *not* respond to an inanimate object (e.g., a pen).

### Introduction
This is the repository for the code for the [Smart Touchscreen DRO](https://github.com/TimPaterson/TouchscreenDigitalReadout).
Go there for description, photos, etc.

This repository contains two projects:
- The code for Microchip SAM D21 MCU that runs the DRO. This is a Microchip Studio project.
- DroUpdateBuilder, a Windows console app that packages DRO code, graphics, and fonts into a single file for updates.
 This is a Visual Studio project. The `bin` folder includes a ready-to-run `DroUpdateBuilder.exe`.

 The release includes ready-to-go binary files. If you wish to build the DRO code yourself, you must also have the 
 [Microchip-SAM-Library](https://github.com/TimPaterson/Microchip-SAM-Library)
 repository in a sibling directory.

 ### Additional Tools
 The following additional tools were used to create the screen images and fonts, available on GitHub:
 - Images (`Screen.bin`): [ScreenDesigner-for-touchscreens](https://github.com/TimPaterson/ScreenDesigner-for-touchscreens)
 - Fonts (`Fonts.bin`): [FontGenerator-embedded-systems](https://github.com/TimPaterson/FontGenerator-embedded-systems)

 Initial start-up of the DRO requires a serial connection to download the graphics and fonts.
 The Windows app `ComTest.exe` has been included in the `bin` folder and in the Release for that purpose.
