<picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark-512.png">
    <img src="assets/logo-light-512.png" width="400" alt="HEBitmap logo">
</picture>

## HEBitmap (Playdate)

HEBitmap (**H**igh **E**fficiency Bitmap) is a custom implementation of the drawBitmap function (Playdate SDK).

This implementation is up to 2x faster than the SDK drawBitmap function, but native features (flip, stencil) are not supported.

## Benchmark

Bitmap info: 96x96 (masked)

| Bitmap count | HEBitmap | drawBitmap | Faster
|:---|:---|:---|:---|
| 1000 | 22 ms | 42 ms | 1.9x

## C Example

```c
#include "he_api.h"

// Initialize with PlaydateAPI pointer
he_library_init(pd);

// Load HEBitmap from a Playdate image file
HEBitmap *bitmap = HEBitmap_load("catbus");

// Draw
HEBitmap_draw(bitmap, 0, 0);

// Free
HEBitmap_free(bitmap);
```

## C Docs

[C API Documentation](https://risolvipro.github.io/HEBitmap/C-API.html)

### Simulator build
* Run `cd <your_build_folder>`
* Run `cmake  ..`
* Run `make`

### Device build
* Before building again, delete all the files in the *build* folder
* Run `cmake -DCMAKE_TOOLCHAIN_FILE=<path to SDK>/PlaydateSDK/C_API/buildsupport/arm.cmake -DCMAKE_BUILD_TYPE=Release ..`
* Note: replace `<path to SDK>` with your SDK path
* Run `make`

## Encoder

You can use the Python encoder to create heb/hebt files, supported inputs are:
* Image
* Animated GIF (saved as bitmap table)
* Folder containing a Playdate image table (name-table-1.png, name-table-2.png, ...)

### Parameters
* `-i` `--input` input file or folder
* `-r` `--raw` save as raw data (no compression)

### Usage
`python encoder.py -i <file_or_folder>`