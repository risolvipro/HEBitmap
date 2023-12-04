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
// Set pointer to Playdate API
HEBitmapSetPlaydateAPI(pd);

// Load a LCDBitmap
LCDBitmap *lcd_bitmap = playdate->graphics->loadBitmap("test", NULL);

// New HEBitmap from LCDBitmap
HEBitmap *he_bitmap = HEBitmapNew(lcd_bitmap);

// Draw it
HEBitmapDraw(he_bitmap, 0, 0);
```

## Notes

The library provides the routine for masked bitmaps, you can comment out `#define HEBITMAP_MASK` in the source to generate the function for opaque bitmaps.

If you need both, you can copy-paste the `HEBitmapDrawMask` function and prepend `#undef HEBITMAP_MASK` to generate both functions.

## Demo

You can get the compiled PDX demo [here](demo/HEBitmap.pdx.zip "PDX demo"). Press A to switch between custom / SDK implementation.
