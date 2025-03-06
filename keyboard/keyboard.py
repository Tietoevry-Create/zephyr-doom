#!/usr/bin/python3
import btfpy
import evdev
from evdev import InputDevice, categorize, ecodes
import time


def find_keyboard():
    devices = [InputDevice(path) for path in evdev.list_devices()]
    for device in devices:
        print(f"Found device: {device.name} at {device.path}")
        if "Keyboard" in device.name:  # Adjust if necessary
            return device.path
    return None

modifier_map = {
    "KEY_LEFTCTRL": 0x01,
    "KEY_LEFTSHIFT": 0x02,
    "KEY_LEFTALT": 0x04,
    "KEY_LEFTMETA": 0x08,  # Left GUI/Windows key
    "KEY_RIGHTCTRL": 0x10,
    "KEY_RIGHTSHIFT": 0x20,
    "KEY_RIGHTALT": 0x40,
    "KEY_RIGHTMETA": 0x80,  # Right GUI/Windows key
}

keycode_map = {  # Map evdev keycodes to HID usage codes
    "KEY_A": 0x04, "KEY_B": 0x05, "KEY_C": 0x06, "KEY_D": 0x07, "KEY_E": 0x08,
    "KEY_F": 0x09, "KEY_G": 0x0A, "KEY_H": 0x0B, "KEY_I": 0x0C, "KEY_J": 0x0D,
    "KEY_K": 0x0E, "KEY_L": 0x0F, "KEY_M": 0x10, "KEY_N": 0x11, "KEY_O": 0x12,
    "KEY_P": 0x13, "KEY_Q": 0x14, "KEY_R": 0x15, "KEY_S": 0x16, "KEY_T": 0x17,
    "KEY_U": 0x18, "KEY_V": 0x19, "KEY_W": 0x1A, "KEY_X": 0x1B, "KEY_Y": 0x1C,
    "KEY_Z": 0x1D, "KEY_1": 0x1E, "KEY_2": 0x1F, "KEY_3": 0x20, "KEY_4": 0x21,
    "KEY_5": 0x22, "KEY_6": 0x23, "KEY_7": 0x24, "KEY_8": 0x25, "KEY_9": 0x26,
    "KEY_0": 0x27, "KEY_ENTER": 0x28, "KEY_ESC": 0x29, "KEY_BACKSPACE": 0x2A,
    "KEY_TAB": 0x2B, "KEY_SPACE": 0x2C, "KEY_MINUS": 0x2D, "KEY_EQUAL": 0x2E,
    "KEY_LEFTBRACE": 0x2F, "KEY_RIGHTBRACE": 0x30, "KEY_BACKSLASH": 0x31,
    "KEY_SEMICOLON": 0x33, "KEY_APOSTROPHE": 0x34, "KEY_GRAVE": 0x35,
    "KEY_COMMA": 0x36, "KEY_DOT": 0x37, "KEY_SLASH": 0x38, "KEY_CAPSLOCK": 0x39,
    "KEY_F1": 0x3A, "KEY_F2": 0x3B, "KEY_F3": 0x3C, "KEY_F4": 0x3D, "KEY_F5": 0x3E,
    "KEY_F6": 0x3F, "KEY_F7": 0x40, "KEY_F8": 0x41, "KEY_F9": 0x42, "KEY_F10": 0x43,
    "KEY_F11": 0x44, "KEY_F12": 0x45, "KEY_SYSRQ": 0x46, "KEY_SCROLLLOCK": 0x47,
    "KEY_PAUSE": 0x48, "KEY_INSERT": 0x49, "KEY_HOME": 0x4A, "KEY_PAGEUP": 0x4B,
    "KEY_DELETE": 0x4C, "KEY_END": 0x4D, "KEY_PAGEDOWN": 0x4E, "KEY_RIGHT": 0x4F,
    "KEY_LEFT": 0x50, "KEY_DOWN": 0x51, "KEY_UP": 0x52, "KEY_NUMLOCK": 0x53,
    "KEY_KPSLASH": 0x54, "KEY_KPASTERISK": 0x55, "KEY_KPMINUS": 0x56,
    "KEY_KPPLUS": 0x57, "KEY_KPENTER": 0x58, "KEY_KP1": 0x59, "KEY_KP2": 0x5A,
    "KEY_KP3": 0x5B, "KEY_KP4": 0x5C, "KEY_KP5": 0x5D, "KEY_KP6": 0x5E,
    "KEY_KP7": 0x5F, "KEY_KP8": 0x60, "KEY_KP9": 0x61, "KEY_KP0": 0x62,
    "KEY_KPDOT": 0x63, "KEY_LEFTCTRL": 0xE0, "KEY_LEFTSHIFT": 0xE1,
    "KEY_LEFTALT": 0xE2, "KEY_LEFTMETA": 0xE3, "KEY_RIGHTCTRL": 0xE4,
    "KEY_RIGHTSHIFT": 0xE5, "KEY_RIGHTALT": 0xE6, "KEY_RIGHTMETA": 0xE7,
}


