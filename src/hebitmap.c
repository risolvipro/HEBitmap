//
//  hebitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "hebitmap.h"
#include "hebitmap_prv.h"

#include "hebitmap_draw.h" // Opaque
#define HEBITMAP_MASK
#include "hebitmap_draw.h" // Mask
#undef HEBITMAP_MASK

static void buffer_align_8_32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int x, int y, int width, int height, uint8_t fill_value);
static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh);
static inline uint32_t bswap32(uint32_t n);
static HEBitmap* HEBitmapFromBuffer(uint8_t *buffer, int isOwner, int hasData);
static uint32_t read_uint8(uint8_t **buffer_ptr);
static uint32_t read_uint32(uint8_t **buffer_ptr);

static PlaydateAPI *playdate;

static char *lua_kBitmap = "hebitmap.bitmap";
static char *lua_kBitmapTable = "hebitmap.bitmaptable";

static _HEBitmapRect screenRect = (_HEBitmapRect){
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};

HEBitmap* HEBitmapBase(void)
{
    HEBitmap *bitmap = playdate->system->realloc(NULL, sizeof(HEBitmap));
    
    _HEBitmap *prv = playdate->system->realloc(NULL, sizeof(_HEBitmap));
    bitmap->prv = prv;
    
    prv->buffer = NULL;
    prv->isOwner = 0;
    prv->hasData = 0;
    prv->bitmapTable = NULL;
    
    bitmap->width = 0;
    bitmap->height = 0;
    prv->bx = 0;
    prv->by = 0;
    prv->bw = 0;
    prv->bh = 0;
    prv->rowbytes = 0;
    
    prv->data = NULL;
    prv->mask = NULL;
    
    prv->luaRef = NULL;
    prv->luaTableRef = NULL;

    return bitmap;
}

HEBitmap* HEBitmapLoad(const char *filename)
{
    LCDBitmap *lcd_bitmap = playdate->graphics->loadBitmap(filename, NULL);
    if(lcd_bitmap)
    {
        HEBitmap *bitmap = HEBitmapFromLCDBitmap(lcd_bitmap);
        playdate->graphics->freeBitmap(lcd_bitmap);
        return bitmap;
    }
    return NULL;
}

