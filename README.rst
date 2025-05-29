
Zephyr-Doom
=======================================================

This is a port of `Doom (1993)`_ for `Zephyr RTOS`_.

The project is forked from `nrf-doom`_ (commit `2d42413`_), which is forked from `Chocolate Doom`_ version 3.0.0. The objective of this internship project is to develop a Zephyr port of the game, making sure it can run on different System-on-Chips (SoCs). This endeavor will be divided into several stages, presented as Minimum Viable Product (MVP) packages.

Zephyr-Doom has only been tested with shareware version of Doom 1.

======================= ================= ================================
 Game                    Status            Comment
======================= ================= ================================
Doom 1 Shareware        OK                
----------------------- ----------------- --------------------------------
Doom 1 Full Version     Not Tested        
----------------------- ----------------- --------------------------------
Doom 2                  Not Tested        
======================= ================= ================================

.. _Chocolate Doom: https://www.chocolate-doom.org/wiki/index.php/Chocolate_Doom
.. _nrf-doom: https://github.com/NordicPlayground/nrf-doom
.. _2d42413: https://github.com/NordicPlayground/nrf-doom/commit/2d42413b2c49cda7c60d3cd14b858df1b665533f

.. _nRF5340: https://www.nordicsemi.com/Products/Low-power-short-range-wireless/nRF5340
.. _Doom (1993): https://en.wikipedia.org/wiki/Doom_(1993_video_game)
.. _Zephyr RTOS: https://zephyrproject.org/

.. _Visual Studio Code: https://code.visualstudio.com/download
.. _nRF Connect for VS Code: https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code/Download#infotabs

.. contents:: Table of Contents
    :depth: 2

Status
-------------------------------------------------------

Backlog
~~~~~~~~~

============================ ================= ================================
 Feature                     Status            Comment
============================ ================= ================================
Game data in QSPI flash      Done
---------------------------- ----------------- --------------------------------
Doom game engine running     Done
---------------------------- ----------------- --------------------------------
Devkit button control        Done
---------------------------- ----------------- --------------------------------
GPIO ILI9486 display support Done              Currently, there is a limitation on the FPS and the picture is rotated by 90 degrees
---------------------------- ----------------- --------------------------------
SPI ILI9341 display support  Done              FPS got significantly decreased due to the multiple hw/sw limitations
---------------------------- ----------------- --------------------------------
SPI FT810 display support    Done              FPS matching `nrf-doom`_ performance
---------------------------- ----------------- --------------------------------
BLE game controller          Done              Transition from the original proprietary radio protocol to BLE using the Nordic UART Service (NUS) for data exchange
---------------------------- ----------------- --------------------------------
BLE HID support              In progress       Replace NUS communication with BLE HID standard to allow control via gamepad (test with Xbox wireless controller) and keyboard (test with BLE keyboard built on RPi 400)
---------------------------- ----------------- --------------------------------
Game data (SD card to flash) In progress       Automatically copy game data from SD card to flash during first start (or on demand)
---------------------------- ----------------- --------------------------------
Game save and load           Open              Enable save and load functionality to store/restore gameplay progress
---------------------------- ----------------- --------------------------------
Pure Zephyr port             Open              Modify the code to make it compatible with any Zephyr device that meets the resource requirements by removing nrfx/ncs elements
---------------------------- ----------------- --------------------------------
Basic CI                     Open              Build the code for target platform(s) in GitHub actions
---------------------------- ----------------- --------------------------------
Static analysis              Open
---------------------------- ----------------- --------------------------------
Simulation on PC             Open              native_posix target
---------------------------- ----------------- --------------------------------
Gamepad calibration          Open
---------------------------- ----------------- --------------------------------
Touchscreen controls         Open              Use dedicated area of display to control movement and fire
---------------------------- ----------------- --------------------------------
Sound                        Open
---------------------------- ----------------- --------------------------------
Multiplayer                  Open
---------------------------- ----------------- --------------------------------
Music                        Open
============================ ================= ================================

