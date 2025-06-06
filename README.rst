
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

Table of Contents
-------------------------------------------------------

- `Status`_
- `Getting Started`_
- `Software`_

Status
-------------------------------------------------------

Backlog
~~~~~~~~~

============================ ================= ================================
 Feature                     Status            Comment
============================ ================= ================================
Read game data QSPI Flash    Done
---------------------------- ----------------- --------------------------------
Doom game engine running     Done
---------------------------- ----------------- --------------------------------
Basic display support         Done             Currently, there is a limitation on the FPS and the picture is rotated by 90 degrees
---------------------------- ----------------- --------------------------------
Devkit button control        Done
---------------------------- ----------------- --------------------------------
BLE game controller          Done              Transition from the original proprietary radio protocol to BLE using the Nordic UART Service (NUS) for data exchange
---------------------------- ----------------- --------------------------------
Display enhancements         In Progress       Migrate from 'GPIO' display to SPI one (without hardware accelerator)
---------------------------- ----------------- --------------------------------
Game data (SD card to flash) Open              Automatically copy game data from SD card to flash during first start (or on demand). 
---------------------------- ----------------- --------------------------------
BLE HID                      Open              Replace NUS communication with BLE HID standard to allow control via BLE keybords, gamepads etc. 
---------------------------- ----------------- --------------------------------
Basic CI                     Open              Build the code for target platfrom(s) in GitHub actions
---------------------------- ----------------- --------------------------------
Sound                        Open
---------------------------- ----------------- --------------------------------
Pure Zephyr port             Open              Modify the code to make it compatible with any Zephyr device that meets the resource requirements by removing nrfx/ncs elements
---------------------------- ----------------- --------------------------------
Multiplayer                  Open
---------------------------- ----------------- --------------------------------
Music                        Open
---------------------------- ----------------- --------------------------------
QSPI Memory Map              Open              Address depends on the hardware
============================ ================= ================================

Compatibility Matrix
~~~~~~~~~

======================= ================= ================================ ================================ ================================
 Item                    Version           MVP1                             MVP2                             MVP3
======================= ================= ================================ ================================ ================================
`nRF5340`_ dev kit       2.0.1            X                                 X
----------------------- ----------------- -------------------------------- -------------------------------- --------------------------------
3,5" `display`_          N/A              X                                 X
----------------------- ----------------- -------------------------------- -------------------------------- --------------------------------
2.8" `SPI display`_      N/A              --                                --
----------------------- ----------------- -------------------------------- -------------------------------- --------------------------------
`micro:bit v2`_          2.x              --                                X
----------------------- ----------------- -------------------------------- -------------------------------- --------------------------------
`joystick v2`_           2                --                                X
======================= ================= ================================ ================================ ================================
-- means not supported, X means supported

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

.. _nRF Connect SDK : https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK/GetStarted
.. _micro:bit v2: https://microbit.org/new-microbit/
.. _joystick v2: https://shop.elecfreaks.com/products/elecfreaks-micro-bit-joystick-bit-v2-kit
.. _display: https://www.laskakit.cz/320x480-barevny-lcd-tft-displej-3-5-shield-arduino-uno/

MVP3
~~~~~~~~~

* Goal - Migrate from current display (connected via GPIO) to SPI one. Support full-screen.
* Features:
 * New display
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 2.8" ILI9341 `SPI display`_
 * `micro:bit v2`_
 * ELECFREAKS `joystick v2`_

.. _SPI display: https://cz.mouser.com/ProductDetail/Adafruit/1947?qs=GURawfaeGuArmJSJoJoDJA%3D%3D

MVP4
~~~~~~~~~

* Goal - Add option to connect BLE gamepad (e.g. Xbox controller) and ideally BLE keyboard. This step will enable full game control (i.e. all the options).
* Features:
 * If using BLE gamepad - TBD. 
 * If using BLE keayborad - full-fledged game control per original manual (inc. cheats). 
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 2.8" ILI9341 `SPI display`_
 * BLE gamepad/keyboard

MVP5
~~~~~~~~~

* Goal - Add sound via I2S.
* Features:
 * Game sounds.
* Required Hardware
 * Nordic Semiconductor `nRF5340`_ dev kit
 * 2.8" ILI9341 `SPI display`_
 * TBD

Getting Started
-------------------------------------------------------

HW Configuration Index
~~~~~~~~~
======================= ================= 
 Item                    Version          
======================= ================= 
`nRF5340`_ dev kit       2.0.1            
----------------------- ----------------- 
3,5" `display`_          N/A              
----------------------- ----------------- 
2.8" `SPI display`_      N/A              
----------------------- ----------------- 
`micro:bit v2`_          2.x              
----------------------- ----------------- 
`joystick v2`_           2                
======================= =================


SW Configuration Index
~~~~~~~~~

======================= ================= ================================
 Item                    Version            Comment
======================= ================= ================================
Windows 11 Enterprise    10.0.22631       N/A     
----------------------- ----------------- --------------------------------
Visual Studio Code       1.93.1           N/A
----------------------- ----------------- --------------------------------
nRF Connect for Desktop  v5.0.2           N/A
----------------------- ----------------- --------------------------------
Toolchain Manager        v1.5.2           N/A
----------------------- ----------------- --------------------------------
nRF Connect SDK          v2.6.2           N/A
----------------------- ----------------- --------------------------------
nRF Connect for VS Code  v2024.9.87       Can be downloaded using the Toolchain Manager found in nRF Connect for Desktop. Alternatively, it can be downloaded directly from inside Visual Studio Code.
======================= ================= ================================

Prerequisites
~~~~~~~~~

#. Install the `Visual Studio Code`_.
#. Install the `nRF Connect for VS Code`_.

Build
~~~~~~~~~
Game
^^^^^^^^^
* VS Code -> nRF Connect extension -> Add Folder as Application -> select zephyrdoom folder.
* VS Code -> nRF Connect extension -> APPLICATIONS -> Add build configuration -> select board target nrf5340dk_nrf5340_cpuapp -> Build Configuration.
Gamepad
^^^^^^^^^
* VS Code -> nRF Connect extension -> Add Folder as Application -> select gamepad\microbit folder.
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

Software
-------------------------------------------------------

Fixed Bugs
~~~~~~~~~
N/A

Known Bugs
~~~~~~~~~
N/A

Improvements
~~~~~~~~~
MVP2
^^^^^^^^^
* FPS increase.
* Moved from proprietary radio com between gamepad and game to BLE com.

To be Improved
~~~~~~~~~
MVP1
^^^^^^^^^
* Low FPS (~8).
* Picture is rotated by 90 degrees. Plus, we are not using full display area.
MVP2
^^^^^^^^^
* Low FPS (~14).
* Picture is rotated by 90 degrees. Plus, we are not using full display area.
* Limited game control ('not enough buttons on the gamepad').
* Need to flash qspi before flashing the application.

