# Firmware for Smart Touchscreen DRO Project
This is the repository for the code for the [Smart Touchscreen DRO](https://github.com/TimPaterson/TouchscreenDigitalReadout).
Follow the link the to [base repository](https://github.com/TimPaterson/TouchscreenDigitalReadout) for description, photos, etc.

This repository contains two projects:
- The code for Microchip SAM D21 MCU that runs the DRO. This is a Microchip Studio project.
- DroUpdateBuilder, a Windows app that packages DRO code, graphics, and fonts into a single file for updates.
 This is a Visual Studio project. The `bin` folder includes a ready-to-run `DroUpdateBuilder.exe`.

 To build the DRO code, you must also have the [Microchip-SAM-Library](https://github.com/TimPaterson/Microchip-SAM-Library)
 repository in a sibling directory.