Getting Started
-------------------------------------------------------

HW Configuration Index
~~~~~~~~~

============================= ================= ================================
 Item                          Version            Comment
============================= ================= ================================
`nRF5340`_ dev kit             2.0.1            N/A
----------------------------- ----------------- --------------------------------
3,5" ILI9486 `display`_        N/A              N/A
----------------------------- ----------------- --------------------------------
2.8" ILI9341 `SPI display`_    N/A              N/A
----------------------------- ----------------- -------------------------------- 
4.3" FT810 `SPI IPS display`_  N/A              N/A
----------------------------- ----------------- --------------------------------
`micro:bit v2`_                2.x              N/A
----------------------------- ----------------- --------------------------------
`joystick v2`_                 2                N/A
----------------------------- ----------------- --------------------------------
`Xbox controller`_             model 1914       N/A
----------------------------- ----------------- --------------------------------
`keyboard`_                    RPi 400          N/A   
============================= ================= ================================


SW Configuration Index
~~~~~~~~~

======================= ================= ================================
 Item                    Version            Comment
======================= ================= ================================
Windows 11 Enterprise    10.0.22631       N/A     
----------------------- ----------------- --------------------------------
Visual Studio Code       1.98.2           N/A
----------------------- ----------------- --------------------------------
nRF Connect SDK          v2.6.2           N/A
----------------------- ----------------- --------------------------------
nRF Connect for VS Code  2025.1.127       Can be downloaded using the Toolchain Manager found in nRF Connect for Desktop. Alternatively, it can be downloaded directly from inside Visual Studio Code.
======================= ================= ================================

Compatibility Matrix
~~~~~~~~~

=========================== ================= ================================ ================================ ================================ ================================ ================================
 Item                        Version           MVP1                             MVP2                             MVP3                             MVP4                             MVP5
=========================== ================= ================================ ================================ ================================ ================================ ================================
`nRF5340`_ dev kit           2.0.1             X                                X                                X                                X                                X
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
3,5" ILI9486 `display`_      N/A               X                                X                                --                               --                               --
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
2.8" ILI9341 `SPI display`_  N/A               --                               --                               X                                --                               --
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
4.3" `SPI IPS display`_      N/A               --                               --                               --                               X                                X
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
`micro:bit v2`_              2.x               --                               X                                X                                X                                --
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
`joystick v2`_               2                 --                               X                                X                                X                                --
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
`Xbox controller`_           model 1914        --                               --                               --                               --                               X
--------------------------- ----------------- -------------------------------- -------------------------------- -------------------------------- -------------------------------- --------------------------------
`keyboard`_                  RPi 400           --                               --                               --                               --                               X
=========================== ================= ================================ ================================ ================================ ================================ ================================
-- means not supported, X means supported

Prerequisites
~~~~~~~~~

#. Install the `Visual Studio Code`_.
#. Install the `nRF Connect for VS Code`_.
#. Install the `nRF Connect SDK`_.

Build
~~~~~~~~~
Game
^^^^^^^^^
* VS Code -> nRF Connect extension -> Add Folder as Application -> select zephyrdoom folder.
* VS Code -> nRF Connect extension -> APPLICATIONS -> Add build configuration -> select board target nrf5340dk_nrf5340_cpuapp -> Build Configuration.
Gamepad
^^^^^^^^^
* VS Code -> nRF Connect extension -> Add Folder as Application -> select gamepad\\microbit folder.
* VS Code -> nRF Connect extension -> APPLICATIONS -> Add build configuration -> select board target bbc_microbit_v2 -> Build Configuration.

Flash
~~~~~~~~~
Game
^^^^^^^^^
#. Connect Nordic Semiconductor `nRF5340`_ dev kit.
#. Flash data (contains WAD file) to external flash::
   
     nrfjprog --family nrf53 --qspicustominit --program qspi.hex --verify