HEBitmap* _HEBitmapFromLCDBitmap(LCDBitmap *lcd_bitmap, int isOwner)
{
    HEBitmap *bitmap = HEBitmapBase();
    _HEBitmap *prv = bitmap->prv;
    
    prv->isOwner = isOwner;
    prv->hasData = 1;
    
    int width, height, rowbytes;
    uint8_t *mask, *data;
    playdate->graphics->getBitmapData(lcd_bitmap, &width, &height, &rowbytes, &mask, &data);
    
    int bx, by, bw, bh;
    get_bounds(mask, rowbytes, width, height, &bx, &by, &bw, &bh);
    
    prv->bx = bx;
    prv->by = by;
    prv->bw = bw;
    prv->bh = bh;

    bitmap->width = width;
    bitmap->height = height;
    
    int rowbytes_aligned = ((bw + 31) / 32) * 4;
    
    prv->rowbytes = rowbytes_aligned;
    
    size_t data_size = rowbytes_aligned * bh;

    prv->data = playdate->system->realloc(NULL, data_size);
    buffer_align_8_32(prv->data, data, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
    
    prv->mask = NULL;
    
    if(mask)
    {
        prv->mask = playdate->system->realloc(NULL, data_size);
        buffer_align_8_32(prv->mask, mask, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
    }
    
    return bitmap;
}

HEBitmap* HEBitmapFromLCDBitmap(LCDBitmap *lcd_bitmap)
{
    return _HEBitmapFromLCDBitmap(lcd_bitmap, 1);
}

HEBitmap* HEBitmapLoadHEB(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(0, file_size);
    playdate->file->read(file, buffer, file_size);
    
    playdate->file->close(file);
    
    return HEBitmapFromBuffer(buffer, 1, 1);
}

HEBitmap* HEBitmapFromBuffer(uint8_t *buffer, int isOwner, int hasData)
{
    HEBitmap *bitmap = HEBitmapBase();
    _HEBitmap *prv = bitmap->prv;
    
    prv->buffer = buffer;
    prv->isOwner = isOwner;
    prv->hasData = hasData;
    
    uint8_t *buffer_ptr = buffer;
    
    // read version
    read_uint32(&buffer_ptr);
    
    bitmap->width = read_uint32(&buffer_ptr);
    bitmap->height = read_uint32(&buffer_ptr);
    prv->bx = read_uint32(&buffer_ptr);
    prv->by = read_uint32(&buffer_ptr);
    prv->bw = read_uint32(&buffer_ptr);
    prv->bh = read_uint32(&buffer_ptr);
    prv->rowbytes = read_uint32(&buffer_ptr);
    
    uint8_t has_mask = read_uint8(&buffer_ptr);
    
    prv->data = buffer_ptr;
    
    prv->mask = NULL;
    if(has_mask)
    {
        buffer_ptr += prv->rowbytes * prv->bh;
        prv->mask = buffer_ptr;
    }
    
    return bitmap;
}

void HEBitmapDraw(HEBitmap *bitmap, int x, int y)
{
    if(((_HEBitmap*)bitmap->prv)->mask)
    {
        HEBitmapDrawMask(playdate, bitmap, x, y);
    }
    else
    {
        HEBitmapDrawOpaque(playdate, bitmap, x, y);
    }
}

LCDColor HEBitmapColorAt(HEBitmap *bitmap, int x, int y)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(x >= prv->bx && x < (prv->bx + prv->bw) && y >= prv->by && y < (prv->by + prv->bh))
    {
        int src_x = x - prv->bx;
        int src_y = y - prv->by;

        int i = src_y * prv->rowbytes + (unsigned int)src_x / 8;
        uint8_t bitmask = (1 << (7 - (unsigned int)src_x % 8));
        
        if(prv->mask && !(prv->mask[i] & bitmask))
        {
            return kColorClear;
        }
        else if(prv->data[i] & bitmask)
        {
            return kColorWhite;
        }
        else
        {
            return kColorBlack;
        }
    }
    
    return kColorClear;
}

void HEBitmapGetData(HEBitmap *bitmap, uint8_t **data, uint8_t **mask, int *rowbytes, int *bx, int *by, int *bw, int *bh)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(data)
    {
        *data = prv->data;
    }
    if(mask)
    {
        *mask = prv->mask;
    }
    
    *rowbytes = prv->rowbytes;
    *bx = prv->bx;
    *by = prv->by;
    *bw = prv->bw;
    *bh = prv->bh;
}

void _HEBitmapFree(HEBitmap *bitmap)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(prv->hasData)
    {
        if(prv->buffer)
        {
            playdate->system->realloc(prv->buffer, 0);
        }
        else
        {
            playdate->system->realloc(prv->data, 0);
            if(prv->mask)
            {
                playdate->system->realloc(prv->mask, 0);
            }
        }
    }
    
    playdate->system->realloc(prv, 0);
    playdate->system->realloc(bitmap, 0);
}

void HEBitmapSetClipRect(int x, int y, int width, int height)
{
    _clipRect = (_HEBitmapRect){
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };
    clipRect = _clipRect;
    
    clipRect.width = hebitmap_max(clipRect.width, 0);
    clipRect.height = hebitmap_max(clipRect.height, 0);
    
    if(clipRect.x < 0)
    {
        int new_width = clipRect.width + clipRect.x;
        clipRect.width = hebitmap_max(new_width, 0);
        clipRect.x = 0;
    }
    if(clipRect.y < 0)
    {
        int new_height = clipRect.height + clipRect.y;
        clipRect.height = hebitmap_max(new_height, 0);
        clipRect.y = 0;
    }
    if((clipRect.x + clipRect.width) > LCD_COLUMNS)
    {
        clipRect.width = LCD_COLUMNS - clipRect.x;
    }
    if((clipRect.y + clipRect.height) > LCD_ROWS)
    {
        clipRect.height = LCD_ROWS - clipRect.y;
    }
}

void HEBitmapClearClipRect(void)
{
    _clipRect = screenRect;
    clipRect = _clipRect;
}

