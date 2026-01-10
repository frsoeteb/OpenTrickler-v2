#!/usr/bin/env python3
"""
Create combined bootloader+app UF2 for initial Pico flashing
"""
import sys
import struct

def read_uf2(filename):
    """Read UF2 file and return list of blocks"""
    blocks = []
    with open(filename, 'rb') as f:
        while True:
            data = f.read(512)
            if len(data) < 512:
                break
            blocks.append(data)
    return blocks

def write_uf2(filename, blocks):
    """Write UF2 blocks to file"""
    with open(filename, 'wb') as f:
        for block in blocks:
            f.write(block)

def main():
    if len(sys.argv) != 4:
        print("Usage: create_combined_image.py bootloader.uf2 app.uf2 combined.uf2")
        sys.exit(1)
    
    bootloader_file = sys.argv[1]
    app_file = sys.argv[2]
    output_file = sys.argv[3]
    
    print(f"Reading {bootloader_file}...")
    bootloader_blocks = read_uf2(bootloader_file)
    
    print(f"Reading {app_file}...")
    app_blocks = read_uf2(app_file)
    
    print(f"Combining {len(bootloader_blocks)} bootloader blocks + {len(app_blocks)} app blocks...")
    combined_blocks = bootloader_blocks + app_blocks
    
    print(f"Writing {output_file}...")
    write_uf2(output_file, combined_blocks)
    
    print(f"✓ Combined image created: {len(combined_blocks)} blocks total")

if __name__ == '__main__':
    main()
