#!/usr/bin/env python3
"""
Convert JPG/PNG image to C array for ESP32 LCD display (ST7789, RGB565)
Usage: python convert_image.py openclaw.jpg
"""

import sys
from PIL import Image
import os

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 format"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert_image_to_c_array(image_path, output_name="image"):
    """Convert image to C array"""
    
    # Open and resize image to 240x240
    try:
        img = Image.open(image_path)
    except Exception as e:
        print(f"Error opening image: {e}")
        return
    
    print(f"Original image size: {img.size}")
    
    # Resize to 240x240 (maintaining aspect ratio with padding)
    target_size = (240, 240)
    img.thumbnail(target_size, Image.Resampling.LANCZOS)
    
    # Create new image with black background
    new_img = Image.new('RGB', target_size, (0, 0, 0))
    
    # Paste resized image in center
    paste_x = (target_size[0] - img.size[0]) // 2
    paste_y = (target_size[1] - img.size[1]) // 2
    new_img.paste(img, (paste_x, paste_y))
    
    img = new_img.convert('RGB')
    width, height = img.size
    
    print(f"Output size: {width}x{height}")
    print(f"Converting to RGB565 format...")
    
    # Convert to RGB565 array
    pixels = []
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            rgb565 = rgb888_to_rgb565(r, g, b)
            pixels.append(rgb565)
    
    # Generate C header file
    output_h = f"{output_name}.h"
    with open(output_h, 'w') as f:
        f.write(f"/* Auto-generated from {os.path.basename(image_path)} */\n")
        f.write(f"/* Image size: {width}x{height}, format: RGB565 */\n\n")
        f.write(f"#pragma once\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"#define {output_name.upper()}_WIDTH  {width}\n")
        f.write(f"#define {output_name.upper()}_HEIGHT {height}\n\n")
        f.write(f"static const uint16_t {output_name}_data[] = {{\n")
        
        # Write pixel data (16 values per line)
        for i, pixel in enumerate(pixels):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{pixel:04X}")
            if i < len(pixels) - 1:
                f.write(", ")
            if (i + 1) % 16 == 0:
                f.write("\n")
        
        if len(pixels) % 16 != 0:
            f.write("\n")
        
        f.write("};\n")
    
    print(f"✓ Generated: {output_h}")
    print(f"✓ Array size: {len(pixels) * 2} bytes ({len(pixels) * 2 / 1024:.1f} KB)")
    print(f"\nTo use in your code:")
    print(f'  #include "{output_h}"')
    print(f'  lcd_draw_bitmap(0, 0, {output_name.upper()}_WIDTH, {output_name.upper()}_HEIGHT, {output_name}_data);')

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python convert_image.py <image_file> [output_name]")
        print("Example: python convert_image.py openclaw.jpg openclaw")
        sys.exit(1)
    
    image_path = sys.argv[1]
    output_name = sys.argv[2] if len(sys.argv) > 2 else "openclaw_image"
    
    if not os.path.exists(image_path):
        print(f"Error: File '{image_path}' not found!")
        sys.exit(1)
    
    convert_image_to_c_array(image_path, output_name)
