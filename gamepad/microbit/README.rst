Bluetooth Gamepad
##################

This sample connects a gamepad to an nRF5340 board over Bluetooth® Low Energy (LE).
The gamepad sends joystick position and button state data at regular intervals to the board.

Requirements
************

- A Micro:Bit V2 board or compatible board
- For receiving data, a nRF5340 DK or compatible board running the code at ../z ephyrdoom.

Building and Uploading
**********************

1. **Build the Sample**
    Open the sample directory in VS Code using the NRF Connect extension and build the project for your board.
    Functionality was tested on the BBC Micro:Bit V2 board using prj_minimal.conf and app.overlay.

2. **Upload the Sample**
    Connect the gamepad to your computer and copy the file from ./build/zephyr/zephyr.hex to the gamepad.
    The gamepad will automatically restart and launch the application.
    The application will then connect to the NRF5340 board when it is running.
