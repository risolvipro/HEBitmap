import os
import argparse
from PIL import Image
import re

bitmap_version = 1
bitmap_table_version = 1

# arguments

parser = argparse.ArgumentParser(description="HEBitmap Encoder")
parser.add_argument('-i', "--input", help="input file. You can pass in a file or a folder (bitmap table). The folder uses the same name convention of a Playdate table e.g. name-table-1.png", required=True)

args = parser.parse_args()

input_filename = args.input

working_dir = os.getcwd()
output_dir = working_dir

def set_bit(byte, value, position):
    if value == 1:
        return byte | (1 << position)
    elif value == 0:
        return byte & ~(1 << position)
    return byte

def encode_image(file):
    im = Image.open(file)
    has_mask = 'A' in im.mode
    
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
    
    output.extend(bitmap_version.to_bytes(4, byteorder="big"))

    output.extend(w.to_bytes(4, byteorder="big"))
    output.extend(h.to_bytes(4, byteorder="big"))
    output.extend(bx.to_bytes(4, byteorder="big"))
    output.extend(by.to_bytes(4, byteorder="big"))
    output.extend(bw.to_bytes(4, byteorder="big"))
    output.extend(bh.to_bytes(4, byteorder="big"))
    output.extend(rowbytes.to_bytes(4, byteorder="big"))
    has_mask_int = 1 if has_mask else 0
    output.extend(has_mask_int.to_bytes(1, byteorder="big"))

    output.extend(data)
    if has_mask:
        output.extend(mask)

    return output

input_file = os.path.join(working_dir, input_filename)
if os.path.isabs(input_filename):
    input_file = input_filename
    output_dir = os.path.dirname(input_filename)

if os.path.isfile(input_file):
    imageData = encode_image(input_file)
    
    filename = os.path.basename(input_file)
    filename_no_ext = os.path.splitext(filename)[0]
    
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

        tableData = bytearray()

        tableData.extend(bitmap_table_version.to_bytes(4, byteorder="big"))
        tableData.extend(len(tableFiles).to_bytes(4, byteorder="big"))

        for tableFile in tableFiles:
            filepath = tableFile[2]
            imageData = encode_image(filepath)

            tableData.extend(len(imageData).to_bytes(4, byteorder="big"))
            tableData.extend(imageData)

        result_filename = tableName + ".hebt"

        f = open(os.path.join(output_dir, result_filename), "wb")
        f.write(tableData)
        f.close()