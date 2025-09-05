import os
import argparse
import sys
from PIL import Image

parser = argparse.ArgumentParser(description="HEBitmap Decoder")
parser.add_argument('-i', "--input", help="input file. You can pass a .heb or .hebt file.", required=True)

args = parser.parse_args()

input_arg = args.input
output_dir = os.getcwd()
input_file = os.path.join(os.getcwd(), input_arg)
if os.path.isabs(input_arg):
    input_file = input_arg
    output_dir = os.path.dirname(input_arg)

filename = os.path.splitext(os.path.basename(input_file))[0]
extension = os.path.splitext(input_file)[1]

def read_u8(data, offset):
    value = int.from_bytes(data[offset[0]:(offset[0]+1)], byteorder="big", signed=False)
    offset[0] += 1
    return value

def read_bool(data, offset):
    value = int.from_bytes(data[offset[0]:(offset[0]+1)], byteorder="big", signed=False)
    offset[0] += 1
    return False if value == 0 else True

def read_u32(data, offset):
    value = int.from_bytes(data[offset[0]:(offset[0]+4)], byteorder="big", signed=False)
    offset[0] += 4
    return value

def decompress(data, offset, length):
    result = []
    i = 0
    while i < length:
        count = read_u8(data, offset)
        value = read_u8(data, offset)
        end = i + count
        if end > length:
            end = length
        while i < end:
            result.append(value)
            i += 1
    return result

def unpack_bits(data):
    result = []
    for byte in data:
        for i in range(7, -1, -1):
            result.append((byte >> i) & 1)
    return result

def decode_heb(data):
    offset = [0]

    version = read_u32(data, offset)
    
    width = read_u32(data, offset)
    height = read_u32(data, offset)
    bx = read_u32(data, offset)
    by = read_u32(data, offset)
    bw = read_u32(data, offset)
    bh = read_u32(data, offset)
    rowbytes = read_u32(data, offset)
    has_mask = read_bool(data, offset)
    
    compressed = False
    if version >= 3:
        # Version 3 supports compression
        compressed = read_bool(data, offset)

    if version >= 2:
        # Version 2 supports padding
        padding_len = read_u32(data, offset)
        offset[0] += padding_len

    data_size = rowbytes * bh

    image_data = None
    mask_data = None
    
    if compressed:
        image_data = unpack_bits(decompress(data, offset, data_size))
        if has_mask:
            mask_data = unpack_bits(decompress(data, offset, data_size))
    else:
        image_data = unpack_bits(data[offset[0]:(offset[0]+data_size)])
        offset[0] += data_size
        if has_mask:
            mask_data = unpack_bits(data[offset[0]:(offset[0]+data_size)])
            offset[0] += data_size

    img = Image.new("RGBA", (width, height))
    for y1 in range(bh):
        y = by + y1
        for x1 in range(bw):
            x = bx + x1
            color = (0 if image_data[y1 * rowbytes * 8 + x1] == 0 else 255)
            alpha = 255
            if mask_data is not None:
                alpha = (0 if mask_data[y1 * rowbytes * 8 + x1] == 0 else 255)
            img.putpixel((x, y), (color, color, color, alpha))

    return {
        "bounds": [bx, by, bw, bh],
        "img": img
    }

def decode_hebt(data):
    offset = [0]

    version = read_u32(data, offset)
    length = read_u32(data, offset)

    compressed = False
    if version >= 3:
        # Version 3 supports compression
        compressed = read_bool(data, offset)

        if version >= 4 and compressed:
            # Allocator length
            read_u32(data, offset)
        
    if version >= 2:
        # Version 2 supports padding
        padding_len = read_u32(data, offset)
        offset[0] += padding_len
    
    bitmaps = []
    for _ in range(length):
        bitmap_size = read_u32(data, offset)

        info = decode_heb(data[offset[0]:(offset[0]+bitmap_size)])
        offset[0] += bitmap_size
        bitmaps.append(info)

    return {
        "bitmaps": bitmaps
    }

with open(input_file, "rb") as f:
    data = f.read()

if extension == ".heb":
    info = decode_heb(data)
    info["img"].save(os.path.join(output_dir, filename + ".png"))
elif extension == ".hebt":
    info = decode_hebt(data)

    dst_path = os.path.join(output_dir, filename)
    i = 2
    while os.path.exists(dst_path):
        dst_path = os.path.join(output_dir, filename + "-" + str(i))
        i += 1

    os.makedirs(dst_path)

    i = 1
    for bitmap in info["bitmaps"]:
        bitmap["img"].save(os.path.join(dst_path, filename + "-table-" + str(i) + ".png"))
        i += 1