def keyboard_listener():
    keyboard_path = find_keyboard()
    if not keyboard_path:
        print("No keyboard found! Make sure it's connected.")
        return

    dev = InputDevice(keyboard_path)
    print(f"Listening for keypresses on {dev.name}")

    current_modifiers = 0

    for event in dev.read_loop():
        if event.type == ecodes.EV_KEY:
            keycode_name = evdev.ecodes.KEY[event.code]
            print(f"Key Event: {keycode_name}, Value: {event.value}")

            if keycode_name in modifier_map:
                if event.value == 1:  # Key press
                    current_modifiers |= modifier_map[keycode_name]
                elif event.value == 0:  # Key release
                    current_modifiers &= ~modifier_map[keycode_name]
                send_key(0, 0)  # Send empty event to update modifier state (no key pressed)
            elif keycode_name in keycode_map:
                hid_keycode = keycode_map[keycode_name]
                if event.value == 1:  # Key press
                    send_key(current_modifiers, hid_keycode)  # Send modifier and key code
                elif event.value == 0:  # Key release
                    send_key(0, 0)  # Send release (all zeros)
            else:
                print(f"Unhandled key: {keycode_name}")



# ********** Bluetooth keyboard **********
# From https://github.com/petzval/btferret
#   Build btfpy.so module - instructions in README file
#
# Download
#   keyboard.py
#   keyboard.txt
#
# Edit keyboard.txt to set ADDRESS=
# to the address of the local device
# that runs this code
#
# Run
#   sudo python3 keyboard.py
#
# Connect from phone/tablet/PC to "HID" device
#
# All keystrokes go to connecting device
# F10 sends "Hello" plus Enter
# ESC stops the server
#
# To add a battery level service:
# uncomment all battery level labelled
# code in keyboard.txt and here.
# F10 will then also send a battery level notification
#
# Note: This code uses the lowest level of security.
# Do not use it if you need high security.
#
# Non-GB keyboards
# Even if the keyboard of this device is non-GB
# it must be specified as "gb" in the boot info as follows:
#
# Edit /etc/default/keyboard to include the line:
# XKBLAYOUT="gb"
#
# It is the receiving device that decides which
# characters correspond to which keys. See discussion
# in the HID Devices section of the documentation.
#
# This code sets an unchanging random address.
# If connection is unreliable try changing the address.
#
# See HID Devices section in documentation for
# more infomation.
#
# ********************************

#/*********  keyboard.txt DEVICES file ******
# DEVICE = My Pi   TYPE=Mesh  node=1  ADDRESS = DC:A6:32:04:DB:56
#  PRIMARY_SERVICE = 1800
#    LECHAR=Device Name   SIZE=4   Permit=02 UUID=2A00
#    LECHAR=Appearance    SIZE=2   Permit=02 UUID=2A01
#  PRIMARY_SERVICE = 180A
#    LECHAR= PnP ID       SIZE=7 Permit=02 UUID=2A50
#  PRIMARY_SERVICE = 1812
#    LECHAR=Protocol Mode   SIZE=1  Permit=06  UUID=2A4E
#    LECHAR=HID Info        SIZE=4  Permit=02  UUID=2A4A
#    LECHAR=HID Ctl Point   SIZE=8  Permit=04  UUID=2A4C
#    LECHAR=Report Map      SIZE=47 Permit=02  UUID=2A4B
#    LECHAR=Report1         SIZE=8  Permit=92  UUID=2A4D
#        ; Report1 must have Report ID = 1
#        ;   0x85, 0x01 in Report Map
#        ; uuid = [0x2A,0x4D]
#        ; index = btfpy.Find_ctic_index(btfpy.Localnode(),btfpy.UUID_2,uuid)
#        ; Send data: btfpy.Write_ctic(btfpy.Localnode(),index,data,0)
#
# ;  *** Optional battery level ***
# ;  PRIMARY_SERVICE = 180F
# ;    LECHAR=Battery Level   SIZE=1 Permit=12  UUID=2A19
#
# ********