#. Select game app.  VS Code -> nRF Connect extension -> APPLICATIONS -> Select zephyrdoom.
#. Flash the game. VS Code -> nRF Connect extension -> ACTIONS -> Flash.

Gamepad
^^^^^^^^^
#. Connect `micro:bit v2`_.
#. Select gamepad app.  VS Code -> nRF Connect extension -> APPLICATIONS -> Select microbit.
#. Copy file gamepad/microbit/build/zephyr/zephyr.hex to micro:bit (acting as a removable usb device).

Monitor
~~~~~~~~~
* VS Code -> nRF Connect extension -> CONNECTED DEVICES -> VCOM1 -> Connect to Serial Port.

MVPs
-------------------------------------------------------

MVP1
~~~~~~~~~

* Goal - Port `nrf-doom`_ to `nRF Connect SDK`_ and get it up and running, with display and basic control using dev kit buttons, under Zephyr RTOS (still with dependency on nRF Connect SDK).
* Features:
 * QSPI flash
 * Doom engine running
 * Basic display support
 * Nordic Semiconductor `nRF5340`_ dev kit button control
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 3,5" ILI9486 `display`_, no touch
* Pin mapping
======================= ================= ================================
Peripheral              Function          nRF5340 Pin
======================= ================= ================================
Buttons                 Button 1          P0.23
----------------------- ----------------- --------------------------------
Buttons                 Button 2          P0.24
----------------------- ----------------- --------------------------------
Buttons                 Button 3          P0.8
----------------------- ----------------- --------------------------------
Buttons                 Button 4          P0.9
----------------------- ----------------- --------------------------------
LEDs                    LED 1             P0.28
----------------------- ----------------- --------------------------------
LEDs                    LED 2             P0.29
----------------------- ----------------- --------------------------------
LEDs                    LED 3             P0.30
----------------------- ----------------- --------------------------------
LEDs                    LED 4             P0.31
----------------------- ----------------- --------------------------------
SPI/SD card             SS                P1.12
----------------------- ----------------- --------------------------------
SPI/SD card             DI                P1.13
----------------------- ----------------- --------------------------------
SPI/SD card             DO                P1.14
----------------------- ----------------- --------------------------------
SPI/SD card             SCK               P1.15
----------------------- ----------------- --------------------------------
QSPI Memory             SCK               P0.17
----------------------- ----------------- --------------------------------
QSPI Memory             CSN               P0.18
----------------------- ----------------- --------------------------------
QSPI Memory             IO0               P0.13
----------------------- ----------------- --------------------------------
QSPI Memory             IO1               P0.14
----------------------- ----------------- --------------------------------
QSPI Memory             IO2               P0.15
----------------------- ----------------- --------------------------------
QSPI Memory             IO3               P0.16
----------------------- ----------------- --------------------------------
LCD                     Bit 1-2           P1.10 - P1.11
----------------------- ----------------- --------------------------------
LCD                     Bit 3-8           P1.04 - P1.09
----------------------- ----------------- --------------------------------
LCD                     RST               P0.25
----------------------- ----------------- --------------------------------
LCD                     CS                P0.07
----------------------- ----------------- --------------------------------
LCD                     RS                P0.06
----------------------- ----------------- --------------------------------
LCD                     WR                P0.05
----------------------- ----------------- --------------------------------
LCD                     RD                P0.04
======================= ================= ================================

MVP2
~~~~~~~~~

* Goal - Integrate a Bluetooth Low Energy (BLE) game controller to enable game control through a micro:bit-based gamepad. This addition will enhance the gaming experience by allowing players to utilize the micro:bit as a game controller via wireless connectivity.
* Features:
 * BLE game controller
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 3,5" ILI9486 `display`_, no touch
 * `micro:bit v2`_
 * ELECFREAKS `joystick v2`_
