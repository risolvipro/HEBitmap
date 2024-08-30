//
//  he_bitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#include "he_bitmap.h"
#include "he_api.h"
#include "he_prv.h"

#include "he_bitmap_draw.h" // Opaque
#define HE_BITMAP_MASK
#include "he_bitmap_draw.h" // Mask
#undef HE_BITMAP_MASK

static PlaydateAPI *playdate;

static HEBitmap* HEBitmap_fromBuffer(uint8_t *buffer, int isOwner);

static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh);
static void allocation_failed(void);

static void buffer_align_8_32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int x, int y, int width, int height, uint8_t fill_value);
static uint32_t read_uint8(uint8_t **buffer_ptr);
static uint32_t read_uint32(uint8_t **buffer_ptr);

//
// Bitmap
//
HEBitmap* HEBitmap_base(void)
{
    HEBitmap *bitmap = playdate->system->realloc(NULL, sizeof(HEBitmap));
    
    _HEBitmap *prv = playdate->system->realloc(NULL, sizeof(_HEBitmap));
    bitmap->prv = prv;
    
    prv->buffer = NULL;
    prv->isOwner = 0;
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

#if HE_LUA_BINDINGS
    prv->luaObject = he_lua_object_new();
    prv->luaTableRef = NULL;
#endif
    
    return bitmap;
}

HEBitmap* HEBitmap_load(const char *filename)
{
    LCDBitmap *lcd_bitmap = playdate->graphics->loadBitmap(filename, NULL);
    if(lcd_bitmap)
    {
        HEBitmap *bitmap = HEBitmap_fromLCDBitmap(lcd_bitmap);
        playdate->graphics->freeBitmap(lcd_bitmap);
        return bitmap;
    }
    return NULL;
}