# **** KEYBOARD REPORT MAP *****
# 0x05, 0x01 Usage Page (Generic Desktop)
# 0x09, 0x06 Usage (Keyboard)
# 0xa1, 0x01 Collection (Application)
# 0x85, 0x01 Report ID = 1
# 0x05, 0x07 Usage Page (Keyboard)
# 0x19, 0xe0 Usage Minimum (Keyboard LeftControl)
# 0x29, 0xe7 Usage Maximum (Keyboard Right GUI)
# 0x15, 0x00 Logical Minimum (0)
# 0x25, 0x01 Logical Maximum (1)
# 0x75, 0x01 Report Size (1)
# 0x95, 0x08 Report Count (8)
# 0x81, 0x02 Input (Data, Variable, Absolute) Modifier byte
# 0x95, 0x01 Report Count (1)
# 0x75, 0x08 Report Size (8)
# 0x81, 0x01 Input (Constant) Reserved byte
# 0x95, 0x06 Report Count (6)
# 0x75, 0x08 Report Size (8)
# 0x15, 0x00 Logical Minimum (0)
# 0x25, 0x65 Logical Maximum (101)
# 0x05, 0x07 Usage Page (Key Codes)
# 0x19, 0x00 Usage Minimum (Reserved (no event indicated))
# 0x29, 0x65 Usage Maximum (Keyboard Application)
# 0x81, 0x00 Input (Data,Array) Key arrays (6 bytes)
# 0xc0 End Collection
#*******************

    # NOTE the size of reportmap (47 in this case) must appear in keyboard.txt as follows:
    #   LECHAR=Report Map      SIZE=47 Permit=02  UUID=2A4B
reportmap = [0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,\
             0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x06,\
             0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0]

    # NOTE the size of report (8 in this case) must appear in keyboard.txt as follows:
    #   LECHAR=Report1         SIZE=8  Permit=92  UUID=2A4D
report = [0,0,0,0,0,0,0,0]

name = "HID"
appear = [0xC1,0x03]  # 03C1 = keyboard icon appears on connecting device
pnpinfo = [0x02,0x6B,0x1D,0x46,0x02,0x37,0x05]
protocolmode = [0x01]
hidinfo = [0x01,0x11,0x00,0x02]
battery = [100]
reportindex = -1
node = 0

def lecallback(clientnode,op,cticn):

  if(op == btfpy.LE_CONNECT):
    print("Connected OK. Key presses sent to client. ESC stops server")
    print("F10 sends Hello plus Enter")

    while True:
      time.sleep(10)

  if(op == btfpy.LE_KEYPRESS):
    # cticn = ASCII code of key OR btferret custom code
    if(cticn == 23):
      # 23 = btferret custom code for F10
      # Send "Hello" plus Enter
      # Must use ord() to send ASCII value from a string
      hello = "Hello\n"
      for n in range(len(hello)):
        send_key(ord(hello[n]))

      #**** battery level ****
      #  if(battery[0] > 0):
      #    battery[0] = battery[0] - 1
      #  uuid = [0x2A,0x19]
      #  btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),battery,1)
      #*******************
    else:
      send_key(cticn)

  if(op == btfpy.LE_DISCONNECT):
    return(btfpy.SERVER_EXIT)
  return(btfpy.SERVER_CONTINUE)