void HEBitmapFree(HEBitmap *bitmap)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(!prv->isOwner)
    {
        if(prv->luaTableRef && prv->bitmapTable)
        {
            HEBitmapTable *bitmapTable = prv->bitmapTable;
            _HEBitmapTable *bitmapTablePrv = bitmapTable->prv;
            bitmapTablePrv->luaRefCount--;
            if(bitmapTablePrv->luaRefCount <= 0)
            {
                playdate->lua->releaseObject(prv->luaTableRef);
                bitmapTablePrv->luaRefCount = 0;
            }
        }
        
        return;
    }
    
    _HEBitmapFree(bitmap);
}

HEBitmapTable* HEBitmapTableBase(void)
{
    HEBitmapTable *bitmapTable = playdate->system->realloc(NULL, sizeof(HEBitmapTable));
    _HEBitmapTable *prv = playdate->system->realloc(NULL, sizeof(_HEBitmapTable));
    bitmapTable->prv = prv;
    
    bitmapTable->length = 0;
    prv->buffer = NULL;
    prv->bitmaps = NULL;
    
    prv->luaRefCount = 0;
    prv->luaRef = NULL;

    return bitmapTable;
}

HEBitmapTable* HEBitmapTableLoad(const char *filename)
{
    LCDBitmapTable *lcd_bitmapTable = playdate->graphics->loadBitmapTable(filename, NULL);
    if(lcd_bitmapTable)
    {
        HEBitmapTable *bitmapTable = HEBitmapTableFromLCDBitmapTable(lcd_bitmapTable);
        playdate->graphics->freeBitmapTable(lcd_bitmapTable);
        return bitmapTable;
    }
    return NULL;
}

HEBitmapTable* HEBitmapTableFromLCDBitmapTable(LCDBitmapTable *lcd_bitmapTable)
{
    HEBitmapTable *bitmapTable = HEBitmapTableBase();
    _HEBitmapTable *prv = bitmapTable->prv;

    int length;
    playdate->graphics->getBitmapTableInfo(lcd_bitmapTable, &length, NULL);
    bitmapTable->length = length;
    
    prv->bitmaps = playdate->system->realloc(0, length * sizeof(HEBitmap*));
    
    for(int i = 0; i < length; i++)
    {
        LCDBitmap *lcd_bitmap = playdate->graphics->getTableBitmap(lcd_bitmapTable, i);
        HEBitmap *bitmap = _HEBitmapFromLCDBitmap(lcd_bitmap, 0);
        _HEBitmap *bitmapPrv = bitmap->prv;
        bitmapPrv->bitmapTable = bitmapTable;
        prv->bitmaps[i] = bitmap;
    }
    
    return bitmapTable;
}

HEBitmapTable* HEBitmapTableLoadHEBT(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(0, file_size);
    playdate->file->read(file, buffer, file_size);
    
    HEBitmapTable *bitmapTable = HEBitmapTableBase();
    _HEBitmapTable *prv = bitmapTable->prv;
    
    prv->buffer = buffer;
    
    uint8_t *buffer_ptr = buffer;
    
    // read version
    read_uint32(&buffer_ptr);
    
    uint32_t length = read_uint32(&buffer_ptr);
    prv->bitmaps = playdate->system->realloc(0, length * sizeof(HEBitmap*));
    bitmapTable->length = length;
    
    for(unsigned int i = 0; i < length; i++)
    {
        uint32_t bitmap_size = read_uint32(&buffer_ptr);
        HEBitmap *bitmap = HEBitmapFromBuffer(buffer_ptr, 0, 0);
        prv->bitmaps[i] = bitmap;
        buffer_ptr += bitmap_size;
    }
    
    playdate->file->close(file);
    
    return bitmapTable;
}

HEBitmap* HEBitmapAtIndex(HEBitmapTable *bitmapTable, unsigned int index)
{
    _HEBitmapTable *prv = bitmapTable->prv;
    
    if(index < bitmapTable->length)
    {
        HEBitmap *bitmap = prv->bitmaps[index];
        _HEBitmap *bitmapPrv = bitmap->prv;
        
        // check if table is a Lua object
        if(prv->luaRef)
        {
            if(!bitmapPrv->luaTableRef)
            {
                if(prv->luaRefCount == 0)
                {
                    playdate->lua->retainObject(prv->luaRef);
                }
                bitmapPrv->luaTableRef = prv->luaRef;
                prv->luaRefCount++;
            }
        }
        
        return prv->bitmaps[index];
    }
    return NULL;
}

