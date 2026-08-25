#!/usr/bin/env python3
import sys

def main():
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        data = f.read()
    with open(dst, "w") as o:
        o.write("/* AUTO-GENERATED from doom1.wad. Do not edit. */\n")
        o.write("const unsigned char doom_wad[] = {\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            o.write("  " + ",".join(str(b) for b in chunk) + ",\n")
        o.write("};\n")
        o.write("const unsigned int doom_wad_len = %d;\n" % len(data))

if __name__ == "__main__":
    main()