* Pin mapping
======================= ================= ================================
Peripheral              Function          nRF5340 Pin
======================= ================= ================================
Buttons                 Button 1          P0.23
----------------------- ----------------- --------------------------------
Buttons                 Button 2          P0.24
----------------------- ----------------- --------------------------------
Buttons                 Button 3          P0.8
----------------------- ----------------- --------------------------------
Buttons                 Button 4          P0.9
----------------------- ----------------- --------------------------------
LEDs                    LED 1             P0.28
----------------------- ----------------- --------------------------------
LEDs                    LED 2             P0.29
----------------------- ----------------- --------------------------------
LEDs                    LED 3             P0.30
----------------------- ----------------- --------------------------------
LEDs                    LED 4             P0.31
----------------------- ----------------- --------------------------------
SPI/SD card             SS                P1.12
----------------------- ----------------- --------------------------------
SPI/SD card             DI                P1.13
----------------------- ----------------- --------------------------------
SPI/SD card             DO                P1.14
----------------------- ----------------- --------------------------------
SPI/SD card             SCK               P1.15
----------------------- ----------------- --------------------------------
QSPI Memory             SCK               P0.17
----------------------- ----------------- --------------------------------
QSPI Memory             CSN               P0.18
----------------------- ----------------- --------------------------------
QSPI Memory             IO0               P0.13
----------------------- ----------------- --------------------------------
QSPI Memory             IO1               P0.14
----------------------- ----------------- --------------------------------
QSPI Memory             IO2               P0.15
----------------------- ----------------- --------------------------------
QSPI Memory             IO3               P0.16
----------------------- ----------------- --------------------------------
LCD                     Bit 1-2           P1.10 - P1.11
----------------------- ----------------- --------------------------------
LCD                     Bit 3-8           P1.04 - P1.09
----------------------- ----------------- --------------------------------
LCD                     RST               P0.25
----------------------- ----------------- --------------------------------
LCD                     CS                P0.07
----------------------- ----------------- --------------------------------
LCD                     RS                P0.06
----------------------- ----------------- --------------------------------
LCD                     WR                P0.05
----------------------- ----------------- --------------------------------
LCD                     RD                P0.04
======================= ================= ================================

.. _nRF Connect SDK: https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK/GetStarted
.. _micro:bit v2: https://microbit.org/new-microbit/
.. _joystick v2: https://shop.elecfreaks.com/products/elecfreaks-micro-bit-joystick-bit-v2-kit
.. _display: https://www.laskakit.cz/320x480-barevny-lcd-tft-displej-3-5-shield-arduino-uno/

MVP3
~~~~~~~~~

* Goal - Migrate from current display (connected via GPIO) to SPI one. Support full screen.
* Features:
 * New display
 * Full screen
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 2.8" ILI9341 `SPI display`_
 * `micro:bit v2`_
 * ELECFREAKS `joystick v2`_
* Pin mapping
======================= ================= ================================
Peripheral              Function          nRF5340 Pin
======================= ================= ================================
Buttons                 Button 1          P0.23
----------------------- ----------------- --------------------------------
Buttons                 Button 2          P0.24
----------------------- ----------------- --------------------------------
Buttons                 Button 3          P0.8
----------------------- ----------------- --------------------------------
Buttons                 Button 4          P0.9
----------------------- ----------------- --------------------------------
LEDs                    LED 1             P0.28
----------------------- ----------------- --------------------------------
LEDs                    LED 2             P0.29
----------------------- ----------------- --------------------------------
LEDs                    LED 3             P0.30
----------------------- ----------------- --------------------------------
LEDs                    LED 4             P0.31
----------------------- ----------------- --------------------------------
QSPI Memory             SCK               P0.17
----------------------- ----------------- --------------------------------
QSPI Memory             CSN               P0.18
----------------------- ----------------- --------------------------------
QSPI Memory             IO0               P0.13
----------------------- ----------------- --------------------------------
QSPI Memory             IO1               P0.14
----------------------- ----------------- --------------------------------
QSPI Memory             IO2               P0.15
----------------------- ----------------- --------------------------------
QSPI Memory             IO3               P0.16
----------------------- ----------------- --------------------------------
SPI/ILI9341             SCK               P1.15
----------------------- ----------------- --------------------------------
SPI/ILI9341             MOSI              P1.13
----------------------- ----------------- --------------------------------
SPI/ILI9341             MISO              P1.14
----------------------- ----------------- --------------------------------
SPI/ILI9341             CS                P1.12
======================= ================= ================================