void HEBitmapTableFree(HEBitmapTable *bitmapTable)
{
    _HEBitmapTable *prv = bitmapTable->prv;
    
    for(unsigned int i = 0; i < bitmapTable->length; i++)
    {
        HEBitmap *bitmap = prv->bitmaps[i];
        _HEBitmapFree(bitmap);
    }
    playdate->system->realloc(prv->bitmaps, 0);
    
    if(prv->buffer)
    {
        playdate->system->realloc(prv->buffer, 0);
    }
    
    playdate->system->realloc(prv, 0);
    playdate->system->realloc(bitmapTable, 0);
}

static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh)
{
    int min_y = 0; int min_x = 0; int max_x = width; int max_y = height;

    if(mask)
    {
        int min_set = 0;

        max_x = 0;
        max_y = 0;
        
        int src_offset = 0;
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                uint8_t bitmask = 1 << (7 - (unsigned int)x % 8);
                if(mask[src_offset + (unsigned int)x / 8] & bitmask)
                {
                    if(!min_set)
                    {
                        min_x = x;
                        min_y = y;
                        min_set = 1;
                    }
                    
                    min_y = hebitmap_min(min_y, y);
                    int y2 = y + 1;
                    max_y = hebitmap_max(max_y, y2);
                    
                    min_x = hebitmap_min(min_x, x);
                    int x2 = x + 1;
                    max_x = hebitmap_max(max_x, x2);
                }
            }
            
            src_offset += rowbytes;
        }
    }
    
    *bx = min_x; *by = min_y; *bw = max_x - min_x; *bh = max_y - min_y;
}
    
static void buffer_align_8_32(uint8_t *dst, uint8_t *src, int dst_rowbytes, int src_rowbytes, int x, int y, int width, int height, uint8_t fill_value)
{
    int src_offset = y * src_rowbytes;
    int dst_offset = 0;
    int aligned_width = dst_rowbytes * 8;
    
    for(int dst_y = 0; dst_y < height; dst_y++)
    {
        for(int dst_x = 0; dst_x < aligned_width; dst_x++)
        {
            uint8_t value = fill_value;
            if(dst_x < width)
            {
                int src_x = x + dst_x;
                uint8_t src_bitmask = 1 << (7 - (unsigned int)src_x % 8);
                value = (src[src_offset + (unsigned int)src_x / 8] & src_bitmask);
            }
            
            uint8_t dst_bitmask = 1 << (7 - (unsigned int)dst_x % 8);
            uint8_t *dst_ptr = dst + dst_offset + (unsigned int)dst_x / 8;
            
            if(value)
            {
                *dst_ptr |= dst_bitmask;
            }
            else
            {
                *dst_ptr &= ~dst_bitmask;
            }
        }
        
        src_offset += src_rowbytes;
        dst_offset += dst_rowbytes;
    }
}

uint32_t read_uint8(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint8_t value = buffer[0];
    *buffer_ptr += 1;
    return value;
}

uint32_t read_uint32(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint32_t value = buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
    *buffer_ptr += 4;
    return value;
}

static int lua_bitmapNew(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmap *bitmap = HEBitmapLoad(filename);
    if(bitmap)
    {
        _HEBitmap *prv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        prv->luaRef = luaRef;
    }
    else
    {
        playdate->lua->pushNil();
    }
    return 1;
}

static int lua_bitmapLoadHEB(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmap *bitmap = HEBitmapLoadHEB(filename);
    if(bitmap)
    {
        _HEBitmap *prv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        prv->luaRef = luaRef;
    }
    else
    {
        playdate->lua->pushNil();
    }
    return 1;
}

static int lua_bitmapGetSize(lua_State *L)
{
    HEBitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    playdate->lua->pushInt(bitmap->width);
    playdate->lua->pushInt(bitmap->height);
    return 2;
}

