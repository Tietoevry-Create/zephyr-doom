#!/usr/bin/python3
import btfpy
import evdev
from evdev import InputDevice, categorize, ecodes
import time
import threading

def find_keyboard():
    devices = [InputDevice(path) for path in evdev.list_devices()]
    for device in devices:
        print(f"Found device: {device.name} at {device.path}")
        if "Keyboard" in device.name:
            return device.path
    return None

modifier_map = {
    "KEY_LEFTCTRL": 0x01, "KEY_LEFTSHIFT": 0x02, "KEY_LEFTALT": 0x04, "KEY_LEFTMETA": 0x08,
    "KEY_RIGHTCTRL": 0x10, "KEY_RIGHTSHIFT": 0x20, "KEY_RIGHTALT": 0x40, "KEY_RIGHTMETA": 0x80,
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
    "KEY_KPDOT": 0x63,
    # HID usage codes for modifiers (0xE0-0xE7) are handled by the modifier byte,
    # so they are not needed here for placement in keycode slots.
}

current_modifiers = 0
pressed_non_modifier_keys = set()
MAX_SIMULTANEOUS_NON_MODIFIER_KEYS = 6

reportmap = [0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,\
             0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x06,\
             0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0]
name = "HID"
appear = [0xC1,0x03]
pnpinfo = [0x02,0x6B,0x1D,0x46,0x02,0x37,0x05]
protocolmode = [0x01]
hidinfo = [0x01,0x11,0x00,0x02]
reportindex = -1
node = 0


def send_hid_report():
    """Constructs and sends the current HID keyboard report."""
    global current_modifiers, pressed_non_modifier_keys, reportindex, node, MAX_SIMULTANEOUS_NON_MODIFIER_KEYS

    if reportindex < 0 or node == 0:
        return

    report_data = [0] * 8
    report_data[0] = current_modifiers

    active_key_list = list(pressed_non_modifier_keys)

    if len(active_key_list) > MAX_SIMULTANEOUS_NON_MODIFIER_KEYS:
        # print(f"Warning: More than {MAX_SIMULTANEOUS_NON_MODIFIER_KEYS} non-modifier keys pressed. Sending ErrorRollOver.")
        for i in range(MAX_SIMULTANEOUS_NON_MODIFIER_KEYS):
            report_data[i + 2] = 0x01
    else:
        for i in range(MAX_SIMULTANEOUS_NON_MODIFIER_KEYS):
            if i < len(active_key_list):
                report_data[i + 2] = active_key_list[i]
            else:
                report_data[i + 2] = 0

    # print(f"Sending HID report: {report_data}")
    btfpy.Write_ctic(node, reportindex, report_data, 0)


def keyboard_listener():
    global current_modifiers, pressed_non_modifier_keys

    # keyboard_path = find_keyboard()
    keyboard_path = "/dev/input/event4"
    if not keyboard_path:
        print("No keyboard found! Make sure it's connected.")
        return

    dev = InputDevice(keyboard_path)
    print(f"Listening for keypresses on {dev.name} ({dev.phys})")

    try:
        for event in dev.read_loop():
            if event.type == ecodes.EV_KEY:
                keycode_name = evdev.ecodes.KEY.get(event.code)
                if keycode_name is None:
                    keycode_name = f"UNKNOWN_CODE_{event.code}"

                if keycode_name in modifier_map:
                    modifier_bit = modifier_map[keycode_name]
                    if event.value == 1:
                        current_modifiers |= modifier_bit
                    elif event.value == 0:
                        current_modifiers &= ~modifier_bit
                    elif event.value == 2:
                         current_modifiers |= modifier_bit
                    send_hid_report()

                elif keycode_name in keycode_map:
                    hid_keycode = keycode_map[keycode_name]
                    if event.value == 1:
                        pressed_non_modifier_keys.add(hid_keycode)
                        send_hid_report()
                    elif event.value == 0:
                        pressed_non_modifier_keys.discard(hid_keycode)
                        send_hid_report()
                    elif event.value == 2:
                        pass

                elif keycode_name != f"UNKNOWN_CODE_{event.code}" and event.value in (0,1,2):
                     pass
    except KeyboardInterrupt:
        print("\nKeyboard listener stopped by user.")
    except Exception as e:
        print(f"Error in keyboard listener: {e}")
    finally:
        print("Cleaning up: Sending final all-keys-up report.")
        current_modifiers = 0
        pressed_non_modifier_keys.clear()
        send_hid_report()


def lecallback(clientnode,op,cticn):
  if(op == btfpy.LE_CONNECT):
    print("Connected OK. Key presses sent to client. ESC stops server")
    while True:
        time.sleep(10)

  if(op == btfpy.LE_KEYPRESS):
    if(cticn == 27):
        print("ESC received via LE_KEYPRESS, stopping server.")
        return(btfpy.SERVER_EXIT)
    print(f"LE_KEYPRESS cticn: {cticn}")
  if(op == btfpy.LE_DISCONNECT):
    print("Disconnected.")
    return(btfpy.SERVER_EXIT)
  return(btfpy.SERVER_CONTINUE)


if(btfpy.Init_blue("keyboard.txt") == 0):
  exit(0)

if(btfpy.Localnode() != 1):
  print("ERROR - Edit keyboard.txt to set ADDRESS = " + btfpy.Device_address(btfpy.Localnode()))
  exit(0)

node = btfpy.Localnode()

uuid = [0x2A,0x4D]
reportindex = btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid)
if(reportindex < 0):
  print("Failed to find Report characteristic (2A4D)")
  exit(0)

uuid_name = [0x2A,0x00]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_name),name,0)
uuid_appear = [0x2A,0x01]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_appear),appear,0)
uuid_proto = [0x2A,0x4E]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_proto),protocolmode,0)
uuid_hidinfo = [0x2A,0x4A]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_hidinfo),hidinfo,0)
uuid_reportmap = [0x2A,0x4B]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_reportmap),reportmap,0)
uuid_pnp = [0x2A,0x50]; btfpy.Write_ctic(node,btfpy.Find_ctic_index(node,btfpy.UUID_2,uuid_pnp),pnpinfo,0)

print("Sending initial all_keys_up HID report.")
send_hid_report()

randadd = [0xD3,0x56,0xDB,0x15,0x32,0xA0]
btfpy.Set_le_random_address(randadd)

btfpy.Set_le_wait(20000)
btfpy.Le_pair(btfpy.Localnode(),btfpy.JUST_WORKS,0)

print("Starting keyboard listener thread...")
keyboard_thread = threading.Thread(target=keyboard_listener, daemon=True)
keyboard_thread.start()

print("Starting Bluetooth LE server...")
btfpy.Le_server(lecallback,0)

print("Bluetooth LE server stopped.")
btfpy.Close_all()