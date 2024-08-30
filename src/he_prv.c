//
//  hebitmap_prv.c
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "he_prv.h"

static PlaydateAPI *playdate;

HEGraphicsContext *he_graphics_context;

static void _grid_resize(HEGrid *grid, HERectF rect, float size);

//
// Vec
//
HEVec2 he_vec2_new(float x, float y)
{
    return (HEVec2){
        .x = x,
        .y = y
    };
}
HEVec2 he_vec2_zero(void)
{
    return he_vec2_new(0, 0);
}

HEVec2i he_vec2i_new(int x, int y)
{
    return (HEVec2i){
        .x = x,
        .y = y
    };
}
HEVec2i he_vec2i_zero(void)
{
    return he_vec2i_new(0, 0);
}

//
// Rect
//
HERect he_rect_new(int x, int y, int width, int height)
{
    return (HERect){
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };
}

HERect he_rect_zero(void)
{
    return he_rect_new(0, 0, 0, 0);
}

int he_rect_intersects(HERect rect1, HERect rect2)
{
    return (rect1.x < (rect2.x + rect2.width) &&
            rect2.x < (rect1.x + rect1.width) &&
            rect1.y < (rect2.y + rect2.height) &&
            rect2.y < (rect1.y + rect1.height));
}

HERect he_rect_intersection(HERect clipRect, HERect rect)
{
    clipRect.width = he_max(clipRect.width, 0);
    clipRect.height = he_max(clipRect.height, 0);
    
    rect.width = he_max(rect.width, 0);
    rect.height = he_max(rect.height, 0);
    
    int clipRight = clipRect.x + clipRect.width;
    int clipBottom = clipRect.y + clipRect.height;
    
    if(rect.x < clipRect.x)
    {
        int new_width = rect.width - (clipRect.x - rect.x);
        rect.width = he_max(new_width, 0);
        rect.x = clipRect.x;
    }
    if(rect.y < clipRect.y)
    {
        int new_height = rect.height - (clipRect.y - rect.y);
        rect.height = he_max(new_height, 0);
        rect.y = clipRect.y;
    }
    if((rect.x + rect.width) > clipRight)
    {
        rect.width = clipRight - rect.x;
    }
    if((rect.y + rect.height) > clipBottom)
    {
        rect.height = clipBottom - rect.y;
    }
    
    return rect;
}

HERectF he_rectf_new(float x, float y, float width, float height)
{
    return (HERectF){
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };
}

HERectF he_rectf_from_rect(HERect rect)
{
    return (HERectF){
        .x = rect.x,
        .y = rect.y,
        .width = rect.width,
        .height = rect.height
    };
}

HERectF he_rectf_zero(void)
{
    return he_rectf_new(0, 0, 0, 0);
}

int he_rectf_equals(HERectF rect1, HERectF rect2)
{
    return (rect1.x == rect2.x && rect1.y == rect2.y && rect1.width == rect2.width && rect1.height == rect2.height);
}

HERectF he_rectf_sum(HERectF rect1, HERectF rect2)
{
    return he_rectf_new(rect1.x - rect2.width * 0.5f, rect1.y - rect2.height * 0.5f, rect1.width + rect2.width, rect1.height + rect2.height);
}

HERectF he_rectf_max(HERectF rect1, HERectF rect2)
{
    float x1 = fminf(rect1.x, rect2.x);
    float x2 = fmaxf(rect1.x + rect1.width, rect2.x + rect2.width);
    float y1 = fminf(rect1.y, rect2.y);
    float y2 = fmaxf(rect1.y + rect1.height, rect2.y + rect2.height);
    return he_rectf_new(x1, y1, x2 - x1, y2 - y1);
}

int he_rectf_intersects(HERectF rect1, HERectF rect2)
{
    return (rect1.x < (rect2.x + rect2.width) &&
            rect2.x < (rect1.x + rect1.width) &&
            rect1.y < (rect2.y + rect2.height) &&
            rect2.y < (rect1.y + rect1.height));
}

int he_rectf_contains(HERectF rect, float x, float y)
{
    return x >= rect.x && x <= (rect.x + rect.width) && y >= rect.y && y <= (rect.y + rect.height);
}