.. _SPI display: https://cz.mouser.com/ProductDetail/Adafruit/1947?qs=GURawfaeGuArmJSJoJoDJA%3D%3D

MVP4
~~~~~~~~~

* Goal - Return back to 4.3" FT810 `SPI IPS display`_ used in original `nrf-doom`_ project.
* Features:
 * Display supporting > 30FPS
 * Full screen 
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 4.3" FT810 `SPI IPS display`_
 * `micro:bit v2`_
 * ELECFREAKS `joystick v2`_
* Pin mapping
======================= ================= ================================
Peripheral              Function          nRF5340 Pin
======================= ================= ================================
Buttons                 Button 1          P0.23
----------------------- ----------------- --------------------------------
Buttons                 Button 2          P0.24
----------------------- ----------------- --------------------------------
Buttons                 Button 3          P0.8
----------------------- ----------------- --------------------------------
Buttons                 Button 4          P0.9
----------------------- ----------------- --------------------------------
LEDs                    LED 1             P0.28
----------------------- ----------------- --------------------------------
LEDs                    LED 2             P0.29
----------------------- ----------------- --------------------------------
LEDs                    LED 3             P0.30
----------------------- ----------------- --------------------------------
LEDs                    LED 4             P0.31
----------------------- ----------------- --------------------------------
QSPI Memory             SCK               P0.17
----------------------- ----------------- --------------------------------
QSPI Memory             CSN               P0.18
----------------------- ----------------- --------------------------------
QSPI Memory             IO0               P0.13
----------------------- ----------------- --------------------------------
QSPI Memory             IO1               P0.14
----------------------- ----------------- --------------------------------
QSPI Memory             IO2               P0.15
----------------------- ----------------- --------------------------------
QSPI Memory             IO3               P0.16
----------------------- ----------------- --------------------------------
SPI/FT810 Display       SCK               P0.06
----------------------- ----------------- --------------------------------
SPI/FT810 Display       MISO              P0.05
----------------------- ----------------- --------------------------------
SPI/FT810 Display       MOSI              P0.25
----------------------- ----------------- --------------------------------
SPI/FT810 Display       CS_N              P0.07
----------------------- ----------------- --------------------------------
SPI/FT810 Display       PD_N              P0.26
======================= ================= ================================

.. _SPI IPS display: https://www.hotmcu.com/43-graphical-ips-lcd-touchscreen-800x480-spi-ft810-p-333.html

MVP5
~~~~~~~~~

* Goal - Add option to connect BLE gamepad (Xbox wireless controller) and ideally BLE keyboard (built on RPi 400). This step will enable full game control (i.e. all the options).
* Features:
 * If using BLE gamepad - TBD. 
 * If using BLE keayborad - full-fledged game control per original manual (inc. cheats). 
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 4.3" FT810 `SPI IPS display`_
 * `xbox controller`_ 
 * `keyboard`_ 
