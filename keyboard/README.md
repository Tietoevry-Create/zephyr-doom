# Raspberry Pi Keyboard

The code in this folder allows you to make a Raspberry Pi act as a bluetooth keyboard that zephyr-doom can connect to.

## Hardware Requirements

* Any Raspberry Pi with Bluetooth support and a USB-A port (tested on Raspberry Pi 4 Model B 1GB RAM and Raspberry Pi 400 — the model that includes a built-in keyboard)
* Raspberry Pi power supply
* Raspberry Pi SD card
* Any USB-A keyboard (tested on Dell KB-216; not required when using the Raspberry Pi 400 since it includes a built-in keyboard)


## Setup
The goal is to download and build the python version of the [btferret repository](https://github.com/petzval/btferret) to the Raspberry Pi and then replace the keyboard.py file in the repository with the keyboard.py file in this folder.

The simplest way is to:

1. Set up the Raspberry Pi with an internet connection and ssh access. For that you can follow the [instructions on the official Raspberry Pi website](https://www.raspberrypi.com/documentation/computers/getting-started.html).

2. Connect to the board using SSH and follow the Python Instructions in the btferret repository.

    2.1. ```sudo apt-get install git```

    2.2. ```sudo git clone https://github.com/petzval/btferret.git```

    2.3. ```sudo apt-get install python3-setuptools python3-dev```

    2.4. ```cd btferret && python3 btfpymake.py build```

    2.5. ```sudo pip install evdev```

3. Replace the keyboard.py file with the one in this folder.


## Usage
1. Connect the keyboard to the RPI and run this command in the btferret repository:

```bash
sudo python3 keyboard.py
```

2. Turn on the board running zephyr-doom. It should connect to the RPI and start receiving key presses.