HEVec2 he_rectf_center(HERectF rect)
{
    return he_vec2_new(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
}

int he_point_in_corner(HEVec2 point, HERectF rect, int cornerIndex, float cornerSize)
{
    switch (cornerIndex)
    {
        case 0:
            return point.y < (rect.y + cornerSize) && point.x < (rect.x + cornerSize);
            break;
        case 1:
            return point.y < (rect.y + cornerSize) && point.x > (rect.x + rect.width - cornerSize);
            break;
        case 2:
            return point.y > (rect.y + rect.height - cornerSize) && point.x > (rect.x + rect.width - cornerSize);
            break;
        case 3:
            return point.y > (rect.y + rect.height - cornerSize) && point.x < (rect.x + cornerSize);
            break;
        default:
            break;
    }
    return 0;
}

//
// Array
//
HEArray* he_array_new(void)
{
    HEArray *array = playdate->system->realloc(NULL, sizeof(HEArray));
    array->items = NULL;
    array->length = 0;
    return array;
}

void he_array_push(HEArray *array, void *item)
{
    array->length++;
    array->items = playdate->system->realloc(array->items, array->length * sizeof(void*));
    array->items[array->length - 1] = item;
}

void he_array_remove(HEArray *array, int index)
{
    if(index >= 0 && index < array->length)
    {
        for(int i = index; i < (array->length - 1); i++)
        {
            array->items[i] = array->items[i + 1];
        }
        
        array->length--;
        
        array->items = playdate->system->realloc(array->items, array->length * sizeof(void*));
    }
}

int he_array_index_of(HEArray *array, void *item)
{
    for(int i = 0; i < array->length; i++)
    {
        if(array->items[i] == item)
        {
            return i;
        }
    }
    
    return -1;
}

void he_array_clear(HEArray *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    array->items = NULL;
    array->length = 0;
}

void he_array_free_container(HEArray *array)
{
    playdate->system->realloc(array, 0);
}

void he_array_free(HEArray *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    playdate->system->realloc(array, 0);
}

HEGenericArray* he_generic_array_new(int itemSize)
{
    HEGenericArray *array = playdate->system->realloc(NULL, sizeof(HEGenericArray));
    array->items = NULL;
    array->length = 0;
    array->itemSize = itemSize;
    return array;
}

uint8_t* he_generic_array_push(HEGenericArray *array, void *item)
{
    array->length++;
    array->items = playdate->system->realloc(array->items, array->length * array->itemSize);
    void *ptr = array->items + (array->length - 1) * array->itemSize;
    if(item)
    {
        memcpy(ptr, item, array->itemSize);
    }
    return ptr;
}

uint8_t* he_generic_array_get(HEGenericArray *array, int index)
{
    return array->items + index * array->itemSize;
}

void he_generic_array_clear(HEGenericArray *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    array->items = NULL;
    array->length = 0;
}

void he_generic_array_free_container(HEGenericArray *array)
{
    playdate->system->realloc(array, 0);
}

void he_generic_array_free(HEGenericArray *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    he_generic_array_free_container(array);
}

//
// Utils
//
void he_bitmap_clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, HERect clipRect)
{
    _HEBitmap *prv = bitmap->prv;
    
    *x1 = x;
    *offset_left = 0;

    if(x < clipRect.x)
    {
        *x1 = clipRect.x;
        *offset_left = clipRect.x - x;
    }
    
    *y1 = y;
    *offset_top = 0;
    
    if(y < clipRect.y)
    {
        *y1 = clipRect.y;
        *offset_top = clipRect.y - y;
    }
    
    int bitmap_x2 = x + prv->bw;
    int bitmap_y2 = y + prv->bh;
    
    int clip_x2 = clipRect.x + clipRect.width;
    int clip_y2 = clipRect.y + clipRect.height;

    *x2 = he_min(bitmap_x2, clip_x2);
    *y2 = he_min(bitmap_y2, clip_y2);
}

HEBitmap* HEBitmapAtIndex_1(HEBitmapTable *bitmapTable, int index)
{
    index--;
    if(index >= 0)
    {
        return HEBitmap_atIndex(bitmapTable, index);
    }
    return NULL;
}

#if HE_LUA_BINDINGS
//
// Lua Array
//
HELuaArray* he_lua_array_new(void *items, int length, int itemSize, HELuaArrayItem type)
{
    HELuaArray *luaArray = playdate->system->realloc(NULL, sizeof(HELuaArray));
    size_t size = length * itemSize;
    luaArray->items = playdate->system->realloc(NULL, size);
    memcpy(luaArray->items, items, size);
    luaArray->length = length;
    luaArray->itemSize = itemSize;
    luaArray->type = type;
    return luaArray;
}

uint8_t* he_lua_array_get(HELuaArray *array, int index)
{
    return &array->items[index * array->itemSize];
}

void he_lua_array_free(HELuaArray *luaArray)
{
    playdate->system->realloc(luaArray->items, 0);
    playdate->system->realloc(luaArray, 0);
}
#endif

//
// Grid
//
HEGridItem* he_grid_item_new(void *ptr)
{
    HEGridItem *item = playdate->system->realloc(NULL, sizeof(HEGridItem));
    item->ptr = ptr;
    item->cells = he_array_new();
    item->grid = NULL;
    item->rect = he_rectf_zero();
    item->row1 = 0;
    item->row2 = 0;
    item->col1 = 0;
    item->col2 = 0;
    item->fixed = 0;
    item->location = HEGridItemLocationNone;
    return item;
}

