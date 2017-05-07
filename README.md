# i3Extra

This is a modified version of the Marlin 1.1.0-RC8 firmware for the WANHAO Duplicator i3 Plus 3d printer.
Currently it's in alpha state, same lcd functions are working and some of them is not.

Changelog Marlin:
- Added the printer specific pin and machine configuration files
- Switched to the Arduino serial port lib to add a second serial for the lcd
- Added the completely rewritten lcd communication functions
- Added a serial bridge mode for lcd firmware update over the printers usb port

Changelog LCD:
- Removed the chines language
- Renamed all the bitmaps to fix the firmware update problem caused by the chines characters
- Removed some start animation frames
- Added the lcd update bridge mode button to the system menu
- Added Marlin and a LCD firmware version display functions to the system menu 
- Changed the bed leveling menu to a usable version
- Swapped and rearranged some buttons in the system/tool menus

If you like to help us to make a better software for the printer don't hesitate to contribute in this project!