* Pin mapping
======================= ================= ================================
Peripheral              Function          nRF5340 Pin
======================= ================= ================================
Buttons                 Button 1          P0.23
----------------------- ----------------- --------------------------------
Buttons                 Button 2          P0.24
----------------------- ----------------- --------------------------------
Buttons                 Button 3          P0.8
----------------------- ----------------- --------------------------------
Buttons                 Button 4          P0.9
----------------------- ----------------- --------------------------------
LEDs                    LED 1             P0.28
----------------------- ----------------- --------------------------------
LEDs                    LED 2             P0.29
----------------------- ----------------- --------------------------------
LEDs                    LED 3             P0.30
----------------------- ----------------- --------------------------------
LEDs                    LED 4             P0.31
----------------------- ----------------- --------------------------------
QSPI Memory             SCK               P0.17
----------------------- ----------------- --------------------------------
QSPI Memory             CSN               P0.18
----------------------- ----------------- --------------------------------
QSPI Memory             IO0               P0.13
----------------------- ----------------- --------------------------------
QSPI Memory             IO1               P0.14
----------------------- ----------------- --------------------------------
QSPI Memory             IO2               P0.15
----------------------- ----------------- --------------------------------
QSPI Memory             IO3               P0.16
----------------------- ----------------- --------------------------------
SPI/FT810 Display       SCK               P0.06
----------------------- ----------------- --------------------------------
SPI/FT810 Display       MISO              P0.05
----------------------- ----------------- --------------------------------
SPI/FT810 Display       MOSI              P0.25
----------------------- ----------------- --------------------------------
SPI/FT810 Display       CS_N              P0.07
----------------------- ----------------- --------------------------------
SPI/FT810 Display       PD_N              P0.26
======================= ================= ================================

* Xbox Controller Mapping
======================= ===============================================
Xbox Button             Description
======================= ===============================================
Left Stick              Move forward/backward, look left/right
----------------------- -----------------------------------------------
A Button                Cycle through available weapons
----------------------- -----------------------------------------------
B Button                Fire current weapon / Select option in menu
----------------------- -----------------------------------------------
X Button                Hold to run
----------------------- -----------------------------------------------
Y Button                Open doors, activate switches
----------------------- -----------------------------------------------
D-Pad Up                Move up in menu
----------------------- -----------------------------------------------
D-Pad Down              Move down in menu
----------------------- -----------------------------------------------
Left Trigger            Strafe left
----------------------- -----------------------------------------------
Right Trigger           Strafe right
----------------------- -----------------------------------------------
Back Button             Toggle automap view
----------------------- -----------------------------------------------
Start Button            Show menu
======================= ===============================================

* Xbox Controller Setup
1. Hold the pairing button on the front of the controller, until the light starts blinking quickly.
2. Restart the board running the zephyrdoom project.
3. Wait for 5-10 seconds.
4. Hold the pairing button again, until the light stops blinking and stays on.

.. _Xbox controller: https://www.xbox.com/en-US/accessories/controllers/xbox-wireless-controller
.. _keyboard: https://www.raspberrypi.com/products/raspberry-pi-400/


Release Notes
-------------------------------------------------------

Fixed Bugs
~~~~~~~~~
N/A

Known Bugs
~~~~~~~~~
N/A

Implemented Improvements
~~~~~~~~~
MVP1
^^^^^^^^^
* N/A
MVP2
^^^^^^^^^
* FPS increase.
* Moved from proprietary radio com between gamepad and game to BLE com.
MVP3
^^^^^^^^^
* N/A
MVP4
^^^^^^^^^
* Moved to 4.3" FT810 `SPI IPS display`_ used in original `nrf-doom`_ project.
* FPS > 30

Known Limitations
~~~~~~~~~
MVP1
^^^^^^^^^
* Low FPS (~8).
* Picture is rotated by 90 degrees. Plus, we are not using full display area.
MVP2
^^^^^^^^^
* Low FPS (~14).
* Picture is rotated by 90 degrees. Plus, we are not using full display area.
* BLE game controller requires manual setting of offsets (hard-coded) to eliminate drift. Calibration procedure could help to address this issue.
MVP3
^^^^^^^^^
* Low FPS (~5).
MVP4
^^^^^^^^^
* N/A