void grid_item_reset(HEGridItem *item)
{
    item->grid = NULL;
    item->rect = he_rectf_zero();
    item->row1 = 0;
    item->row2 = 0;
    item->col1 = 0;
    item->col2 = 0;
    item->location = HEGridItemLocationNone;
    // item->fixed is not resetted
    he_array_clear(item->cells);
}

void he_grid_item_free(HEGridItem *item)
{
    he_grid_remove(item);
    he_array_free(item->cells);
    playdate->system->realloc(item, 0);
}

HEGrid* he_grid_new(HERectF rect, float size)
{
    HEGrid *grid = playdate->system->realloc(NULL, sizeof(HEGrid));
    grid->cells = NULL;
    grid->fixedItems = he_array_new();
    
    _grid_resize(grid, rect, size);
    
    return grid;
}

static void _grid_resize(HEGrid *grid, HERectF rect, float size)
{
    grid->rect = rect;
    grid->size = size;
    grid->size_inv = 1.0f / size;
    grid->cols = ceilf(grid->rect.width / size);
    grid->rows = ceilf(grid->rect.height / size);
    
    grid->cells = playdate->system->realloc(grid->cells, grid->rows * grid->cols * sizeof(HEGridCell));
    
    for(int row = 0; row < grid->rows; row++)
    {
        for(int col = 0; col < grid->cols; col++)
        {
            HEGridCell *cell = &grid->cells[row * grid->cols + col];
            cell->items = he_array_new();
        }
    }
}

void he_grid_resize(HEGrid *grid, HERectF rect, float size)
{
    HEArray *items = he_array_new();
    
    for(int row = 0; row < grid->rows; row++)
    {
        for(int col = 0; col < grid->cols; col++)
        {
            HEGridCell *cell = &grid->cells[row * grid->cols + col];
            for(int i = 0; i < cell->items->length; i++)
            {
                HEGridItem *item = cell->items->items[i];
                he_array_push(items, item);
            }
        }
    }
    
    _grid_resize(grid, rect, size);
    
    for(int i = 0; i < items->length; i++)
    {
        HEGridItem *item = items->items[i];
        
        HERectF rect = item->rect;
        grid_item_reset(item);
        
        he_grid_add(grid, item, rect);
    }
    
    he_array_free(items);
}

int grid_get_row(HEGrid *grid, float y)
{
    int row = (y - grid->rect.y) * grid->size_inv;
    return he_max(0, he_min(grid->rows - 1, row));
}

int grid_get_col(HEGrid *grid, float x)
{
    int col = (x - grid->rect.x) * grid->size_inv;
    return he_max(0, he_min(grid->cols - 1, col));
}

void he_grid_add(HEGrid *grid, HEGridItem *item, HERectF rect)
{
    // update rect
    item->rect = rect;
    
    if(item->location == HEGridItemLocationFixed && item->fixed)
    {
        // Already in fixed array
        return;
    }
    
    int row1 = grid_get_row(grid, rect.y);
    int row2 = grid_get_row(grid, rect.y + rect.height);
    int col1 = grid_get_col(grid, rect.x);
    int col2 = grid_get_col(grid, rect.x + rect.width);

    if(item->location == HEGridItemLocationGrid && item->row1 == row1 && item->col1 == col1 && item->row2 == row2 && item->col2 == col2)
    {
        // Same grid position
        return;
    }
    
    he_grid_remove(item);
    
    item->row1 = row1;
    item->row2 = row2;
    item->col1 = col1;
    item->col2 = col2;
    item->grid = grid;
    
    if(item->fixed)
    {
        item->location = HEGridItemLocationFixed;
        he_array_push(grid->fixedItems, item);
    }
    else
    {
        item->location = HEGridItemLocationGrid;
        
        for(int row = row1; row <= row2; row++)
        {
            for(int col = col1; col <= col2; col++)
            {
                HEGridCell *cell = &grid->cells[row * grid->cols + col];
                he_array_push(item->cells, cell);
                he_array_push(cell->items, item);
            }
        }
    }
}

void he_grid_remove(HEGridItem *item)
{
    if(!item->grid)
    {
        return;
    }
    
    if(item->location == HEGridItemLocationGrid)
    {
        for(int i = 0; i < item->cells->length; i++)
        {
            HEGridCell *cell = item->cells->items[i];
            int index = he_array_index_of(cell->items, item);
            if(index >= 0)
            {
                he_array_remove(cell->items, index);
            }
        }
        he_array_clear(item->cells);
    }
    else if(item->location == HEGridItemLocationFixed)
    {
        int index = he_array_index_of(item->grid->fixedItems, item);
        if(index >= 0)
        {
            he_array_remove(item->grid->fixedItems, index);
        }
    }
    
    item->grid = NULL;
    item->location = HEGridItemLocationNone;
}

