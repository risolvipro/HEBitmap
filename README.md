<picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark-512.png">
    <img src="assets/logo-light-512.png" width="400" alt="HEBitmap logo">
</picture>

## HEBitmap (Playdate)

HEBitmap (**H**igh **E**fficiency Bitmap) is a custom implementation of the drawBitmap function (Playdate SDK).

This implementation is up to 4x faster than the SDK drawBitmap function, but native features (clipRect, flip, stencil) are not supported.

## Benchmark

Bitmap info: 48x22 (masked)

| N Bitmaps | HEBitmap | drawBitmap | Faster
|:---|:---|:---|:---|
| 2000 | 30 ms | 120 ms | 4x
| 1000 | 20 ms | 63 ms | 3x
| 500 | 19 ms | 33 ms | 1.7x

## Example

```c
#include "hebitmap.h"

// Set Playdate API pointer
HEBitmapSetPlaydateAPI(pd);

// Load a LCDBitmap
LCDBitmap *lcd_bitmap = playdate->graphics->loadBitmap("test", NULL);

// New HEBitmap from LCDBitmap
HEBitmap *he_bitmap = HEBitmapNew(lcd_bitmap);

// Draw
HEBitmapDraw(he_bitmap, 0, 0);

// Free
HEBitmapFree(he_bitmap);

// Free original LCDBitmap
playdate->graphics->freeBitmap(lcd_bitmap);
```

## Demo

You can get the compiled PDX demo [here](demo/HEBitmap.pdx.zip "PDX demo"). Press A to switch between custom / SDK implementation.