HEBitmap* _HEBitmap_fromLCDBitmap(LCDBitmap *lcd_bitmap, int isOwner)
{
    int width, height, rowbytes;
    uint8_t *lcd_mask, *lcd_data;
    playdate->graphics->getBitmapData(lcd_bitmap, &width, &height, &rowbytes, &lcd_mask, &lcd_data);
    
    if(!lcd_data)
    {
        return NULL;
    }
    
    int bx, by, bw, bh;
    get_bounds(lcd_mask, rowbytes, width, height, &bx, &by, &bw, &bh);
    
    int rowbytes_aligned = ((bw + 31) / 32) * 4;
    size_t data_size = rowbytes_aligned * bh;
    
    uint8_t *data = playdate->system->realloc(NULL, data_size);
    
    if(!data)
    {
        allocation_failed();
        return NULL;
    }
    
    uint8_t *mask = NULL;
    
    if(lcd_mask)
    {
        mask = playdate->system->realloc(NULL, data_size);
        if(!mask)
        {
            allocation_failed();
            playdate->system->realloc(data, 0);
            return NULL;
        }
    }
    
    HEBitmap *bitmap = HEBitmap_base();
    _HEBitmap *prv = bitmap->prv;
    
    prv->isOwner = isOwner;
    prv->freeData = 1;
    
    prv->bx = bx;
    prv->by = by;
    prv->bw = bw;
    prv->bh = bh;

    bitmap->width = width;
    bitmap->height = height;
    
    prv->rowbytes = rowbytes_aligned;

    prv->data = data;
    buffer_align_8_32(prv->data, lcd_data, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
        
    prv->mask = mask;
    if(prv->mask)
    {
        buffer_align_8_32(prv->mask, lcd_mask, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
    }
    
    return bitmap;
}

HEBitmap* HEBitmap_fromLCDBitmap(LCDBitmap *lcd_bitmap)
{
    return _HEBitmap_fromLCDBitmap(lcd_bitmap, 1);
}

HEBitmap* HEBitmap_loadHEB(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(NULL, file_size);
    if(buffer)
    {
        playdate->file->read(file, buffer, file_size);
    }
    playdate->file->close(file);
    
    if(!buffer)
    {
        allocation_failed();
        return NULL;
    }
    
    return HEBitmap_fromBuffer(buffer, 1);
}

static void decompress(uint8_t *dst, uint8_t **buffer, size_t len)
{
    uint32_t i = 0;
    
    while(i < len)
    {
        uint8_t count = **buffer;
        (*buffer)++;
        uint8_t data = **buffer;
        (*buffer)++;
        uint32_t end = i + count;
        while(i < end)
        {
            dst[i++] = data;
        }
    }
}

static HEBitmap* HEBitmap_fromBuffer(uint8_t *buffer, int isOwner)
{
    HEBitmap *bitmap = HEBitmap_base();
    _HEBitmap *prv = bitmap->prv;
    
    prv->isOwner = isOwner;
    
    uint8_t *buffer_ptr = buffer;
    
    // read version
    uint32_t version = read_uint32(&buffer_ptr);
    
    bitmap->width = read_uint32(&buffer_ptr);
    bitmap->height = read_uint32(&buffer_ptr);
    prv->bx = read_uint32(&buffer_ptr);
    prv->by = read_uint32(&buffer_ptr);
    prv->bw = read_uint32(&buffer_ptr);
    prv->bh = read_uint32(&buffer_ptr);
    prv->rowbytes = read_uint32(&buffer_ptr);
    
    uint8_t has_mask = read_uint8(&buffer_ptr);
    
    int compressed = 0;
    if(version >= 3)
    {
        // Version 3 supports compression
        compressed = read_uint8(&buffer_ptr);
    }
    
    if(version >= 2)
    {
        // Version 2 supports padding
        uint32_t padding_len = read_uint32(&buffer_ptr);
        buffer_ptr += padding_len;
    }
        
    if(compressed)
    {
        prv->freeData = 1;
        
        size_t data_size = prv->rowbytes * prv->bh;
        
        prv->data = playdate->system->realloc(NULL, data_size);
        decompress(prv->data, &buffer_ptr, data_size);
        
        if(has_mask)
        {
            prv->mask = playdate->system->realloc(NULL, data_size);
            decompress(prv->mask, &buffer_ptr, data_size);
        }
        
        if(isOwner)
        {
            playdate->system->realloc(buffer, 0);
        }
    }
    else
    {
        prv->freeData = isOwner;
        prv->buffer = buffer;
        prv->data = buffer_ptr;
        
        if(has_mask)
        {
            buffer_ptr += prv->rowbytes * prv->bh;
            prv->mask = buffer_ptr;
        }
    }
    
    return bitmap;
}

void HEBitmap_draw(HEBitmap *bitmap, int x, int y)
{
    if(((_HEBitmap*)bitmap->prv)->mask)
    {
        HEBitmap_drawMask(playdate, bitmap, x, y);
    }
    else
    {
        HEBitmap_drawOpaque(playdate, bitmap, x, y);
    }
}

LCDColor HEBitmap_colorAt(HEBitmap *bitmap, int x, int y)
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

void HEBitmap_getData(HEBitmap *bitmap, uint8_t **data, uint8_t **mask, int *rowbytes, int *bx, int *by, int *bw, int *bh)
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

void _HEBitmap_free(HEBitmap *bitmap)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(prv->freeData)
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

void HEBitmap_free(HEBitmap *bitmap)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(!prv->isOwner)
    {
#if HE_LUA_BINDINGS
        if(prv->bitmapTable)
        {
            _HEBitmapTable *_bitmapTable = prv->bitmapTable->prv;
            he_gc_remove(&_bitmapTable->luaObject);
        }
#endif
        return;
    }
    
    _HEBitmap_free(bitmap);
}

//
// Bitmap table
//
HEBitmapTable* HEBitmapTable_base(void)
{
    HEBitmapTable *bitmapTable = playdate->system->realloc(NULL, sizeof(HEBitmapTable));
    _HEBitmapTable *prv = playdate->system->realloc(NULL, sizeof(_HEBitmapTable));
    bitmapTable->prv = prv;
    
    bitmapTable->length = 0;
    prv->buffer = NULL;
    prv->bitmaps = NULL;
    
#if HE_LUA_BINDINGS
    prv->luaObject = he_lua_object_new();
#endif
    return bitmapTable;
}

HEBitmapTable* HEBitmapTable_load(const char *filename)
{
    LCDBitmapTable *lcd_bitmapTable = playdate->graphics->loadBitmapTable(filename, NULL);
    if(lcd_bitmapTable)
    {
        HEBitmapTable *bitmapTable = HEBitmapTable_fromLCDBitmapTable(lcd_bitmapTable);
        playdate->graphics->freeBitmapTable(lcd_bitmapTable);
        return bitmapTable;
    }
    return NULL;
}

HEBitmapTable* HEBitmapTable_fromLCDBitmapTable(LCDBitmapTable *lcd_bitmapTable)
{
    HEBitmapTable *bitmapTable = HEBitmapTable_base();
    _HEBitmapTable *prv = bitmapTable->prv;

    int length;
    playdate->graphics->getBitmapTableInfo(lcd_bitmapTable, &length, NULL);
    bitmapTable->length = length;
    
    prv->bitmaps = playdate->system->realloc(NULL, length * sizeof(HEBitmap*));
    
    for(int i = 0; i < length; i++)
    {
        prv->bitmaps[i] = NULL;
    }
    
    int valid = 1;
    
    for(int i = 0; i < length; i++)
    {
        LCDBitmap *lcd_bitmap = playdate->graphics->getTableBitmap(lcd_bitmapTable, i);
        HEBitmap *bitmap = _HEBitmap_fromLCDBitmap(lcd_bitmap, 0);
        if(bitmap)
        {
            _HEBitmap *_bitmap = bitmap->prv;
            _bitmap->bitmapTable = bitmapTable;
            prv->bitmaps[i] = bitmap;
        }
        else
        {
            valid = 0;
            break;
        }
    }
    
    if(!valid)
    {
        HEBitmapTable_free(bitmapTable);
        return NULL;
    }
    
    return bitmapTable;
}

HEBitmapTable* HEBitmapTable_loadHEBT(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(NULL, file_size);
    if(buffer)
    {
        playdate->file->read(file, buffer, file_size);
    }
    playdate->file->close(file);
    
    if(!buffer)
    {
        allocation_failed();
        return NULL;
    }
    
    HEBitmapTable *bitmapTable = HEBitmapTable_base();
    _HEBitmapTable *prv = bitmapTable->prv;
        
    uint8_t *buffer_ptr = buffer;
    
    // read version
    uint32_t version = read_uint32(&buffer_ptr);
    
    uint32_t length = read_uint32(&buffer_ptr);
    prv->bitmaps = playdate->system->realloc(NULL, length * sizeof(HEBitmap*));
    bitmapTable->length = length;
    
    int compressed = 0;
    if(version >= 3)
    {
        // version 3 supports compression
        compressed = read_uint8(&buffer_ptr);
    }
    
    if(version >= 2)
    {
        // version 2 supports padding
        uint32_t padding_len = read_uint32(&buffer_ptr);
        buffer_ptr += padding_len;
    }
    
    for(unsigned int i = 0; i < length; i++)
    {
        uint32_t bitmap_size = read_uint32(&buffer_ptr);
        HEBitmap *bitmap = HEBitmap_fromBuffer(buffer_ptr, 0);
        _HEBitmap *_bitmap = bitmap->prv;
        _bitmap->bitmapTable = bitmapTable;
        
        prv->bitmaps[i] = bitmap;
        
        buffer_ptr += bitmap_size;
    }
    
    if(compressed)
    {
        prv->buffer = buffer;
    }
    else
    {
        playdate->system->realloc(buffer, 0);
    }
    
    return bitmapTable;
}


HEBitmap* HEBitmap_atIndex(HEBitmapTable *bitmapTable, unsigned int index)
{
    _HEBitmapTable *prv = bitmapTable->prv;
    
    if(index < bitmapTable->length)
    {
        HEBitmap *bitmap = prv->bitmaps[index];
#if HE_LUA_BINDINGS
        _HEBitmap *_bitmap = bitmap->prv;
        // check if table is a Lua object
        if(prv->luaObject.ref)
        {
            if(!_bitmap->luaTableRef)
            {
                _bitmap->luaTableRef = prv->luaObject.ref;
                he_gc_add(&prv->luaObject);
            }
        }
#endif
        return bitmap;
    }
    
    return NULL;
}

void HEBitmapTable_free(HEBitmapTable *bitmapTable)
{
    _HEBitmapTable *prv = bitmapTable->prv;
    
    for(unsigned int i = 0; i < bitmapTable->length; i++)
    {
        HEBitmap *bitmap = prv->bitmaps[i];
        if(bitmap)
        {
            _HEBitmap_free(bitmap);
        }
    }
    playdate->system->realloc(prv->bitmaps, 0);
    
    if(prv->buffer)
    {
        playdate->system->realloc(prv->buffer, 0);
    }
    
    playdate->system->realloc(prv, 0);
    playdate->system->realloc(bitmapTable, 0);
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

static uint32_t read_uint8(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint8_t value = buffer[0];
    *buffer_ptr += 1;
    return value;
}

static uint32_t read_uint32(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint32_t value = buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
    *buffer_ptr += 4;
    return value;
}

static void allocation_failed(void)
{
    playdate->system->logToConsole("HEBitmap: cannot allocate data");
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
                    
                    min_y = he_min(min_y, y);
                    int y2 = y + 1;
                    max_y = he_max(max_y, y2);
                    
                    min_x = he_min(min_x, x);
                    int x2 = x + 1;
                    max_x = he_max(max_x, x2);
                }
            }
            
            src_offset += rowbytes;
        }
    }
    
    *bx = min_x; *by = min_y; *bw = max_x - min_x; *bh = max_y - min_y;
}

