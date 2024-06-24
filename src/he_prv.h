//
//  he_api_prv.h
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#ifndef hebitmap_prv_h
#define hebitmap_prv_h

#include "he_api.h"
#include "he_foundation.h"

#define HE_GFX_STACK_SIZE 1024

#define lua_kBitmap "he.bitmap"
#define lua_kBitmapTable "he.bitmaptable"
#define lua_kSprite "he._sprite"
#define lua_kSpritePublic "he.sprite"
#define lua_kSpriteCollision "he.spritecollision"
#define lua_kGraphics "he.graphics"
#define lua_kArray "he.array"

typedef struct {
    int x;
    int y;
} HEVec2i;

typedef struct {
    void **items;
    int length;
} HEArray;

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

typedef struct {
    LuaUDObject *ref;
    int refCount;
} HELuaObject;

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
    LuaUDObject *luaTableRef;
    HELuaObject luaObject;
} _HEBitmap;

typedef struct {
    HEBitmap **bitmaps;
    uint8_t *buffer;
    HELuaObject luaObject;
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

extern PlaydateAPI *playdate;
extern HEGraphicsContext gfx_stack[HE_GFX_STACK_SIZE];
extern int gfx_stack_index;
extern HEGraphicsContext *gfx_context;
extern const HERect gfx_screenRect;

HEVec2 vec2_new(float x, float y);
HEVec2 vec2_zero(void);
HEVec2i vec2i_new(int x, int y);
HEVec2i vec2i_zero(void);

HERect rect_new(int x, int y, int width, int height);
HERect rect_zero(void);
int rect_intersects(HERect rect1, HERect rect2);
HERect rect_intersection(HERect clipRect, HERect rect);
HERectF rectf_new(float x, float y, float width, float height);
HERectF rectf_zero(void);
HERectF rectf_from_rect(HERect rect);
int rectf_equals(HERectF rect1, HERectF rect2);
HERectF rectf_sum(HERectF rect1, HERectF rect2);
HERectF rectf_max(HERectF rect1, HERectF rect2);
int rectf_intersects(HERectF rect1, HERectF rect2);
int rectf_contains(HERectF rect, float x, float y);

HEVec2 rectf_center(HERectF rect);
int point_in_corner(HEVec2 point, HERectF rect, int cornerIndex, float cornerSize);

HEArray* array_new(void);
void array_push(HEArray *array, void *item);
void array_remove(HEArray *array, int index);
int array_index_of(HEArray *array, void *item);
void array_clear(HEArray *array);
void array_free_container(HEArray *array);
void array_free(HEArray *array);

HEGenericArray* generic_array_new(int itemSize);
uint8_t* generic_array_get(HEGenericArray *array, int index);
uint8_t* generic_array_push(HEGenericArray *array, void *item);
void generic_array_clear(HEGenericArray *array);
void generic_array_free_container(HEGenericArray *array);
void generic_array_free(HEGenericArray *array);

HELuaObject lua_object_new(void);
void gc_add(HELuaObject *luaObject);
void gc_remove(HELuaObject *luaObject);

void bitmap_clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, HERect clipRect);

HEBitmap* HEBitmapAtIndex_1(HEBitmapTable *bitmapTable, int index);

HELuaArray* lua_array_new(void *items, int length, int itemSize, HELuaArrayItem type);
uint8_t* lua_array_get(HELuaArray *array, int index);
void lua_array_free(HELuaArray *luaArray);

HEGridItem* grid_item_new(void *ptr);
void grid_item_free(HEGridItem *item);

HEGrid* grid_new(HERectF rect, float size);
void grid_add(HEGrid *grid, HEGridItem *item, HERectF rect);
void grid_remove(HEGridItem *item);
void grid_query(HEGrid *grid, HERectF rect, HEGridItem **items, int *count);
void grid_resize(HEGrid *grid, HERectF rect, float size);
void grid_free(HEGrid *grid);

static inline int he_min(const int a, const int b)
{
    return a < b ? a : b;
}

static inline int he_max(const int a, const int b)
{
    return a > b ? a : b;
}

static inline int signf(float d)
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

static inline int nsign(int sign)
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

#endif /* hebitmap_prv_h */
