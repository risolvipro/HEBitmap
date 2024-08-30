//
//  he_api_prv.h
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#ifndef he_prv_h
#define he_prv_h

#include "he_api.h"
#include "he_foundation.h"

#if HE_LUA_BINDINGS
#define lua_kBitmap "he.bitmap"
#define lua_kBitmapTable "he.bitmaptable"
#define lua_kSprite "he._sprite"
#define lua_kSpritePublic "he.sprite"
#define lua_kSpriteCollision "he.spritecollision"
#define lua_kGraphics "he.graphics"
#define lua_kArray "he.array"
#endif

typedef struct {
    int x;
    int y;
} HEVec2i;

typedef struct {
    void **items;
    int length;
} HEArray;

#if HE_LUA_BINDINGS
typedef enum {
    HELuaArrayItemCollision,
    HELuaArrayItemSprite
} HELuaArrayItem;

typedef struct {
    uint8_t *items;
    int length;
    int itemSize;
    HELuaArrayItem type;
} HELuaArray;
#endif

#if HE_LUA_BINDINGS
typedef struct {
    LuaUDObject *ref;
    int refCount;
} HELuaObject;
#endif

typedef struct HEGrid HEGrid;

typedef enum {
    HEGridItemLocationNone,
    HEGridItemLocationGrid,
    HEGridItemLocationFixed
} HEGridItemLocation;

typedef struct {
    void *ptr;
    HEArray *cells;
    HEGrid *grid;
    HERectF rect;
    int row1;
    int row2;
    int col1;
    int col2;
    int fixed;
    HEGridItemLocation location;
} HEGridItem;

typedef struct {
    HEArray *items;
} HEGridCell;

typedef struct HEGrid {
    HEGridCell *cells;
    HEArray *fixedItems;
    HERectF rect;
    float size;
    float size_inv;
    int rows;
    int cols;
} HEGrid;

typedef struct {
    uint8_t *data;
    uint8_t *mask;
    int rowbytes;
    int bx;
    int by;
    int bw;
    int bh;
    uint8_t *buffer;
    int isOwner;
    int freeData;
    HEBitmapTable *bitmapTable;
#if HE_LUA_BINDINGS
    LuaUDObject *luaTableRef;
    HELuaObject luaObject;
#endif
} _HEBitmap;

typedef struct {
    HEBitmap **bitmaps;
    uint8_t *buffer;
#if HE_LUA_BINDINGS
    HELuaObject luaObject;
#endif
} _HEBitmapTable;

typedef struct {
    uint8_t *items;
    int length;
    int itemSize;
} HEGenericArray;

typedef struct {
    HERect _clipRect;
    HERect clipRect;
} HEGraphicsContext;

extern HEGraphicsContext *he_graphics_context;

static const HERect gfx_screenRect = {
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};

HEVec2 he_vec2_new(float x, float y);
HEVec2 he_vec2_zero(void);
HEVec2i he_vec2i_new(int x, int y);
HEVec2i he_vec2i_zero(void);

HERect he_rect_new(int x, int y, int width, int height);
HERect he_rect_zero(void);
int he_rect_intersects(HERect rect1, HERect rect2);
HERect he_rect_intersection(HERect clipRect, HERect rect);
HERectF he_rectf_new(float x, float y, float width, float height);
HERectF he_rectf_zero(void);
HERectF he_rectf_from_rect(HERect rect);
int he_rectf_equals(HERectF rect1, HERectF rect2);
HERectF he_rectf_sum(HERectF rect1, HERectF rect2);
HERectF he_rectf_max(HERectF rect1, HERectF rect2);
int he_rectf_intersects(HERectF rect1, HERectF rect2);
int he_rectf_contains(HERectF rect, float x, float y);

HEVec2 he_rectf_center(HERectF rect);
int he_point_in_corner(HEVec2 point, HERectF rect, int cornerIndex, float cornerSize);

HEArray* he_array_new(void);
void he_array_push(HEArray *array, void *item);
void he_array_remove(HEArray *array, int index);
int he_array_index_of(HEArray *array, void *item);
void he_array_clear(HEArray *array);
void he_array_free_container(HEArray *array);
void he_array_free(HEArray *array);

HEGenericArray* he_generic_array_new(int itemSize);
uint8_t* he_generic_array_get(HEGenericArray *array, int index);
uint8_t* he_generic_array_push(HEGenericArray *array, void *item);
void he_generic_array_clear(HEGenericArray *array);
void he_generic_array_free_container(HEGenericArray *array);
void he_generic_array_free(HEGenericArray *array);

#if HE_LUA_BINDINGS
HELuaObject he_lua_object_new(void);
void he_gc_add(HELuaObject *luaObject);
void he_gc_remove(HELuaObject *luaObject);
#endif

void he_bitmap_clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, HERect clipRect);

HEBitmap* HEBitmapAtIndex_1(HEBitmapTable *bitmapTable, int index);

#if HE_LUA_BINDINGS
HELuaArray* he_lua_array_new(void *items, int length, int itemSize, HELuaArrayItem type);
uint8_t* he_lua_array_get(HELuaArray *array, int index);
void he_lua_array_free(HELuaArray *luaArray);
#endif

HEGridItem* he_grid_item_new(void *ptr);
void he_grid_item_free(HEGridItem *item);

HEGrid* he_grid_new(HERectF rect, float size);
void he_grid_add(HEGrid *grid, HEGridItem *item, HERectF rect);
void he_grid_remove(HEGridItem *item);
void he_grid_query(HEGrid *grid, HERectF rect, HEGridItem **items, int *count);
void he_grid_resize(HEGrid *grid, HERectF rect, float size);
void he_grid_free(HEGrid *grid);

static inline int he_min(const int a, const int b)
{
    return a < b ? a : b;
}

static inline int he_max(const int a, const int b)
{
    return a > b ? a : b;
}

static inline int he_signf(float d)
{
    if(d > 0)
    {
        return -1;
    }
    else if(d < 0)
    {
        return 1;
    }
    return 0;
}

static inline int he_nsign(int sign)
{
    if(sign != 0)
    {
        return -sign;
    }
    return 0;
}

static inline uint32_t bswap32(uint32_t n)
{
#if TARGET_PLAYDATE
    uint32_t r;
    __asm("rev %0, %1" : "=r"(r) : "r"(n));
    return r;
#else
    return (n >> 24) | ((n << 8) & 0xFF0000U) | (n << 24) | ((n >> 8) & 0x00FF00U);
#endif
}

#endif /* he_prv_h */
