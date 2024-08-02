
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

Table of Contents
^^^^^^^^^^^^^^^^^^^^^^^

- `Status`_
- `Getting Started`_
- `Software`_
- `Hardware`_

Status
-------------------------------------------------------
Backlog
"""""""""""""""""""""
============================ ================= ================================
 Feature                     Status            Comment
============================ ================= ================================
Read game data QSPI Flash    Done
---------------------------- ----------------- --------------------------------
Doom game engine running     Done
---------------------------- ----------------- --------------------------------
Basic diplay support         Done              Currently, there is a limitation on the FPS and the picture is rotated by 90 degrees
---------------------------- ----------------- --------------------------------
Devkit button control        Done
---------------------------- ----------------- --------------------------------
BLE game controller          In Progress       Transition from the original proprietary radio protocol to BLE using the Nordic UART Service (NUS) for data exchange
---------------------------- ----------------- --------------------------------
Display enhancements         Open
---------------------------- ----------------- --------------------------------
Game data (SD card to flash) Open
---------------------------- ----------------- --------------------------------
Pure Zephyr port             Open              Modify the code to make it compatible with any Zephyr device that meets the resource requirements by removing nrfx/ncs elements
---------------------------- ----------------- --------------------------------
BLE HID                      Open              Replace NUS communication with BLE HID standard to allow control via BLE keybords, gamepads etc. 
---------------------------- ----------------- --------------------------------
Basic CI                     Open              Build the code for target platfrom(s) in GitHub actions
---------------------------- ----------------- --------------------------------
Multiplayer                  Open
---------------------------- ----------------- --------------------------------
Sound                        Open
---------------------------- ----------------- --------------------------------
Music                        Open
============================ ================= ================================

MVP1
"""""""""""""""""""""
 
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
"""""""""""""""""""""
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

Getting Started
-------------------------------------------------------

Prerequisites
"""""""""""""""""""""

Build
"""""""""""""""""""""

Flash
"""""""""""""""""""""

Monitor
"""""""""""""""""""""

Software
-------------------------------------------------------

Changes
"""""""""""""""""""""

Known Bugs
"""""""""""""""""""""

To be Improved
"""""""""""""""""""""

Hardware
-------------------------------------------------------

System Requirements
"""""""""""""""""""""""""

CPU, RAM, Flash

Display
""""""""""""""""""""""""