#if HE_LUA_BINDINGS
//
// Lua bindings
//
static int lua_bitmapNew(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmap *bitmap = HEBitmap_load(filename);
    if(bitmap)
    {
        _HEBitmap *prv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        prv->luaObject.ref = luaRef;
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
    HEBitmap *bitmap = HEBitmap_loadHEB(filename);
    if(bitmap)
    {
        _HEBitmap *prv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        prv->luaObject.ref = luaRef;
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
    int x = playdate->lua->getArgFloat(2);
    int y = playdate->lua->getArgFloat(3);
    LCDColor color = HEBitmap_colorAt(bitmap, x, y);
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
    int x = playdate->lua->getArgFloat(2);
    int y = playdate->lua->getArgFloat(3);
    HEBitmap_draw(bitmap, x, y);
    return 0;
}

static int lua_bitmapFree(lua_State *L)
{
    HEBitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    HEBitmap_free(bitmap);
    return 0;
}

static const lua_reg lua_bitmap[] = {
    { "new", lua_bitmapNew },
    { "loadHEB", lua_bitmapLoadHEB },
    { "getSize", lua_bitmapGetSize },
    { "_colorAt", lua_bitmapColorAt },
    { "draw", lua_bitmapDraw },
    { "__gc", lua_bitmapFree },
    { NULL, NULL }
};

static int lua_bitmapTableNew(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    HEBitmapTable *bitmapTable = HEBitmapTable_load(filename);
    if(bitmapTable)
    {
        _HEBitmapTable *prv = bitmapTable->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmapTable, lua_kBitmapTable, 0);
        prv->luaObject.ref = luaRef;
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
    HEBitmapTable *bitmapTable = HEBitmapTable_loadHEBT(filename);
    if(bitmapTable)
    {
        _HEBitmapTable *prv = bitmapTable->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmapTable, lua_kBitmapTable, 0);
        prv->luaObject.ref = luaRef;
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
    int index = playdate->lua->getArgFloat(2);
    
    HEBitmap *bitmap = HEBitmapAtIndex_1(bitmapTable, index);
    if(bitmap)
    {
        _HEBitmap *_bitmap = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        _bitmap->luaObject.ref = luaRef;
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

static int lua_bitmapTableFree(lua_State *L)
{
    HEBitmapTable *bitmapTable = playdate->lua->getArgObject(1, lua_kBitmapTable, NULL);
    HEBitmapTable_free(bitmapTable);
    return 0;
}

static const lua_reg lua_bitmapTable[] = {
    { "new", lua_bitmapTableNew },
    { "loadHEBT", lua_bitmapTableLoadHEBT },
    { "getBitmap", lua_bitmapAtIndex },
    { "getLength", lua_bitmapTableLength },
    { "__gc", lua_bitmapTableFree },
    { NULL, NULL }
};
#endif

void he_bitmap_init(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    
#if HE_LUA_BINDINGS
    if(enableLua)
    {
        playdate->lua->registerClass(lua_kBitmap, lua_bitmap, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kBitmapTable, lua_bitmapTable, NULL, 0, NULL);
    }
#endif
}