void he_grid_query(HEGrid *grid, HERectF queryRect, HEGridItem **items, int *count)
{
    *count = 0;
    
    int is_point = (queryRect.width == 0 && queryRect.height == 0);
    
    for(int i = 0; i < grid->fixedItems->length; i++)
    {
        HEGridItem *item = grid->fixedItems->items[i];
        items[(*count)++] = item;
    }
    
    int row1 = grid_get_row(grid, queryRect.y);
    int row2 = grid_get_row(grid, queryRect.y + queryRect.height);
    int col1 = grid_get_col(grid, queryRect.x);
    int col2 = grid_get_col(grid, queryRect.x + queryRect.width);
    
    for(int row = row1; row <= row2; row++)
    {
        for(int col = col1; col <= col2; col++)
        {
            HEGridCell *cell = &grid->cells[row * grid->cols + col];
            for(int i = 0; i < cell->items->length; i++)
            {
                HEGridItem *item = cell->items->items[i];
                if((!is_point && he_rectf_intersects(item->rect, queryRect)) || he_rectf_contains(item->rect, queryRect.x, queryRect.y))
                {
                    int found = 0;
                    for(int j = 0; j < *count; j++)
                    {
                        HEGridItem *item2 = items[j];
                        if(item2 == item)
                        {
                            found = 1;
                            break;
                        }
                    }
                    
                    if(!found)
                    {
                        items[(*count)++] = item;
                    }
                }
            }
        }
    }
}

void he_grid_free(HEGrid *grid)
{
    for(int row = 0; row < grid->rows; row++)
    {
        for(int col = 0; col < grid->cols; col++)
        {
            HEGridCell *cell = &grid->cells[row * grid->cols + col];
            for(int i = 0; i < cell->items->length; i++)
            {
                HEGridItem *item = cell->items->items[i];
                if(item->grid)
                {
                    grid_item_reset(item);
                }
            }
            he_array_free(cell->items);
        }
    }
    
    he_array_free(grid->fixedItems);
    playdate->system->realloc(grid->cells, 0);
    playdate->system->realloc(grid, 0);
}

#if HE_LUA_BINDINGS
//
// Lua utils
//
HELuaObject he_lua_object_new(void)
{
    return (HELuaObject){
        .ref = NULL,
        .refCount = 0
    };
}

void he_gc_add(HELuaObject *luaObject)
{
    if(luaObject->ref)
    {
        if(luaObject->refCount == 0)
        {
            playdate->lua->retainObject(luaObject->ref);
        }
        luaObject->refCount++;
    }
}

void he_gc_remove(HELuaObject *luaObject)
{
    if(luaObject->ref)
    {
        luaObject->refCount--;
        if(luaObject->refCount == 0)
        {
            playdate->lua->releaseObject(luaObject->ref);
        }
        else if(luaObject->refCount < 0)
        {
            luaObject->refCount = 0;
        }
    }
}
#endif

#if HE_LUA_BINDINGS
//
// Lua bindings
//
static int lua_reg_arrayIndex(lua_State *L)
{
    if(playdate->lua->indexMetatable())
    {
        return 1;
    }
    HELuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    int i = playdate->lua->getArgInt(2);
    if(i > 0 && i <= luaArray->length)
    {
#if HE_SPRITE_MODULE
        uint8_t *item = he_lua_array_get(luaArray, i - 1);
        
        switch(luaArray->type)
        {
            case HELuaArrayItemCollision: {
                HESpriteCollision *collision = (HESpriteCollision*)item;
                HESpriteCollision *collisionObject = playdate->system->realloc(NULL, sizeof(HESpriteCollision));
                memcpy(collisionObject, collision, sizeof(HESpriteCollision));
                playdate->lua->pushObject(collisionObject, lua_kSpriteCollision, 0);
                return 1;
            }
            case HELuaArrayItemSprite: {
                HESprite *sprite = (HESprite*)item;
                playdate->lua->pushObject(sprite, lua_kSprite, 0);
                return 1;
            }
        }
#endif
    }
    return 0;
}

static int lua_reg_arrayLen(lua_State *L)
{
    HELuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    playdate->lua->pushInt(luaArray->length);
    return 1;
}

static int lua_reg_arrayFree(lua_State *L)
{
    HELuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    he_lua_array_free(luaArray);
    return 0;
}

static const lua_reg lua_array[] = {
    { "__index", lua_reg_arrayIndex },
    { "__len", lua_reg_arrayLen },
    { "__gc", lua_reg_arrayFree },
    { NULL, NULL }
};
#endif

void he_prv_init(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    
#if HE_LUA_BINDINGS
    if(enableLua)
    {
        playdate->lua->registerClass(lua_kArray, lua_array, NULL, 0, NULL);
    }
#endif
}
