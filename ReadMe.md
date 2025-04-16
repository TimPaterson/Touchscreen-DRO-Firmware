# Firmware for Smart Touchscreen DRO Project
### Latest Updates - Release v.9
- Supports use with a lathe by changing a selection in Settings. Lathe tool library is not yet implemented.
- Touch calibration expanded to 5 points, which improves overall accuracy.
- Will self-start on first-time startup without requiring a serial connection.

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

 ### Initial Start-Up
 Once the MCU is programmed and connected to a touchscreen, it will automatically go into touch
 calibration. When calibration is complete, a flash drive with a firmware update file needs to
 be connected to its USB port. The DRO will allow you to browse the flash drive for the update file, and 
 select it for programming. This will load all the necessary graphics and fonts, and the DRO
 will be ready to go!

 ### Additional Tools
 The following additional tools were used to create the screen images and fonts, available on GitHub:
 - Images (`Screen.bin`): [ScreenDesigner-for-touchscreens](https://github.com/TimPaterson/ScreenDesigner-for-touchscreens)
 - Fonts (`Fonts.bin`): [FontGenerator-embedded-systems](https://github.com/TimPaterson/FontGenerator-embedded-systems)
