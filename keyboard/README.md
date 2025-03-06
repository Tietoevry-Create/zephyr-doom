# Raspberry Pi Keyboard

The code in this folder allows you to make a Raspberry Pi act as a bluetooth keyboard that zephyr-doom can connect to.

## Hardware Requirements

* Any Raspberry Pi with bluetooth support and a USB-A port (tested on Raspberry Pi 4 Model B 1GB RAM)
* Raspberry Pi power supply
* Raspberry Pi SD card
* Any USB-A keyboard (tested on Dell KB-216)


## Setup
The goal is to download and build the python version of the [btferret repository](https://github.com/petzval/btferret) to the Raspberry Pi and then replace the keyboard.py file in the repository with the keyboard.py file in this folder.

The simplest way is to:

1. Set up the Raspberry Pi with an internet connection and ssh access. For that you can follow the [instructions on the official Raspberry Pi website](https://www.raspberrypi.com/documentation/computers/getting-started.html).

2. Connect to the board using SSH and follow Python Instructions in the btferret repository.

3. Run
```bash
pip install evdev
```

4. Replace the keyboard.py file with the one in this folder.


## Usage
1. Connect the keyboard to the RPI and run this command in the downloaded repository:

```bash
sudo python3 keyboard.py
```

2. Turn on zephyr-doom. It should connect to the RPI and start receiving key presses