static int lua_bitmapColorAt(lua_State *L)
{
    HEBitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    LCDColor color = HEBitmapColorAt(bitmap, x, y);
    int lua_color;
    switch(color)
    {
        case kColorBlack:
            lua_color = 1;
            break;
        case kColorWhite:
            lua_color = 2;
            break;
        default:
            lua_color = 3;
            break;
    }
    playdate->lua->pushInt(lua_color);
    return 1;
}

static int lua_bitmapDraw(lua_State *L)
{
    HEBitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    HEBitmapDraw(bitmap, x, y);
    return 0;
}

static int lua_freeBitmap(lua_State *L)
{
    HEBitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    HEBitmapFree(bitmap);
    return 0;
}

static const lua_reg lua_bitmap[] = {
    { "new", lua_bitmapNew },
    { "loadHEB", lua_bitmapLoadHEB },
    { "getSize", lua_bitmapGetSize },
    { "_colorAt", lua_bitmapColorAt },
    { "draw", lua_bitmapDraw },
    { "__gc", lua_freeBitmap },
    { NULL, NULL }
};

static int lua_bitmapTableNew(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmapTable *bitmapTable = HEBitmapTableLoad(filename);
    if(bitmapTable)
    {
        _HEBitmapTable *prv = bitmapTable->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmapTable, lua_kBitmapTable, 0);
        prv->luaRef = luaRef;
    }
    else
    {
        playdate->lua->pushNil();
    }
    return 1;
}

static int lua_bitmapTableLoadHEBT(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmapTable *bitmapTable = HEBitmapTableLoadHEBT(filename);
    if(bitmapTable)
    {
        _HEBitmapTable *prv = bitmapTable->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmapTable, lua_kBitmapTable, 0);
        prv->luaRef = luaRef;
    }
    else
    {
        playdate->lua->pushNil();
    }
    return 1;
}


static int lua_bitmapAtIndex(lua_State *L)
{
    HEBitmapTable *bitmapTable = playdate->lua->getArgObject(1, lua_kBitmapTable, NULL);
    int index = playdate->lua->getArgInt(2);
    HEBitmap *bitmap = HEBitmapAtIndex(bitmapTable, index);
    if(bitmap)
    {
        _HEBitmap *bitmapPrv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        bitmapPrv->luaRef = luaRef;
    }
    else
    {
        playdate->lua->pushNil();
    }
    return 1;
}


static int lua_bitmapTableLength(lua_State *L)
{
    HEBitmapTable *bitmapTable = playdate->lua->getArgObject(1, lua_kBitmapTable, NULL);
    playdate->lua->pushInt(bitmapTable->length);
    return 1;
}

static int lua_freeBitmapTable(lua_State *L)
{
    HEBitmapTable *bitmapTable = playdate->lua->getArgObject(1, lua_kBitmapTable, NULL);
    HEBitmapTableFree(bitmapTable);
    return 0;
}

static const lua_reg lua_bitmapTable[] = {
    { "new", lua_bitmapTableNew },
    { "loadHEBT", lua_bitmapTableLoadHEBT },
    { "getBitmap", lua_bitmapAtIndex },
    { "getLength", lua_bitmapTableLength },
    { "__gc", lua_freeBitmapTable },
    { NULL, NULL }
};

static int lua_setClipRect(lua_State *L)
{
    int x = playdate->lua->getArgInt(1);
    int y = playdate->lua->getArgInt(2);
    int width = playdate->lua->getArgInt(3);
    int height = playdate->lua->getArgInt(4);
    HEBitmapSetClipRect(x, y, width, height);
    return 0;
}

static int lua_clearClipRect(lua_State *L)
{
    HEBitmapClearClipRect();
    return 0;
}

void HEBitmapInit(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    HEBitmapClearClipRect();
    
    if(enableLua)
    {
        playdate->lua->addFunction(lua_setClipRect, "hebitmap.graphics.setClipRect", NULL);
        playdate->lua->addFunction(lua_clearClipRect, "hebitmap.graphics.clearClipRect", NULL);
        playdate->lua->registerClass(lua_kBitmap, lua_bitmap, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kBitmapTable, lua_bitmapTable, NULL, 0, NULL);
    }
}
