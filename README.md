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
#include "hebitmap.h"

// Initialize with PlaydateAPI pointer
// Pass 1 to enable Lua support
HEBitmapInit(pd, 0);

// Load HEBitmap from a Playdate image file
HEBitmap *bitmap = HEBitmapLoad("catbus");

// Draw
HEBitmapDraw(bitmap, 0, 0);

// Free
HEBitmapFree(bitmap);
```

## C Docs

__HEBitmapTable* HEBitmapLoad(const char *filename);__\
Load a new HEBitmap from a Playdate image file (similar to playdate->graphics->loadBitmap).

__HEBitmapTable* HEBitmapLoadHEB(const char *filename);__\
Load a new HEBitmap from a .heb file (use the Python encoder to create it).

__HEBitmap* HEBitmapFromLCDBitmap(LCDBitmap *lcd_bitmap);__\
Create a HEBitmap from a LCDBitmap.

__void HEBitmapSetClipRect(x, y, width, height);__\
Set the clip rect.

__void HEBitmapClearClipRect(void);__\
Clear the clip rect.

__HEBitmapTable* HEBitmapTableLoad(const char *filename);__\
Load a new HEBitmap from a Playdate image table file (similar to playdate->graphics->loadBitmapTable).

__HEBitmapTable* HEBitmapTableLoadHEBT(const char *filename);__\
Load a new HEBitmapTable from a .hebt file (use the Python encoder to create it).

__HEBitmapTable* HEBitmapTableFromLCDBitmapTable(LCDBitmapTable *bitmapTable);__\
Create a HEBitmapTable from a LCDBitmapTable.

__HEBitmap* HEBitmapAtIndex(HEBitmapTable *bitmapTable, unsigned int index);__\
Get the bitmap at the given index in a table.

__void HEBitmapTableFree(HEBitmapTable *bitmapTable);__\
Free the bitmap table.

## Lua

```c
#include "hebitmap.h"

// Initialize in kEventInitLua
// Pass 1 to enable Lua support
HEBitmapInit(pd, 1);
```

```lua
import "hebitmap"

-- Load HEBitmap from a Playdate image file
local bitmap = hebitmap.bitmap.new("catbus")

function playdate.update()
    -- Draw
	bitmap:draw(100, 100)
end
```

## Create a C-Lua project on macOS

* Install CMake (run `brew install cmake` from the terminal)
* In the *lua* folder of the repo you can find a minimal template to create a C-Lua project
* Copy all the files beginning with "hebitmap" from *src* into *lua/src*
* Navigate into the *lua* folder and create a *build* folder
* Open the terminal and navigate into it

### Simulator build
* Run `cd <your_build_folder>`
* Run `cmake  ..`
* Run `make`

### Device build
* Before building again, delete all the files in the *build* folder
* Run `cmake -DCMAKE_TOOLCHAIN_FILE=<path to SDK>/PlaydateSDK/C_API/buildsupport/arm.cmake -DCMAKE_BUILD_TYPE=Release ..`
* Note: replace `<path to SDK>` with your SDK path
* Run `make`

You can now build and run it as a Lua project.

## Lua Docs

__hebitmap.bitmap.new(filename)__\
Create a new HEBitmap from a Playdate image file (similar to playdate.graphics.image.new)

__hebitmap.bitmap.loadHEB(filename)__\
Create a new HEBitmap from a .heb file (use the Python encoder to create it).

__hebitmap.bitmap:draw(x, y)__\
Draw the bitmap at the given position.

__hebitmap.bitmap:getColorAt(x, y)__\
Get the color at the given position. It returns a Playdate color such as *playdate.graphics.kColorBlack*

__hebitmap.graphics.setClipRect(x, y, width, height)__\
Set the clip rect.

__hebitmap.graphics.clearClipRect()__\
Clear the clip rect.

__hebitmap.bitmaptable.new(filename)__\
Load a new HEBitmapTable from a Playdate image table file (similar to playdate.graphics.imagetable.new).

__hebitmap.bitmaptable.loadHEBT(filename)__\
Load a new HEBitmapTable from a .hebt file (use the Python encoder to create it).

__hebitmap.bitmaptable:getBitmap(index)__\
Get the bitmap at the given index in a table.