#*********** SEND KEY *****************
# key = ASCII code of character (e.g a=97) OR one of the
#            following btferret custom codes:
#
# 1 = Pause     8 = Backspace  17 = F4     24 = F11
# 2 = Insert    9 = Tab        18 = F5     25 = F12
# 3 = Del      10 = Enter      19 = F6     27 = Esc
# 4 = Home     11 = Pound (^3) 20 = F7     28 = Right arrow
# 5 = End      14 = F1         21 = F8     29 = Left arrow
# 6 = PgUp     15 = F2         22 = F9     30 = Down arrow
# 7 = PgDn     16 = F3         23 = F10    31 = Up arrow
#
# ASCII codes                    'a' = 97               (valid range 32-126)
# CTRL add 128 (0x80)         CTRL a = 'a' + 128 = 225  (valid range 225-255)
# Left ALT add 256 (0x100)     ALT a = 'a' + 256 = 353  (valid range 257-382)
# Right ALT add 384 (0x180)  AltGr a = 'a' + 384 = 481  (valid range 481-516)
#
# SHIFT F1-F8 codes SHIFT F1 = 471  (valid range 471-478)
#
# Note CTRL i = same as Tab  CTRL m = same as Enter
# Some ALT keys generate ASCII codes
#
# To send k: send_key('k')
# To send F1: send_key(14)
# To send CTRL b:  send_key(226) same as send_key('b' | 0x80)
# To send AltGr t: send_key(500) same as send_key('t' | 0x180)
#
# These key codes are also listed in the
# keys_to_callback() section in documentation
#
# Modifier bits, hex values:
# 01=Left CTL  02=Left Shift  04=Left Alt  08=Left GUI
# 10=Right CTL 20=Right Shift 40=Right Alt 80=Right GUI
#
#*************************************

def send_key(modifiers, keycode):
    global reportindex
    global node

    buf = [0, 0, 0, 0, 0, 0, 0, 0]

    buf[0] = modifiers  # Modifier byte
    buf[2] = keycode    # Keycode

    btfpy.Write_ctic(node, reportindex, buf, 0)  # Send the report

    # Send key release (important!)
    buf = [0, 0, 0, 0, 0, 0, 0, 0]  # All zeros for release
    btfpy.Write_ctic(node, reportindex, buf, 0)

############ START ###########

if(btfpy.Init_blue("keyboard.txt") == 0):
  exit(0)

if(btfpy.Localnode() != 1):
  print("ERROR - Edit keyboard.txt to set ADDRESS = " + btfpy.Device_address(btfpy.Localnode()))
  exit(0)

node = btfpy.Localnode()

# look up Report1 index
uuid = [0x2A,0x4D]
reportindex = btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid)
if(reportindex < 0):
  print("Failed to find Report characteristic")
  exit(0)

  # Write data to local characteristics  node=local node
uuid = [0x2A,0x00]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),name,0)

uuid = [0x2A,0x01]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),appear,0)

uuid = [0x2A,0x4E]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),protocolmode,0)

uuid = [0x2A,0x4A]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),hidinfo,0)

uuid = [0x2A,0x4B]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),reportmap,0)

uuid = [0x2A,0x4D]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),report,0)

uuid = [0x2A,0x50]
btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),pnpinfo,0)

  #**** battery level *****
  # uuid = [0x2A,0x19]
  # btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid),battery,1)
  #************************

  # Set unchanging random address by hard-coding a fixed value.
  # If connection produces an "Attempting Classic connection"
  # error then choose a different address.
  # If set_le_random_address() is not called, the system will set a
  # new and different random address every time this code is run.

  # Choose the following 6 numbers
  # 2 hi bits of first number must be 1
randadd = [0xD3,0x56,0xDB,0x15,0x32,0xA0]
btfpy.Set_le_random_address(randadd)

btfpy.Keys_to_callback(btfpy.KEY_ON,0)   # enable LE_KEYPRESS calls in lecallback
                                         # 0 = GB keyboard
btfpy.Set_le_wait(20000)  # Allow 20 seconds for connection to complete

btfpy.Le_pair(btfpy.Localnode(),btfpy.JUST_WORKS,0)  # Easiest option, but if client requires
                                                     # passkey security - remove this command

import threading

keyboard_thread = threading.Thread(target=keyboard_listener, daemon=True)
keyboard_thread.start()

btfpy.Le_server(lecallback,0)



btfpy.Close_all()