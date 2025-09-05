import os
import argparse
from PIL import Image
from PIL import ImageSequence
import re

format_version = 4

# arguments

parser = argparse.ArgumentParser(description="HEBitmap Encoder")
parser.add_argument('-i', "--input", help="input file. You can pass in a file or a folder containing a Playdate image table (name-table-1.png, name-table-2.png, ...). Compression is enabled by default.", required=True)
parser.add_argument('-r', "--raw", help="Save the file as raw data (no compression).", required=False, action='store_true')

args = parser.parse_args()

input_arg = args.input
compressed = not args.raw

working_dir = os.getcwd()
output_dir = working_dir

def set_bit(byte, value, position):
    if value == 1:
        return byte | (1 << position)
    elif value == 0:
        return byte & ~(1 << position)
    return byte

def add_padding(data):
    padding = (len(data) + 4) % 32
    if padding > 0:
        padding = 32 - padding
    data.extend(padding.to_bytes(4, byteorder="big"))
    for _ in range(padding):
        data.extend((0).to_bytes(1, byteorder="big"))
        
def compress(data, rowbytes, bh):
    output = bytearray()
    
    last_byte = 0b00000000
    last_byte_set = False
    count = 0

    for row in range(bh):
        for col in range(rowbytes):
            byte = data[row * rowbytes + col]

            if not last_byte_set:
                last_byte_set = True
                last_byte = byte
            
            changed = True

            if last_byte == byte:
                if count < 255:
                    count += 1
                    changed = False

            if changed:
                output.extend(count.to_bytes(1, byteorder="big"))
                output.extend(last_byte.to_bytes(1, byteorder="big"))
                count = 1
                last_byte = byte

    if count > 0:
        output.extend(count.to_bytes(1, byteorder="big"))
        output.extend(last_byte.to_bytes(1, byteorder="big"))

    return output

def encode_image(im):
    im = im.convert('RGBA')
    
    w, h = im.size

    bounds = (0, 0, w, h)
    bbox = im.getbbox()
    if bbox:
        bounds = bbox

    bx, by, b_right, b_bottom = bounds
    bw = b_right - bx
    bh = b_bottom - by

    rowbytes = ((bw + 31) // 32) * 4
    aligned_width = rowbytes * 8

    has_mask = False

    data = bytearray()
    mask = bytearray()

    for y in range(by, by + bh):
        data_byte = 0b00000000
        mask_byte = 0b00000000
        
        bit8_count = 0

        for x in range(bx, bx + aligned_width):
            r, g, b, a = (0, 0, 0, 0)

            if x < (bx + bw):
                r, g, b, a = im.getpixel((x,y))
                if a <= 127:
                    has_mask = True        

            color = round(0.2125 * r + 0.7154 * g + 0.0721 * b)
            
            data_bit = 0 if (color < 127) else 1
            mask_bit = 0 if (a < 127) else 1

            data_byte = set_bit(data_byte, data_bit, (7 - bit8_count))
            mask_byte = set_bit(mask_byte, mask_bit, (7 - bit8_count))

            bit8_count += 1
            if bit8_count == 8:
                data.extend(data_byte.to_bytes(1, byteorder="big"))
                mask.extend(mask_byte.to_bytes(1, byteorder="big"))

                data_byte = 0b00000000
                mask_byte = 0b00000000
                bit8_count = 0

    output = bytearray()
    
    output.extend(format_version.to_bytes(4, byteorder="big"))

    output.extend(w.to_bytes(4, byteorder="big"))
    output.extend(h.to_bytes(4, byteorder="big"))
    output.extend(bx.to_bytes(4, byteorder="big"))
    output.extend(by.to_bytes(4, byteorder="big"))
    output.extend(bw.to_bytes(4, byteorder="big"))
    output.extend(bh.to_bytes(4, byteorder="big"))
    output.extend(rowbytes.to_bytes(4, byteorder="big"))
    has_mask_int = 1 if has_mask else 0
    output.extend(has_mask_int.to_bytes(1, byteorder="big"))
    
    if format_version >= 3:
        compressed_int = 1 if compressed else 0
        output.extend(compressed_int.to_bytes(1, byteorder="big"))

    if format_version >= 2:
        add_padding(output)

    if format_version >= 3 and compressed:
        data = compress(data, rowbytes, bh)
        if has_mask:
            mask = compress(mask, rowbytes, bh)

        output.extend(data)
        if has_mask:
            output.extend(mask)
    else:
        output.extend(data)
        if has_mask:
            output.extend(mask)

    bufferLen = rowbytes * bh
    if has_mask:
        bufferLen += rowbytes * bh

    return (output, bufferLen)

def save_table(name, tableImages):
    data = bytearray()

    data.extend(format_version.to_bytes(4, byteorder="big"))
    data.extend(len(tableImages).to_bytes(4, byteorder="big"))

    if format_version >= 3:
        compressed_int = 1 if compressed else 0
        data.extend(compressed_int.to_bytes(1, byteorder="big"))

        if format_version >= 4 and compressed:
            bufferLen = 0
            for tableImage in tableImages:
                bufferLen += tableImage[1]
            data.extend(bufferLen.to_bytes(4, byteorder="big"))

    if format_version >= 2:
        add_padding(data)

    for tableImage in tableImages:
        data.extend(len(tableImage[0]).to_bytes(4, byteorder="big"))
        data.extend(tableImage[0])

    result_filename = name + ".hebt"
    f = open(os.path.join(output_dir, result_filename), "wb")
    f.write(data)
    f.close()

input_file = os.path.join(working_dir, input_arg)
if os.path.isabs(input_arg):
    input_file = input_arg
    output_dir = os.path.dirname(input_arg)

if os.path.isfile(input_file):
    filename = os.path.basename(input_file)
    filename_no_ext = os.path.splitext(filename)[0]
    
    im = Image.open(input_file)
    
    if im.format == "GIF" and im.is_animated:
        tableImages = []
        for frame in ImageSequence.Iterator(im):
            tableImages.append(encode_image(frame))
        
        save_table(filename_no_ext, tableImages)
    else:
        (imageData, bufferLen) = encode_image(im)
        result_filename = filename_no_ext + ".heb"

        f = open(os.path.join(output_dir, result_filename), "wb")
        f.write(imageData)
        f.close()
elif os.path.isdir(input_file):
    input_dir = input_file
    dirname = os.path.basename(input_dir)

    tableFiles = []
    for filename in os.listdir(input_dir):
        filepath = os.path.join(input_dir, filename)
        filename_no_ext = os.path.splitext(filename)[0]
        match = re.search("(.+)-table-([0-9]+)$", filename_no_ext)
        if match:
            name = match[1]
            index = int(match[2])
            tableFiles.append((index, name, filepath))
    
    tableFiles = sorted(tableFiles, key=lambda file: file[0])

    if len(tableFiles) > 0:
        tableName = tableFiles[0][1]

        tableImages = []
        for tableFile in tableFiles:
            filename = tableFile[2]
            im = Image.open(filename)
            tableImages.append(encode_image(im))
        
        save_table(tableName, tableImages)