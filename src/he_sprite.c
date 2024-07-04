//
//  he_sprite.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#include "he_sprite.h"
#include "he_prv.h"
#include <float.h>

#define HE_COLLISION_PAIR_MAX 257 // Prime number
#define HE_GRID_RESULT_MAX 2048

typedef enum {
    HESpriteTypeNone,
    HESpriteTypeBitmap,
    HESpriteTypeTileBitmap,
    HESpriteTypeCallback,
    HESpriteTypeLuaCallback
} HESpriteType;

typedef struct {
    HERectF rect;
    HERectF lastRect;
    HEVec2 center;
    HEVec2 lastCenter;
} HESpriteCollisionInstance;

typedef struct {
    HESpriteCollisionType value;
    int cacheEnabled;
    HESpriteCollisionTypeCallback *callback;
    int luaCallback;
    HESpriteCollisionType luaResult;
} HESpriteCollisionTypeInfo;

typedef struct {
    HESprite *target;
    float velocity;
    float refreshRate;
    float refreshElapsedTime;
    HEVec2 oldPosition;
    int hasOldPosition;
    HEVec2 offset;
} HESpriteFollow;

typedef struct {
    HESpriteType type;
    HEVec2 position;
    float width;
    float height;
    HEVec2 centerAnchor;
    HERectF collisionInnerRect;
    int hasCollisionInnerRect;
    HERectF lastCollisionRect;
    int hasLastCollisionRect;
    int visible;
    int hasClipRect;
    HERect clipRect;
    HESpriteClipRectReference clipRectReference;
    int z_index;
    int ignoresDrawOffset;
    int ignoresScreenClipRect;
    HERect screenRect;
    HERect screenClipRect;
    HEBitmap *bitmap;
    HEBitmap *tileBitmap;
    HEVec2i tileOffset;
    HESpriteCollisionInstance collisionInstance;
    HESpriteCollisionTypeInfo collisionType;
    int collisionsEnabled;
    HESpriteFollow follow;
    int fastCollisions;
    HEGridItem *collisionGridItem;
    HEGridItem *visibilityGridItem;
    int moving;
    int added;
    int index;
    HESpriteDrawCallback *drawCallback;
    HESpriteUpdateCallback *updateCallback;
    void *userdata;
    int luaUpdateCallback;
    LuaUDObject *luaRef;
} _HESprite;

typedef struct HECollisionPair HECollisionPair;

typedef struct HECollisionPair {
    HESprite *sprite1;
    HESprite *sprite2;
    HESpriteCollisionType collisionType;
    HECollisionPair *next;
} HECollisionPair;

static void _HESprite_remove(HESprite *sprite, int index, int all);

static HERectF HESprite_measureCollisionRect(HESprite *sprite);
static HERect HESprite_measureScreenRect(HESprite *sprite);
static HERectF HESprite_measureRect(HESprite *sprite);
static void HESprite_unfollow(HESprite *sprite);
static void HESprite_updateCollisionInstance(HESprite *sprite);
static void resolveMinimumDisplacement(HESpriteCollisionType type, HESprite *sprite1, HESprite *sprite2, int resolve, HEGenericArray *results);
static HESpriteCollisionType collisionTypeForSprites(HESprite *sprite1, HESprite *sprite2);
static int segment_rect_intersection(float x1, float y1, float x2, float y2, HERectF rect, float *t1_out, float *t2_out, float *nx1_out, float *ny1_out, float *nx2_out, float *ny2_out);

static HECollisionPair* collision_pair_get(HESprite *sprite1, HESprite *sprite2);
static HECollisionPair* collision_pair_insert(HESprite *sprite1, HESprite *sprite2);
static void remove_collision_pairs(HESprite *sprite);
static void clear_collision_pairs(void);

static void HESprite_updateCollisions(HESprite *sprite);
static void HESprite_collisionsGridRemove(HESprite *sprite);
static void HESprite_collisionsGridAdd(HESprite *sprite);
static void HESprite_updateVisibility(HESprite *sprite);
static void HESprite_visibilityGridRemove(HESprite *sprite);
static void HESprite_visibilityGridAdd(HESprite *sprite);

static void clearVisibleSprites(void);
static void clearSpriteCollisions(void);

static PlaydateAPI *playdate;

static HERect gfx_spriteScreenClipRect = {
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};
static HEVec2i gfx_spriteDrawOffset = {
    .x = 0,
    .y = 0
};

static HEArray *sprites;
static HEArray *visibleSprites;
static HEGenericArray *spriteCollisions;
static HECollisionPair *collisionPairs[HE_COLLISION_PAIR_MAX] = {NULL};
static HEGridItem *grid_default_results[HE_GRID_RESULT_MAX] = {0};
static HEGridItem *grid_query_results[HE_GRID_RESULT_MAX] = {0};
static HEArray *movingSprites;
static HEArray *followSprites;
static const float cornerTolerance = 1;
static HEGrid *collisionsGrid;
static HEGrid *visibilityGrid;

//
// Sprite
//
HESprite* HESprite_new(void)
{
    HESprite *sprite = playdate->system->realloc(NULL, sizeof(HESprite));
    _HESprite *prv = playdate->system->realloc(NULL, sizeof(_HESprite));
    sprite->prv = prv;
    
    prv->type = HESpriteTypeNone;
    prv->bitmap = NULL;
    prv->tileBitmap = NULL;
    prv->tileOffset = vec2i_zero();
    prv->position = vec2_zero();
    prv->width = 0;
    prv->height = 0;
    prv->centerAnchor = vec2_new(0.5, 0.5);
    prv->collisionInnerRect = rectf_zero();
    prv->hasCollisionInnerRect = 0;
    prv->lastCollisionRect = rectf_zero();
    prv->hasLastCollisionRect = 0;
    prv->visible = 1;
    prv->z_index = 0;
    prv->ignoresDrawOffset = 0;
    prv->ignoresScreenClipRect = 0;
    prv->clipRect = rect_zero();
    prv->hasClipRect = 0;
    prv->clipRectReference = HESpriteClipRectRelative;
    
    prv->collisionType.value = HESpriteCollisionTypeSlide;
    prv->collisionType.cacheEnabled = 0;
    prv->collisionType.callback = NULL;
    prv->collisionType.luaCallback = 0;
    prv->collisionType.luaResult = HESpriteCollisionTypeSlide;
    
    prv->collisionsEnabled = 0;
    prv->follow = (HESpriteFollow){
        .target = NULL,
        .velocity = 0,
        .refreshRate = 0,
        .refreshElapsedTime = 0,
        .oldPosition = vec2_zero(),
        .hasOldPosition = 0,
        .offset = vec2_zero()
    };
    prv->fastCollisions = 0;
    
    prv->collisionInstance.rect = rectf_zero();
    prv->collisionInstance.lastRect = rectf_zero();
    prv->collisionInstance.center = vec2_zero();
    prv->collisionInstance.lastCenter = vec2_zero();

    prv->screenRect = rect_zero();
    prv->screenClipRect = rect_zero();
    prv->collisionGridItem = grid_item_new(sprite);
    prv->visibilityGridItem = grid_item_new(sprite);
    prv->added = 0;
    prv->moving = 0;
    prv->updateCallback = NULL;
    prv->drawCallback = NULL;
    prv->userdata = NULL;
    prv->luaUpdateCallback = 0;
    prv->luaRef = NULL;
    
    return sprite;
}

int HESprite_add_ret(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(array_index_of(sprites, sprite) < 0)
    {
        int index = sprites->length;
        array_push(sprites, sprite);
        prv->added = 1;
        prv->index = index;
        
        HESprite_updateCollisions(sprite);
        HESprite_updateVisibility(sprite);
        
        return index;
    }
    return -1;
}

void HESprite_add(HESprite *sprite)
{
    HESprite_add_ret(sprite);
}

static void _HESprite_remove(HESprite *sprite, int index, int all)
{
    _HESprite *prv = sprite->prv;
    
    HESprite_collisionsGridRemove(sprite);
    HESprite_visibilityGridRemove(sprite);
    
    if(!all)
    {
        remove_collision_pairs(sprite);
        HESprite_unfollow(sprite);
        
        int moving_index = array_index_of(movingSprites, sprite);
        if(moving_index >= 0)
        {
            array_remove(movingSprites, moving_index);
        }
        
        int visible_index = array_index_of(visibleSprites, sprite);
        if(visible_index >= 0)
        {
            array_remove(visibleSprites, visible_index);
        }
        
        for(int i = 0; i < sprites->length; i++)
        {
            HESprite *sprite2 = sprites->items[i];
            _HESprite *_sprite2 = sprite2->prv;
            if(_sprite2->follow.target == sprite)
            {
                HESprite_unfollow(sprite2);
            }
            if(index >= 0 && index > prv->index)
            {
                _sprite2->index--;
            }
        }
        
        if(index >= 0)
        {
            array_remove(sprites, index);
        }
    }
    
    prv->added = 0;
    prv->index = -1;
    prv->hasLastCollisionRect = 0;
}

int HESprite_remove_ret(HESprite *sprite)
{
    int index = array_index_of(sprites, sprite);
    _HESprite_remove(sprite, index, 0);
    return index;
}

void HESprite_remove(HESprite *sprite)
{
    HESprite_remove_ret(sprite);
}

static void HESprite_resetType(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    prv->type = HESpriteTypeNone;
    prv->drawCallback = NULL;
    
#if HE_LUA_BINDINGS
    if(prv->bitmap)
    {
        _HEBitmap *_bitmap = prv->bitmap->prv;
        gc_remove(&_bitmap->luaObject);
    }
#endif
    prv->bitmap = NULL;
    
#if HE_LUA_BINDINGS
    if(prv->tileBitmap)
    {
        _HEBitmap *_bitmap = prv->tileBitmap->prv;
        gc_remove(&_bitmap->luaObject);
    }
#endif
    prv->tileBitmap = NULL;
}

void HESprite_setEmpty(HESprite *sprite)
{
    HESprite_resetType(sprite);
}

void HESprite_setBitmap(HESprite *sprite, HEBitmap *bitmap)
{
    _HESprite *prv = sprite->prv;
    
    HESprite_resetType(sprite);
    
    if(bitmap)
    {
        prv->type = HESpriteTypeBitmap;
        prv->bitmap = bitmap;
        prv->width = bitmap->width;
        prv->height = bitmap->height;
        
        HESprite_updateCollisions(sprite);
        HESprite_updateVisibility(sprite);
        
#if HE_LUA_BINDINGS
        _HEBitmap *_bitmap = bitmap->prv;
        gc_add(&_bitmap->luaObject);
#endif
    }
}

void HESprite_setTileBitmap(HESprite *sprite, HEBitmap *bitmap)
{
    _HESprite *prv = sprite->prv;
    
    HESprite_resetType(sprite);
    
    if(bitmap)
    {
        prv->type = HESpriteTypeTileBitmap;
        prv->tileBitmap = bitmap;

#if HE_LUA_BINDINGS
        _HEBitmap *_bitmap = bitmap->prv;
        gc_add(&_bitmap->luaObject);
#endif
    }
}

void HESprite_setDrawCallback(HESprite *sprite, HESpriteDrawCallback *callback)
{
    _HESprite *prv = sprite->prv;
    
    HESprite_resetType(sprite);
    
    if(callback)
    {
        prv->type = HESpriteTypeCallback;
        prv->drawCallback = callback;
    }
}

void HESprite_setTileOffset(HESprite *sprite, int dx, int dy)
{
    _HESprite *prv = sprite->prv;
    prv->tileOffset.x = dx;
    prv->tileOffset.y = dy;
}

void HESprite_setCollisionType(HESprite *sprite, HESpriteCollisionType type)
{
    _HESprite *prv = sprite->prv;
    prv->collisionType.value = type;
}

void HESprite_setCollisionTypeCallback(HESprite *sprite, HESpriteCollisionTypeCallback *callback)
{
    _HESprite *prv = sprite->prv;
    prv->collisionType.callback = callback;
    prv->collisionType.value = HESpriteCollisionTypeSlide;
}

HESpriteCollisionType HESprite_getCollisionType(HESprite *sprite, int *isCallback)
{
    *isCallback = 0;
    _HESprite *prv = sprite->prv;
    if(prv->collisionType.luaCallback || prv->collisionType.callback)
    {
        *isCallback = 1;
    }
    return prv->collisionType.value;
}

void HESprite_cacheCollisionType(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->collisionType.cacheEnabled = flag;
    if(!flag)
    {
        remove_collision_pairs(sprite);
    }
}

int HESprite_collisionTypeIsBeingCached(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    return prv->collisionType.cacheEnabled;
}

void HESprite_invalidateCollisionType(HESprite *sprite)
{
    remove_collision_pairs(sprite);
}

HESpriteCollisionType HESprite_castCollisionType(int type)
{
    if(type == HESpriteCollisionTypeSlide || type == HESpriteCollisionTypeFreeze || type == HESpriteCollisionTypeOverlap || type == HESpriteCollisionTypeBounce || type == HESpriteCollisionTypeIgnore)
    {
        return type;
    }
    return HESpriteCollisionTypeFreeze;
}

void HESprite_setCollisionsEnabled(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->collisionsEnabled = flag;
    HESprite_updateCollisions(sprite);
}

void HESprite_setCollisionRect(HESprite *sprite, float x, float y, float width, float height)
{
    _HESprite *prv = sprite->prv;
    prv->collisionInnerRect.x = x;
    prv->collisionInnerRect.y = y;
    prv->collisionInnerRect.width = width;
    prv->collisionInnerRect.height = height;
    prv->hasCollisionInnerRect = 1;
    HESprite_updateCollisions(sprite);
}

void HESprite_clearCollisionRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    prv->hasCollisionInnerRect = 0;
    prv->collisionInnerRect = rectf_zero();
    HESprite_updateCollisions(sprite);
}

void HESprite_setVisible(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->visible = flag;
    HESprite_updateVisibility(sprite);
}

int HESprite_isVisible(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    return prv->visible;
}

int HESprite_isVisibleOnScreen(HESprite *sprite)
{
    int index = array_index_of(visibleSprites, sprite);
    return (index >= 0);
}

void HESprite_setZIndex(HESprite *sprite, int z)
{
    _HESprite *prv = sprite->prv;
    prv->z_index = z;
}

void HESprite_willMove(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(!prv->hasLastCollisionRect)
    {
        prv->lastCollisionRect = HESprite_measureCollisionRect(sprite);
        prv->hasLastCollisionRect = 1;
    }
    if(prv->added)
    {
        if(!prv->moving)
        {
            prv->moving = 1;
            array_push(movingSprites, sprite);
        }
    }
}

void HESprite_didMove(HESprite *sprite)
{
    HESprite_updateCollisions(sprite);
    HESprite_updateVisibility(sprite);
}

static void HESprite_collisionsGridAdd(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERectF rectMax = HESprite_measureCollisionRect(sprite);
    if(prv->hasLastCollisionRect)
    {
        rectMax = rectf_max(rectMax, prv->lastCollisionRect);
    }
    grid_add(collisionsGrid, prv->collisionGridItem, rectMax);
}

static void HESprite_collisionsGridRemove(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    grid_remove(prv->collisionGridItem);
}

static void HESprite_updateCollisions(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(!prv->added)
    {
        return;
    }
    if(prv->collisionsEnabled && !prv->ignoresDrawOffset)
    {
        HESprite_collisionsGridAdd(sprite);
    }
    else
    {
        HESprite_collisionsGridRemove(sprite);
    }
}

static void HESprite_visibilityGridAdd(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERectF rect = HESprite_measureRect(sprite);
    prv->visibilityGridItem->fixed = prv->ignoresDrawOffset;
    grid_add(visibilityGrid, prv->visibilityGridItem, rect);
}

static void HESprite_visibilityGridRemove(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    grid_remove(prv->visibilityGridItem);
}

static void HESprite_updateVisibility(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(!prv->added)
    {
        return;
    }
    if(prv->visible)
    {
        HESprite_visibilityGridAdd(sprite);
    }
    else
    {
        HESprite_visibilityGridRemove(sprite);
    }
}

void HESprite_resetMotion(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    prv->hasLastCollisionRect = 0;
    prv->moving = 0;
}

void HESprite_moveTo(HESprite *sprite, float x, float y)
{
    _HESprite *prv = sprite->prv;
    
    HESprite_willMove(sprite);
    
    prv->position.x = x;
    prv->position.y = y;
    
    HESprite_didMove(sprite);
}

void HESprite_unfollow(HESprite *sprite)
{
    _HESprite *_sprite = sprite->prv;
    _sprite->follow.target = NULL;
    int index = array_index_of(followSprites, sprite);
    if(index >= 0)
    {
        array_remove(followSprites, index);
    }
}

void HESprite_setFollowTarget(HESprite *sprite, HESprite *target)
{
    _HESprite *_sprite = sprite->prv;
    
    HESprite_unfollow(sprite);
    
    _sprite->follow.target = target;
    if(target)
    {
        array_push(followSprites, sprite);
    }
}

void HESprite_setFollowVelocity(HESprite *sprite, float velocity)
{
    _HESprite *prv = sprite->prv;
    prv->follow.velocity = velocity;
}

void HESprite_setFollowOffset(HESprite *sprite, float dx, float dy)
{
    _HESprite *prv = sprite->prv;
    prv->follow.offset.x = dx;
    prv->follow.offset.y = dy;
}

void HESprite_setFollowRefreshRate(HESprite *sprite, float rate)
{
    _HESprite *prv = sprite->prv;
    prv->follow.refreshRate = rate;
}

void HESprite_setFastCollisions(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->fastCollisions = flag;
}

void HESprite_setPosition(HESprite *sprite, float x, float y)
{
    _HESprite *prv = sprite->prv;
    
    prv->position.x = x;
    prv->position.y = y;
    
    // Not moving
    prv->hasLastCollisionRect = 0;
    
    HESprite_updateCollisions(sprite);
    HESprite_updateVisibility(sprite);
}

HEVec2 HESprite_getPosition(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    return prv->position;
}

HEVec2 HESprite_getCenterPosition(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    return vec2_new(prv->position.x - prv->width * prv->centerAnchor.x + prv->width * 0.5f, prv->position.y - prv->height * prv->centerAnchor.y + prv->height * 0.5f);
}

void HESprite_getSize(HESprite *sprite, float *width, float *height)
{
    _HESprite *prv = sprite->prv;
    *width = prv->width;
    *height = prv->height;
}

HESpriteCollision* HESprite_checkCollisions(HESprite *sprite, int *len)
{
    _HESprite *_sprite = sprite->prv;
    
    HEGenericArray *results = generic_array_new(sizeof(HESpriteCollision));
    
    HESpriteCollisionInstance *c1 = &_sprite->collisionInstance;
    HESprite_updateCollisionInstance(sprite);
    
    int count;
    grid_query(collisionsGrid, c1->rect, grid_query_results, &count);
    
    for(int i = 0; i < count; i++)
    {
        HEGridItem *item = grid_query_results[i];
        HESprite *sprite2 = item->ptr;
        _HESprite *_sprite2 = sprite2->prv;
        
        if(sprite == sprite2)
        {
            continue;
        }
        
        HESpriteCollisionType type = collisionTypeForSprites(sprite, sprite2);
        if(type == HESpriteCollisionTypeIgnore)
        {
            continue;
        }
        
        HESprite_updateCollisionInstance(sprite2);
        HESpriteCollisionInstance *c2 = &_sprite2->collisionInstance;
        
        if(rectf_intersects(c1->rect, c2->rect))
        {
            resolveMinimumDisplacement(type, sprite, sprite2, 0, results);
        }
    }
    
    HESpriteCollision *collisions = (HESpriteCollision*)results->items;
    *len = results->length;
    generic_array_free_container(results);
    return collisions;
}

void HESprite_setSize(HESprite *sprite, float width, float height)
{
    _HESprite *prv = sprite->prv;
    if(prv->type != HESpriteTypeBitmap)
    {
        prv->width = width;
        prv->height = height;
        
        HESprite_updateCollisions(sprite);
        HESprite_updateVisibility(sprite);
    }
}

void HESprite_setCenter(HESprite *sprite, float cx, float cy)
{
    _HESprite *prv = sprite->prv;
    
    prv->centerAnchor.x = cx;
    prv->centerAnchor.y = cy;
    
    HESprite_updateCollisions(sprite);
    HESprite_updateVisibility(sprite);
}

void HESprite_setNeedsCollisions(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(prv->collisionsEnabled)
    {
        HESprite_willMove(sprite);
        HESprite_didMove(sprite);
    }
}

void HESprite_setIgnoresDrawOffset(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->ignoresDrawOffset = flag;
    
    HESprite_updateCollisions(sprite);
    HESprite_updateVisibility(sprite);
}

void HESprite_setIgnoresScreenClipRect(HESprite *sprite, int flag)
{
    _HESprite *prv = sprite->prv;
    prv->ignoresScreenClipRect = flag;
}

void HESprite_setUpdateCallback(HESprite *sprite, HESpriteUpdateCallback *callback)
{
    _HESprite *prv = sprite->prv;
    prv->updateCallback = callback;
    prv->luaUpdateCallback = 0;
}

void HESprite_setUserdata(HESprite *sprite, void *userdata)
{
    _HESprite *prv = sprite->prv;
    prv->userdata = userdata;
}

void* HESprite_getUserdata(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    return prv->userdata;
}

void HESprite_free(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    
    int index = array_index_of(sprites, sprite);
    _HESprite_remove(sprite, index, 0);
    
#if HE_LUA_BINDINGS
    if(prv->bitmap)
    {
        _HEBitmap *_bitmap = prv->bitmap->prv;
        gc_remove(&_bitmap->luaObject);
    }
#endif
    
#if HE_LUA_BINDINGS
    if(prv->tileBitmap)
    {
        _HEBitmap *_bitmap = prv->tileBitmap->prv;
        gc_remove(&_bitmap->luaObject);
    }
#endif
    
    grid_item_free(prv->collisionGridItem);
    grid_item_free(prv->visibilityGridItem);

    playdate->system->realloc(prv, 0);
    playdate->system->realloc(sprite, 0);
}

void he_sprites_setDrawOffset(int dx, int dy)
{
    gfx_spriteDrawOffset.x = dx;
    gfx_spriteDrawOffset.y = dy;
}

void HESprite_setScreenClipRect(int x, int y, int width, int height)
{
    gfx_spriteScreenClipRect = rect_intersection(gfx_screenRect, rect_new(x, y, width, height));
}

void HESprite_clearScreenClipRect(void)
{
    gfx_spriteScreenClipRect = gfx_screenRect;
}

HESprite** he_sprites_getAll(int *len)
{
    *len = sprites->length;
    return (HESprite**)sprites->items;
}

void he_sprites_removeAll(void)
{
    array_clear(movingSprites);
    array_clear(followSprites);
    
    clearVisibleSprites();
    clearSpriteCollisions();
    
    clear_collision_pairs();
    
    for(int i = 0; i < sprites->length; i++)
    {
        HESprite *sprite = sprites->items[i];
        _HESprite_remove(sprite, i, 1);
    }
    array_clear(sprites);
}

HESpriteCollision* he_sprites_getCollisions(int *len)
{
    *len = spriteCollisions->length;
    return (HESpriteCollision*)spriteCollisions->items;
}

HESprite** he_sprites_queryWithRect(float x, float y, float width, float height, int *len)
{
    HERectF rect = rectf_new(x, y, width, height);
    
    HEArray *array = array_new();
    
    int count;
    grid_query(collisionsGrid, rect, grid_query_results, &count);
    
    for(int i = 0; i < count; i++)
    {
        HEGridItem *item = grid_query_results[i];
        HESprite *sprite = item->ptr;
        
        HERectF spriteRect = HESprite_measureCollisionRect(sprite);
        if(rectf_intersects(rect, spriteRect))
        {
            array_push(array, sprite);
        }
    }
    
    HESprite **items = (HESprite**)array->items;
    *len = array->length;
    array_free_container(array);
    return items;
}

HESprite** he_sprites_queryWithSegment(float x1, float y1, float x2, float y2, int *len)
{
    HEArray *array = array_new();
    
    float x_min = fminf(x1, x2);
    float x_max = fmaxf(x1, x2);
    
    float y_min = fminf(y1, y2);
    float y_max = fmaxf(y1, y2);
    
    HERectF rect = rectf_new(x_min, y_min, x_max - x_min, y_max - y_min);
    
    int count;
    grid_query(collisionsGrid, rect, grid_query_results, &count);
    
    for(int i = 0; i < count; i++)
    {
        HEGridItem *item = grid_query_results[i];
        HESprite *sprite = item->ptr;

        HERectF spriteRect = HESprite_measureCollisionRect(sprite);
        
        float t1; float t2; float nx1; float ny1; float nx2; float ny2;
        int intersect = segment_rect_intersection(x1, y1, x2, y2, spriteRect, &t1, &t2, &nx1, &ny1, &nx2, &ny2);
        
        if(intersect && t1 > 0 && t2 < 1)
        {
            array_push(array, sprite);
        }
    }
    
    HESprite **items = (HESprite**)array->items;
    *len = array->length;
    array_free_container(array);
    return items;
}

HESprite** he_sprites_queryWithPoint(float x, float y, int *len)
{
    HEArray *array = array_new();

    HERectF rect = rectf_new(x, y, 0, 0);
    
    int count;
    grid_query(collisionsGrid, rect, grid_query_results, &count);
    
    for(int i = 0; i < count; i++)
    {
        HEGridItem *item = grid_query_results[i];
        HESprite *sprite = item->ptr;
        
        HERectF spriteRect = HESprite_measureCollisionRect(sprite);
        if(rectf_contains(spriteRect, x, y))
        {
            array_push(array, sprite);
        }
    }
    
    HESprite **items = (HESprite**)array->items;
    *len = array->length;
    array_free_container(array);
    return items;
}

static int sort_sprites(const void *a, const void *b)
{
    _HESprite *sprite1 = (*(HESprite**)a)->prv;
    _HESprite *sprite2 = (*(HESprite**)b)->prv;
    
    if(sprite1->z_index < sprite2->z_index)
    {
        return -1;
    }
    else if(sprite1->z_index > sprite2->z_index)
    {
        return 1;
    }
    else if(sprite1->index < sprite2->index)
    {
        return -1;
    }
    else if(sprite1->index > sprite2->index)
    {
        return 1;
    }
    return 0;
}

static HERectF HESprite_measureRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    
    float x = prv->position.x - prv->width * prv->centerAnchor.x;
    float y = prv->position.y - prv->height * prv->centerAnchor.y;
    
    return rectf_new(x, y, prv->width, prv->height);
}

static HERectF HESprite_measureCollisionRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERectF collisionRect = HESprite_measureRect(sprite);
    if(prv->ignoresDrawOffset)
    {
        collisionRect.x += gfx_spriteDrawOffset.x;
        collisionRect.y += gfx_spriteDrawOffset.y;
    }
    if(prv->hasCollisionInnerRect)
    {
        collisionRect.x += prv->collisionInnerRect.x;
        collisionRect.y += prv->collisionInnerRect.y;
        collisionRect.width = prv->collisionInnerRect.width;
        collisionRect.height = prv->collisionInnerRect.height;
    }
    return collisionRect;
}

static HERect HESprite_measureScreenRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERectF rect = HESprite_measureRect(sprite);
    if(!prv->ignoresDrawOffset)
    {
        rect.x += gfx_spriteDrawOffset.x;
        rect.y += gfx_spriteDrawOffset.y;
    }
    return rect_new(roundf(rect.x), roundf(rect.y), ceilf(rect.width), ceilf(rect.height));
}

static HERect HESprite_measureClipRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERect rect = gfx_spriteScreenClipRect;
    if(prv->ignoresScreenClipRect)
    {
        rect = gfx_screenRect;
    }
    if(prv->hasClipRect)
    {
        HERect spriteRect = HESprite_measureScreenRect(sprite);
        HERect clipRect = rect_new(prv->clipRect.x, prv->clipRect.x, prv->clipRect.width, prv->clipRect.height);
        if(prv->clipRectReference == HESpriteClipRectRelative)
        {
            clipRect.x += spriteRect.x;
            clipRect.y += spriteRect.y;
        }
        rect = rect_intersection(rect, clipRect);
    }
    return rect;
}

static HESpriteCollisionType HESprite_requestCollisionType(HESprite *sprite1, HESprite *sprite2)
{
    _HESprite *_sprite1 = sprite1->prv;
    _HESprite *_sprite2 = sprite2->prv;
    
    if(_sprite1->collisionType.callback)
    {
        return _sprite1->collisionType.callback(sprite1, sprite2);
    }
    else if(_sprite1->collisionType.luaCallback)
    {
        playdate->lua->pushInt(_sprite1->index + 1);
        playdate->lua->pushInt(_sprite2->index + 1);
        playdate->lua->callFunction("he_callSpriteCollisionType", 2, NULL);
        return _sprite1->collisionType.luaResult;
    }
    return _sprite1->collisionType.value;
}

static void HESprite_updateCollisionInstance(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    HERectF rect = HESprite_measureCollisionRect(sprite);
    HERectF lastRect = rect;
    if(prv->hasLastCollisionRect)
    {
        lastRect = prv->lastCollisionRect;
    }
    prv->collisionInstance.rect = rect;
    prv->collisionInstance.center = rectf_center(rect);
    prv->collisionInstance.lastRect = lastRect;
    prv->collisionInstance.lastCenter = rectf_center(lastRect);
}

static HESpriteCollisionType collisionTypeForSprites(HESprite *sprite1, HESprite *sprite2)
{
    _HESprite *_sprite1 = sprite1->prv;
    
    if(!_sprite1->collisionType.callback && !_sprite1->collisionType.luaCallback)
    {
        return _sprite1->collisionType.value;
    }
    
    int cacheEnabled = _sprite1->collisionType.cacheEnabled;

    HECollisionPair *cachedPair = NULL;
    if(cacheEnabled)
    {
        cachedPair = collision_pair_get(sprite1, sprite2);
        if(cachedPair)
        {
            return cachedPair->collisionType;
        }
    }
    
    HESpriteCollisionType type = HESprite_requestCollisionType(sprite1, sprite2);
    if(cacheEnabled)
    {
        HECollisionPair *collisionPair = collision_pair_insert(sprite1, sprite2);
        collisionPair->collisionType = type;
    }
    return type;
}

void HESprite_setClipRect(HESprite *sprite, int x, int y, int width, int height)
{
    _HESprite *prv = sprite->prv;
    prv->hasClipRect = 1;
    prv->clipRect = rect_new(x, y, width, height);
}

void HESprite_clearClipRect(HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    prv->hasClipRect = 0;
    prv->clipRect = rect_zero();
}

void HESprite_setClipRectReference(HESprite *sprite, HESpriteClipRectReference reference)
{
    _HESprite *prv = sprite->prv;
    prv->clipRectReference = reference;
}

//
// Sprite collision
//
void HESpriteCollision_free(HESpriteCollision *collision)
{
    playdate->system->realloc(collision, 0);
}

//
// Collision pair
//
static HECollisionPair* collision_pair_new(HESprite *sprite1, HESprite *sprite2)
{
    HECollisionPair *pair = playdate->system->realloc(NULL, sizeof(HECollisionPair));
    pair->sprite1 = sprite1;
    pair->sprite2 = sprite2;
    pair->collisionType = HESpriteCollisionTypeSlide;
    pair->next = NULL;
    return pair;
}

static void collision_pair_free(HECollisionPair *pair)
{
    HECollisionPair *head = pair;
    while(head)
    {
        HECollisionPair *next = head->next;
        collision_pair_free(head);
        head = next;
    }
}

static int collision_pair_hash(HESprite *sprite1, HESprite *sprite2)
{
    return ((uintptr_t)sprite1 ^ (uintptr_t)sprite2) % HE_COLLISION_PAIR_MAX;
}

static HECollisionPair* collision_pair_get(HESprite *sprite1, HESprite *sprite2)
{
    int hash = collision_pair_hash(sprite1, sprite2);
    HECollisionPair *head = collisionPairs[hash];
    while(head)
    {
        if(head->sprite1 == sprite1 && head->sprite2 == sprite2)
        {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

static HECollisionPair* collision_pair_insert(HESprite *sprite1, HESprite *sprite2)
{
    int hash = collision_pair_hash(sprite1, sprite2);
    HECollisionPair *head = collisionPairs[hash];
    if(head)
    {
        HECollisionPair *pair = collision_pair_new(sprite1, sprite2);
        pair->next = head;
        collisionPairs[hash] = pair;
        return pair;
    }
    
    HECollisionPair *pair = collision_pair_new(sprite1, sprite2);
    collisionPairs[hash] = pair;
    return pair;
}

static void remove_collision_pairs(HESprite *sprite)
{
    for(int i = 0; i < HE_COLLISION_PAIR_MAX; i++)
    {
        HECollisionPair *current = collisionPairs[i];
        HECollisionPair *prev = NULL;
        
        while(current)
        {
            if(current->sprite1 == sprite || current->sprite2 == sprite)
            {
                HECollisionPair *current_tmp = current;
                
                if(!prev)
                {
                    // Head node
                    collisionPairs[i] = current->next;
                    prev = NULL;
                }
                else
                {
                    // Middle or last node
                    prev->next = current->next;
                }
                
                current = current->next;
                
                playdate->system->realloc(current_tmp, 0);
            }
            else
            {
                prev = current;
                current = current->next;
            }
        }
    }
}

static void clear_collision_pairs(void)
{
    for(int i = 0; i < HE_COLLISION_PAIR_MAX; i++)
    {
        HECollisionPair *current = collisionPairs[i];
        collision_pair_free(current);
        collisionPairs[i] = NULL;
    }
}

static void clearSpriteCollisions(void)
{
    generic_array_clear(spriteCollisions);
}

static void clearVisibleSprites(void)
{
    array_clear(visibleSprites);
}

//
// Collision resolution
//
static int segment_rect_intersection(float x1, float y1, float x2, float y2, HERectF rect, float *t1_out, float *t2_out, float *nx1_out, float *ny1_out, float *nx2_out, float *ny2_out)
{
    *t1_out = 0;
    *t2_out = 0;
    
    *nx1_out = 0;
    *ny1_out = 0;
    
    *nx2_out = 0;
    *ny2_out = 0;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    
    float dx_min = (dx != 0) ? -dx : 0;
    float dy_min = (dy != 0) ? -dy : 0;

    float t1 = -FLT_MAX;
    float t2 = FLT_MAX;
    
    for(int side = 0; side < 4; side++)
    {
        float p;
        float q;
        float nx;
        float ny;
        
        switch(side)
        {
            case 0:
            {
                p = dx_min; q = x1 - rect.x; nx = 1; ny = 0;
                break;
            }
            case 1:
            {
                p = dx; q = rect.x + rect.width - x1; nx = -1; ny = 0;
                break;
            }
            case 2:
            {
                p = dy_min; q = y1 - rect.y; nx = 0; ny = 1;
                break;
            }
            default:
            {
                p = dy; q = rect.y + rect.height - y1; nx = 0; ny = -1;
                break;
            }
        }

        if(p == 0)
        {
            if(q <= 0)
            {
                return 0;
            }
        }
        else
        {
            float r = q / p;

            if(p < 0)
            {
                if(r > t2)
                {
                    return 0;
                }
                else if(r > t1)
                {
                    // Line is clipped
                    t1 = r;
                    *nx1_out = nx;
                    *ny1_out = ny;
                }
            }
            else
            {
                if(r < t1)
                {
                    return 0;
                }
                else if(r < t2)
                {
                    // Line is clipped
                    t2 = r;
                    *nx2_out = nx;
                    *ny2_out = ny;
                }
            }
        }
    }
    
    *t1_out = t1;
    *t2_out = t2;
    
    return 1;
}

static void resolveSpriteCollision(HESpriteCollisionType type, HESprite *sprite1, HESprite *sprite2, float dx, float dy, float nx1, float ny1, float nx2, float ny2, int overlaps, int resolve, HEGenericArray *results)
{
    _HESprite *_sprite1 = sprite1->prv;
    _HESprite *_sprite2 = sprite2->prv;
    
    HESpriteCollisionInstance *c1 = &_sprite1->collisionInstance;
    HESpriteCollisionInstance *c2 = &_sprite2->collisionInstance;
    
    HEVec2 goal = c1->center;
    HEVec2 normal = vec2_new(nx1, ny1);
    HEVec2 move = vec2_new(c1->center.x - c1->lastCenter.x, c1->center.y - c1->lastCenter.y);
    HEVec2 touch = vec2_new(c1->center.x + dx, c1->center.y + dy);
    
    // Goal (relative to sprite)
    HEVec2 spriteGoal = vec2_new(c1->rect.x - _sprite1->collisionInnerRect.x + c1->rect.width * _sprite1->centerAnchor.x, c1->rect.y - _sprite1->collisionInnerRect.y + c1->rect.height * _sprite1->centerAnchor.y);
    
    // Touch (relative to sprite)
    HEVec2 spriteTouch = vec2_new(c1->rect.x + dx - _sprite1->collisionInnerRect.x + c1->rect.width * _sprite1->centerAnchor.x, c1->rect.y + dy - _sprite1->collisionInnerRect.y + c1->rect.height * _sprite1->centerAnchor.y);
    
    HERectF touchRect = rectf_new(touch.x - c1->rect.width * 0.5f - _sprite1->collisionInnerRect.x, touch.y - c1->rect.height * 0.5f - _sprite1->collisionInnerRect.y, _sprite1->width, _sprite1->height);
    HERectF touchOtherRect = rectf_new(c2->lastRect.x - _sprite2->collisionInnerRect.x, c2->lastRect.y - _sprite2->collisionInnerRect.y, _sprite2->width, _sprite2->height);
    
    HEVec2 destination = touch;
    
    if(!overlaps && type == HESpriteCollisionTypeSlide)
    {
        // Tunnelling is allowed when a sprite slides "into" a corner
        
        HEVec2 cornerPoint = touch;
        if(normal.x > 0)
        {
            cornerPoint.x += c1->rect.width * 0.5f;
        }
        else if(normal.x < 0)
        {
            cornerPoint.x -= c1->rect.width * 0.5f;
        }
        if(normal.y < 0)
        {
            cornerPoint.y -= c1->rect.height * 0.5f;
        }
        else if(normal.y > 0)
        {
            cornerPoint.y += c1->rect.height * 0.5f;
        }
        
        int cornerIndex = -1;
        if(nx1 < 0)
        {
            if(ny2 > 0){ cornerIndex = 1; }
            else if(ny2 < 0){ cornerIndex = 2; }
        }
        else if(nx1 > 0)
        {
            if(ny2 > 0){ cornerIndex = 0; }
            else if(ny2 < 0){ cornerIndex = 3; }
        }
        else if(ny1 < 0)
        {
            if(nx2 > 0){ cornerIndex = 3; }
            else if(nx2 < 0){ cornerIndex = 2; }
        }
        else if(ny1 > 0)
        {
            if(nx2 > 0){ cornerIndex = 0; }
            else if(nx2 < 0){ cornerIndex = 1; }
        }
        
        if(point_in_corner(cornerPoint, c2->lastRect, cornerIndex, cornerTolerance))
        {
            return;
        }
    }
    
    switch(type)
    {
        case HESpriteCollisionTypeSlide:
        {
            if(overlaps && (move.x != 0 || move.y != 0))
            {
                if(normal.x != 0)
                {
                    destination.y = goal.y;
                }
                else
                {
                    destination.x = goal.x;
                }
            }
            break;
        }
        case HESpriteCollisionTypeBounce:
        {
            if(overlaps && (move.x != 0 || move.y != 0))
            {
                if(normal.x != 0)
                {
                    destination.x = touch.x + dx;
                    destination.y = goal.y;
                }
                else
                {
                    destination.x = goal.x;
                    destination.y = touch.y + dy;
                }
            }
            break;
        }
        default:
            break;
    }
    
    if(resolve && type != HESpriteCollisionTypeOverlap)
    {
        float offsetX = _sprite1->width * _sprite1->centerAnchor.x;
        float offsetY = _sprite1->height * _sprite1->centerAnchor.y;
        
        _sprite1->position.x = destination.x - c1->rect.width * 0.5f - _sprite1->collisionInnerRect.x + offsetX;
        _sprite1->position.y = destination.y - c1->rect.height * 0.5f - _sprite1->collisionInnerRect.y + offsetY;
        
        HESprite_didMove(sprite1);
        HESprite_resetMotion(sprite1);
        HESprite_updateCollisionInstance(sprite1);
    }

    HESpriteCollision *collision = (HESpriteCollision*)generic_array_push(results, NULL);
    
    collision->type = type;
    collision->sprite = sprite1;
    collision->otherSprite = sprite2;
    collision->normal = normal;
    collision->touch = spriteTouch;
    collision->rect = touchRect;
    collision->otherRect = touchOtherRect;
    collision->goal = spriteGoal;
}

static void resolveMinimumDisplacement(HESpriteCollisionType type, HESprite *sprite1, HESprite *sprite2, int resolve, HEGenericArray *results)
{
    _HESprite *_sprite1 = sprite1->prv;
    _HESprite *_sprite2 = sprite2->prv;

    HESpriteCollisionInstance *c1 = &_sprite1->collisionInstance;
    HESpriteCollisionInstance *c2 = &_sprite2->collisionInstance;

    float max_dx;
    float max_dy;
    
    if(c1->lastCenter.x < c2->lastCenter.x)
    {
        max_dx = c2->lastRect.x - (c1->rect.x + c1->rect.width);
    }
    else
    {
        max_dx = c2->lastRect.x + c2->lastRect.width - c1->rect.x;
    }
    
    if(c1->lastCenter.y < c2->lastCenter.y)
    {
        max_dy = c2->lastRect.y - (c1->rect.y + c1->rect.height);
    }
    else
    {
        max_dy = c2->lastRect.y + c2->lastRect.height - c1->rect.y;
    }
    
    float dx = 0;
    float dy = 0;
    
    float nx1 = 0;
    float ny1 = 0;
    
    float nx2 = 0;
    float ny2 = 0;
    
    if(fabsf(max_dx) < fabsf(max_dy))
    {
        dx = max_dx;
        int nx_sign = signf(max_dx);
        nx1 = nx_sign;
        nx2 = nsign(nx_sign);
    }
    else
    {
        dy = max_dy;
        int ny_sign = signf(max_dy);
        ny1 = ny_sign;
        ny2 = nsign(ny_sign);
    }
    
    resolveSpriteCollision(type, sprite1, sprite2, dx, dy, nx1, ny1, nx2, ny2, 1, resolve, results);
}

void he_sprites_move(float deltaTime)
{
    clearSpriteCollisions();
    
    for(int i = 0; i < followSprites->length; i++)
    {
        HESprite *sprite = followSprites->items[i];
        _HESprite *_sprite = sprite->prv;
        HESpriteFollow *follow = &_sprite->follow;
        
        HESprite *targetSprite = _sprite->follow.target;
        
        HEVec2 targetPosition = HESprite_getCenterPosition(targetSprite);
        HEVec2 spritePosition = HESprite_getCenterPosition(sprite);

        if(follow->refreshRate > 0)
        {
            follow->refreshElapsedTime += deltaTime;
            
            if(!follow->hasOldPosition)
            {
                follow->oldPosition = targetPosition;
                follow->hasOldPosition = 1;
            }
            
            if(follow->refreshElapsedTime >= follow->refreshRate)
            {
                follow->refreshElapsedTime = fmodf(follow->refreshElapsedTime, follow->refreshRate);
                follow->oldPosition = targetPosition;
                follow->hasOldPosition = 1;
            }
            
            targetPosition = follow->oldPosition;
        }
        
        targetPosition.x += _sprite->follow.offset.x;
        targetPosition.y += _sprite->follow.offset.y;

        float velocity = fabsf(follow->velocity * deltaTime);
        
        float dx = targetPosition.x - spritePosition.x;
        float dy = targetPosition.y - spritePosition.y;
        
        float distance = sqrtf(dx * dx + dy * dy);
        
        float goalX = targetPosition.x;
        float goalY = targetPosition.y;
        
        if(distance > velocity)
        {
            float dirX = 0;
            float dirY = 0;
            if(distance != 0)
            {
                dirX = dx / distance;
                dirY = dy / distance;
            }
            
            goalX = spritePosition.x + dirX * velocity;
            goalY = spritePosition.y + dirY * velocity;
        }
        
        // Adjust center
        goalX += -(_sprite->width) * 0.5f + _sprite->width * _sprite->centerAnchor.x;
        goalY += -(_sprite->height) * 0.5f + _sprite->height * _sprite->centerAnchor.y;
        
        HESprite_moveTo(sprite, goalX, goalY);
    }
    
    for(int i = 0; i < movingSprites->length; i++)
    {
        HESprite *sprite1 = movingSprites->items[i];
        _HESprite *_sprite1 = sprite1->prv;
        
        if(!_sprite1->collisionsEnabled)
        {
            HESprite_resetMotion(sprite1);
            continue;
        }
        
        HESprite_updateCollisionInstance(sprite1);
        HESpriteCollisionInstance *c1 = &_sprite1->collisionInstance;
        
        HERectF queryRect = rectf_max(c1->lastRect, c1->rect);
        
        int count;
        grid_query(collisionsGrid, queryRect, grid_default_results, &count);
        
        for(int j = 0; j < count; j++)
        {
            HEGridItem *item = grid_default_results[j];
            HESprite *sprite2 = item->ptr;
            _HESprite *_sprite2 = sprite2->prv;
            
            if(sprite1 == sprite2)
            {
                continue;
            }
            
            HESpriteCollisionType type = collisionTypeForSprites(sprite1, sprite2);
            if(type == HESpriteCollisionTypeIgnore)
            {
                continue;
            }
            
            HESprite_updateCollisionInstance(sprite2);
            HESpriteCollisionInstance *c2 = &_sprite2->collisionInstance;
            
            if(_sprite1->fastCollisions || _sprite2->fastCollisions)
            {
                if(rectf_intersects(c1->rect, c2->lastRect))
                {
                    resolveMinimumDisplacement(type, sprite1, sprite2, 1, spriteCollisions);
                }
                continue;
            }
            
            float vx = c1->center.x - c1->lastCenter.x;
            float vy = c1->center.y - c1->lastCenter.y;
            
            if(rectf_intersects(c1->rect, c2->lastRect))
            {
                // Intersection at current position
                
                if(!rectf_intersects(c1->lastRect, c2->lastRect) && (vx != 0 || vy != 0))
                {
                    // 1. Not intersecting in the last position
                    // 2. Is moving
                    
                    HERectF rect_sum = rectf_sum(c2->lastRect, c1->rect);
                    
                    float t1; float t2; float nx1; float ny1; float nx2; float ny2;
                    int intersect = segment_rect_intersection(c1->lastCenter.x, c1->lastCenter.y, c1->center.x, c1->center.y, rect_sum, &t1, &t2, &nx1, &ny1, &nx2, &ny2);
                    
                    if(intersect && t1 < 1)
                    {
                        float dx = vx * (t1 - 1);
                        float dy = vy * (t1 - 1);
                        
                        resolveSpriteCollision(type, sprite1, sprite2, dx, dy, nx1, ny1, nx2, ny2, 1, 1, spriteCollisions);
                    }
                }
                else
                {
                    // Use minimum displacement
                    resolveMinimumDisplacement(type, sprite1, sprite2, 1, spriteCollisions);
                }
            }
            else
            {
                // Check for tunneling
                
                HERectF rect_sum = rectf_sum(c2->lastRect, c1->rect);
                
                float t1; float t2; float nx1; float ny1; float nx2; float ny2;
                int intersect = segment_rect_intersection(c1->lastCenter.x, c1->lastCenter.y, c1->center.x, c1->center.y, rect_sum, &t1, &t2, &nx1, &ny1, &nx2, &ny2);
                
                if(intersect && ((t1 > 0 && t1 < 1) || (t1 == 0 && t2 > 0)))
                {
                    float dx = vx * (t1 - 1);
                    float dy = vy * (t1 - 1);

                    resolveSpriteCollision(type, sprite1, sprite2, dx, dy, nx1, ny1, nx2, ny2, 0, 1, spriteCollisions);
                }
            }
        }
        
        HESprite_resetMotion(sprite1);
    }
    
    array_clear(movingSprites);
}

void he_sprites_update(void)
{
    clearVisibleSprites();
    
    HERect adjustedScreenRect = gfx_screenRect;
    adjustedScreenRect.x -= gfx_spriteDrawOffset.x;
    adjustedScreenRect.y -= gfx_spriteDrawOffset.y;

    HERectF queryRect = rectf_from_rect(adjustedScreenRect);
    
    int count;
    grid_query(visibilityGrid, queryRect, grid_default_results, &count);
    
    for(int i = 0; i < count; i++)
    {
        HEGridItem *item = grid_default_results[i];
        HESprite *sprite = item->ptr;
        _HESprite *prv = sprite->prv;
        
        HERect screenClip = gfx_spriteScreenClipRect;
        if(prv->ignoresScreenClipRect)
        {
            screenClip = gfx_screenRect;
        }
        HERect clipRect = HESprite_measureClipRect(sprite);
        
        if(rect_intersects(screenClip, clipRect))
        {
            if(prv->updateCallback)
            {
                prv->updateCallback(sprite);
            }
            else if(prv->luaUpdateCallback)
            {
                playdate->lua->pushInt(prv->index + 1);
                playdate->lua->callFunction("he_callSpriteUpdate", 1, NULL);
            }
            
            // Recalc rect after update, bitmap or size could change
            
            HERect clipRect2 = HESprite_measureClipRect(sprite);
            
            if(rect_intersects(screenClip, clipRect2))
            {
                prv->screenRect = HESprite_measureScreenRect(sprite);
                prv->screenClipRect = clipRect2;
                array_push(visibleSprites, sprite);
            }
        }
    }
    
    qsort(visibleSprites->items, visibleSprites->length, sizeof(HESprite*), sort_sprites);
}

void he_sprites_draw(void)
{
    for(int i = 0; i < visibleSprites->length; i++)
    {
        HESprite *sprite = visibleSprites->items[i];
        _HESprite *prv = sprite->prv;
        
        HERect clipRect = prv->screenClipRect;
        
        he_graphics_pushContext();
        he_graphics_setClipRect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
        
        switch(prv->type)
        {
            case HESpriteTypeBitmap:
            {
                HEBitmap_draw(prv->bitmap, prv->screenRect.x, prv->screenRect.y);
                break;
            }
            case HESpriteTypeTileBitmap:
            {
                HERect rect = prv->screenRect;
                HEBitmap *bitmap = prv->tileBitmap;
                
                HERect innerRect = rect_intersection(clipRect, rect);
                
                he_graphics_pushContext();
                he_graphics_setClipRect(innerRect.x, innerRect.y, innerRect.width, innerRect.height);
                
                int mx = (innerRect.x - rect.x - prv->tileOffset.x) % bitmap->width;
                if(mx < 0)
                {
                    mx = bitmap->width + mx;
                }
                int startX = innerRect.x - mx;
                
                int my = (innerRect.y - rect.y - prv->tileOffset.y) % bitmap->height;
                if(my < 0)
                {
                    my = bitmap->height + my;
                }
                int startY = innerRect.y - my;
                
                int endX = innerRect.x + innerRect.width;
                int endY = innerRect.y + innerRect.height;
                
                for(int x = startX; x < endX; x += bitmap->width)
                {
                    for(int y = startY; y < endY; y += bitmap->height)
                    {
                        HEBitmap_draw(bitmap, x, y);
                    }
                }
                
                he_graphics_popContext();
                
                break;
            }
            case HESpriteTypeCallback:
            case HESpriteTypeLuaCallback:
            {
                playdate->graphics->pushContext(NULL);
                playdate->graphics->setDrawOffset(prv->screenRect.x, prv->screenRect.y);
                playdate->graphics->setScreenClipRect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
                
                if(prv->type == HESpriteTypeCallback)
                {
                    prv->drawCallback(sprite, prv->screenRect);
                }
                else
                {
                    playdate->lua->pushInt(prv->index + 1);
                    playdate->lua->pushInt(prv->screenRect.x);
                    playdate->lua->pushInt(prv->screenRect.y);
                    playdate->lua->pushInt(prv->screenRect.width);
                    playdate->lua->pushInt(prv->screenRect.height);
                    playdate->lua->callFunction("he_callSpriteDraw", 5, NULL);
                }
                
                playdate->graphics->popContext();
                break;
            }
            default:
                break;
        }
        
        he_graphics_popContext();
    }
}

void he_sprites_resizeGrid(float x, float y, float width, float height, float cellSize)
{
    HERectF rect = rectf_new(x, y, width, height);
    
    grid_resize(collisionsGrid, rect, cellSize);
    grid_resize(visibilityGrid, rect, cellSize);
}

#if HE_LUA_BINDINGS
//
// Lua bindings
//
static int lua_spriteNew(lua_State *L)
{
    HESprite *sprite = HESprite_new();
    _HESprite *prv = sprite->prv;
    LuaUDObject *luaRef = playdate->lua->pushObject(sprite, lua_kSprite, 0);
    prv->luaRef = luaRef;
    // Enable collision cache in Lua
    prv->collisionType.cacheEnabled = 1;
    return 1;
}

static int lua_spriteSetEmpty(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_setEmpty(sprite);
    return 0;
}

static int lua_spriteSetBitmap(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HEBitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    HESprite_setBitmap(sprite, bitmap);
    return 0;
}

static int lua_spriteSetBitmapFromTable(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HEBitmapTable *bitmapTable = playdate->lua->getArgObject(2, lua_kBitmapTable, NULL);
    int index = playdate->lua->getArgInt(3);
    HEBitmap *bitmap = HEBitmapAtIndex_1(bitmapTable, index);
    if(bitmap)
    {
        _HEBitmap *bitmapPrv = bitmap->prv;
        LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
        bitmapPrv->luaObject.ref = luaRef;
        
        HESprite_setBitmap(sprite, bitmap);
    }
    return 0;
}

static int lua_spriteSetTileBitmap(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HEBitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    HESprite_setTileBitmap(sprite, bitmap);
    return 0;
}

static int lua_spriteSetTileOffset(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int dx = playdate->lua->getArgFloat(2);
    int dy = playdate->lua->getArgFloat(3);
    HESprite_setTileOffset(sprite, dx, dy);
    return 0;
}

static int lua_spriteSetDrawCallback(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    HESprite_resetType(sprite);
    prv->type = HESpriteTypeLuaCallback;
    return 0;
}

static int lua_spriteSetUpdateCallback(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    int flag = playdate->lua->getArgBool(2);
    prv->luaUpdateCallback = flag;
    prv->updateCallback = NULL;
    return 0;
}

static int lua_spriteSetZIndex(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int z_index = playdate->lua->getArgInt(2);
    HESprite_setZIndex(sprite, z_index);
    return 0;
}

static int lua_spriteMoveTo(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    HESprite_moveTo(sprite, x, y);
    return 0;
}

static int lua_spriteSetFollowTarget(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite *target = playdate->lua->getArgObject(2, lua_kSprite, NULL);
    HESprite_setFollowTarget(sprite, target);
    return 0;
}

static int lua_spriteSetFollowVelocity(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float velocity = playdate->lua->getArgFloat(2);
    HESprite_setFollowVelocity(sprite, velocity);
    return 0;
}

static int lua_spriteSetFollowRefreshRate(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float rate = playdate->lua->getArgFloat(2);
    HESprite_setFollowRefreshRate(sprite, rate);
    return 0;
}

static int lua_spriteSetFollowOffset(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float dx = playdate->lua->getArgFloat(2);
    float dy = playdate->lua->getArgFloat(3);
    HESprite_setFollowOffset(sprite, dx, dy);
    return 0;
}

static int lua_spriteSetFastCollisions(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_setFastCollisions(sprite, flag);
    return 0;
}

static int lua_spriteSetPosition(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    HESprite_setPosition(sprite, x, y);
    return 0;
}

static int lua_spriteGetPosition(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    playdate->lua->pushFloat(prv->position.x);
    playdate->lua->pushFloat(prv->position.y);
    return 2;
}

static int lua_spriteSetSize(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float width = playdate->lua->getArgFloat(2);
    float height = playdate->lua->getArgFloat(3);
    HESprite_setSize(sprite, width, height);
    return 0;
}

static int lua_spriteGetSize(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    playdate->lua->pushFloat(prv->width);
    playdate->lua->pushFloat(prv->height);
    return 2;
}

static int lua_spriteSetCenter(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float cx = playdate->lua->getArgFloat(2);
    float cy = playdate->lua->getArgFloat(3);
    HESprite_setCenter(sprite, cx, cy);
    return 0;
}

static int lua_spriteSetVisible(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_setVisible(sprite, flag);
    return 0;
}

static int lua_spriteIsVisible(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int visible = HESprite_isVisible(sprite);
    playdate->lua->pushBool(visible);
    return 1;
}

static int lua_spriteIsVisibleOnScreen(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int visible = HESprite_isVisibleOnScreen(sprite);
    playdate->lua->pushBool(visible);
    return 1;
}

static int lua_spriteSetIgnoresDrawOffset(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_setIgnoresDrawOffset(sprite, flag);
    return 0;
}

static int lua_spriteSetIgnoresScreenClip(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_setIgnoresScreenClipRect(sprite, flag);
    return 0;
}

static int lua_spriteSetClipRect(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    float width = playdate->lua->getArgFloat(4);
    float height = playdate->lua->getArgFloat(5);
    HESprite_setClipRect(sprite, x, y, width, height);
    return 0;
}

static int lua_spriteClearClipRect(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_clearClipRect(sprite);
    return 0;
}

static int lua_spriteSetClipRectReference(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESpriteClipRectReference reference = playdate->lua->getArgInt(2);
    if(reference == HESpriteClipRectRelative || reference == HESpriteClipRectAbsolute)
    {
        HESprite_setClipRectReference(sprite, reference);
    }
    return 0;
}

static int lua_spriteSetCollisionType(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    int type = playdate->lua->getArgInt(2);
    HESprite_setCollisionType(sprite, HESprite_castCollisionType(type));
    prv->collisionType.luaCallback = 0;
    return 0;
}

static int lua_spriteSetCollisionTypeResult(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    int type = playdate->lua->getArgInt(2);
    prv->collisionType.luaResult = HESprite_castCollisionType(type);
    return 0;
}

static int lua_spriteSetCollisionTypeCallback(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _HESprite *prv = sprite->prv;
    int flag = playdate->lua->getArgBool(2);
    prv->collisionType.luaCallback = flag;
    prv->collisionType.value = HESpriteCollisionTypeSlide;
    return 0;
}

static int lua_spriteGetCollisionType(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int isCallback;
    HESpriteCollisionType type = HESprite_getCollisionType(sprite, &isCallback);
    playdate->lua->pushInt(type);
    playdate->lua->pushBool(isCallback);
    return 2;
}

static int lua_spriteCacheCollisionType(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_cacheCollisionType(sprite, flag);
    return 0;
}

static int lua_spriteCollisionTypeIsBeingCached(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    playdate->lua->pushBool(HESprite_collisionTypeIsBeingCached(sprite));
    return 1;
}

static int lua_spriteInvalidateCollisionType(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_invalidateCollisionType(sprite);
    return 0;
}

static int lua_spriteSetCollisionsEnabled(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int flag = playdate->lua->getArgBool(2);
    HESprite_setCollisionsEnabled(sprite, flag);
    return 0;
}

static int lua_spriteSetCollisionRect(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    float width = playdate->lua->getArgFloat(4);
    float height = playdate->lua->getArgFloat(5);
    HESprite_setCollisionRect(sprite, x, y, width, height);
    return 0;
}

static int lua_spriteClearCollisionRect(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_clearCollisionRect(sprite);
    return 0;
}

static int lua_spriteSetNeedsCollisions(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_setNeedsCollisions(sprite);
    return 0;
}

static int lua_spriteAdd(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int index = HESprite_add_ret(sprite);
    playdate->lua->pushInt(index);
    return 1;
}

static int lua_spriteRemove(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int index = HESprite_remove_ret(sprite);
    playdate->lua->pushInt(index);
    return 1;
}

static int lua_spriteCheckCollisions(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    
    int len;
    HESpriteCollision *collisions = HESprite_checkCollisions(sprite, &len);
    HELuaArray *luaArray = lua_array_new(collisions, len, sizeof(HESpriteCollision), HELuaArrayItemCollision);
    playdate->system->realloc(collisions, 0);
    
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    playdate->lua->pushInt(luaArray->length);
    return 2;
}

static int lua_spriteRemoveAll(lua_State *L)
{
    he_sprites_removeAll();
    return 0;
}

static int lua_spriteFree(lua_State *L)
{
    HESprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    HESprite_free(sprite);
    return 0;
}

static const lua_reg lua_sprite[] = {
    { "new", lua_spriteNew },
    { "add", lua_spriteAdd },
    { "remove", lua_spriteRemove },
    { "setEmpty", lua_spriteSetEmpty },
    { "setBitmap", lua_spriteSetBitmap },
    { "setBitmapFromTable", lua_spriteSetBitmapFromTable },
    { "setTileBitmap", lua_spriteSetTileBitmap },
    { "setTileOffset", lua_spriteSetTileOffset },
    { "setDrawCallback", lua_spriteSetDrawCallback },
    { "setUpdateCallback", lua_spriteSetUpdateCallback },
    { "setZIndex", lua_spriteSetZIndex },
    { "moveTo", lua_spriteMoveTo },
    { "setPosition", lua_spriteSetPosition },
    { "getPosition", lua_spriteGetPosition },
    { "setSize", lua_spriteSetSize },
    { "getSize", lua_spriteGetSize },
    { "setCenter", lua_spriteSetCenter },
    { "setVisible", lua_spriteSetVisible },
    { "isVisible", lua_spriteIsVisible },
    { "isVisibleOnScreen", lua_spriteIsVisibleOnScreen },
    { "setIgnoresDrawOffset", lua_spriteSetIgnoresDrawOffset },
    { "setCollisionType", lua_spriteSetCollisionType },
    { "setCollisionTypeCallback", lua_spriteSetCollisionTypeCallback },
    { "getCollisionType", lua_spriteGetCollisionType },
    { "setCollisionTypeResult", lua_spriteSetCollisionTypeResult},
    { "cacheCollisionType", lua_spriteCacheCollisionType },
    { "invalidateCollisionType", lua_spriteInvalidateCollisionType },
    { "collisionTypeIsBeingCached", lua_spriteCollisionTypeIsBeingCached },
    { "setCollisionsEnabled", lua_spriteSetCollisionsEnabled },
    { "setIgnoresScreenClip", lua_spriteSetIgnoresScreenClip },
    { "setClipRect", lua_spriteSetClipRect },
    { "clearClipRect", lua_spriteClearClipRect },
    { "setClipRectReference", lua_spriteSetClipRectReference },
    { "setCollisionRect", lua_spriteSetCollisionRect },
    { "clearCollisionRect", lua_spriteClearCollisionRect },
    { "setFollowTarget", lua_spriteSetFollowTarget },
    { "setFollowVelocity", lua_spriteSetFollowVelocity },
    { "setFollowRefreshRate", lua_spriteSetFollowRefreshRate },
    { "setFollowOffset", lua_spriteSetFollowOffset },
    { "setFastCollisions", lua_spriteSetFastCollisions },
    { "setNeedsCollisions", lua_spriteSetNeedsCollisions },
    { "checkCollisions", lua_spriteCheckCollisions },
    { "__gc", lua_spriteFree },
    { NULL, NULL }
};

static int lua_collisionGetSprite(lua_State *L, HESprite *sprite)
{
    _HESprite *prv = sprite->prv;
    if(prv->index >= 0)
    {
        playdate->lua->pushInt(prv->index + 1);
        return 1;
    }
    return 0;
}

static int lua_spriteCollisionGetType(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushInt(collision->type);
    return 1;
}

static int lua_spriteCollisionGetSprite1(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    return lua_collisionGetSprite(L, collision->sprite);
}

static int lua_spriteCollisionGetSprite2(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    return lua_collisionGetSprite(L, collision->otherSprite);
}

static int lua_spriteCollisionGetTouch(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushFloat(collision->touch.x);
    playdate->lua->pushFloat(collision->touch.y);
    return 2;
}

static int lua_spriteCollisionGetRect1(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushFloat(collision->rect.x);
    playdate->lua->pushFloat(collision->rect.y);
    playdate->lua->pushFloat(collision->rect.width);
    playdate->lua->pushFloat(collision->rect.height);
    return 4;
}

static int lua_spriteCollisionGetRect2(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushFloat(collision->otherRect.x);
    playdate->lua->pushFloat(collision->otherRect.y);
    playdate->lua->pushFloat(collision->otherRect.width);
    playdate->lua->pushFloat(collision->otherRect.height);
    return 4;
}

static int lua_spriteCollisionGetNormal(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushFloat(collision->normal.x);
    playdate->lua->pushFloat(collision->normal.y);
    return 2;
}

static int lua_spriteCollisionGetGoal(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    playdate->lua->pushFloat(collision->goal.x);
    playdate->lua->pushFloat(collision->goal.y);
    return 2;
}

static int lua_spriteCollisionFree(lua_State *L)
{
    HESpriteCollision *collision = playdate->lua->getArgObject(1, lua_kSpriteCollision, NULL);
    HESpriteCollision_free(collision);
    return 0;
}

static const lua_reg lua_spriteCollision[] = {
    { "getType", lua_spriteCollisionGetType },
    { "_getSprite1", lua_spriteCollisionGetSprite1 },
    { "_getSprite2", lua_spriteCollisionGetSprite2 },
    { "getTouch", lua_spriteCollisionGetTouch },
    { "getNormal", lua_spriteCollisionGetNormal },
    { "getRect", lua_spriteCollisionGetRect1 },
    { "getOtherRect", lua_spriteCollisionGetRect2 },
    { "getGoal", lua_spriteCollisionGetGoal },
    { "__gc", lua_spriteCollisionFree },
    { NULL, NULL }
};

static int lua_spriteSetDrawOffset(lua_State *L)
{
    int dx = playdate->lua->getArgFloat(1);
    int dy = playdate->lua->getArgFloat(2);
    he_sprites_setDrawOffset(dx, dy);
    return 0;
}

static int lua_spriteSetScreenClipRect(lua_State *L)
{
    int x = playdate->lua->getArgFloat(1);
    int y = playdate->lua->getArgFloat(2);
    int width = playdate->lua->getArgFloat(3);
    int height = playdate->lua->getArgFloat(4);
    HESprite_setScreenClipRect(x, y, width, height);
    return 0;
}

static int lua_spritesMove(lua_State *L)
{
    float deltaTime = playdate->lua->getArgFloat(1);
    he_sprites_move(deltaTime);
    return 0;
}

static int lua_spritesUpdate(lua_State *L)
{
    he_sprites_update();
    return 0;
}

static int lua_spritesDraw(lua_State *L)
{
    he_sprites_draw();
    return 0;
}

static int lua_spritesGetCollisions(lua_State *L)
{
    HELuaArray *luaArray = lua_array_new(spriteCollisions->items, spriteCollisions->length, sizeof(HESpriteCollision), HELuaArrayItemCollision);
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    playdate->lua->pushInt(luaArray->length);
    return 2;
}

static int lua_spriteResizeGrid(lua_State *L)
{
    float x = playdate->lua->getArgFloat(1);
    float y = playdate->lua->getArgFloat(2);
    float width = playdate->lua->getArgFloat(3);
    float height = playdate->lua->getArgFloat(4);
    float size = playdate->lua->getArgFloat(5);
    he_sprites_resizeGrid(x, y, width, height, size);
    return 0;
}

static int lua_spriteQueryWithRect(lua_State *L)
{
    float x = playdate->lua->getArgFloat(1);
    float y = playdate->lua->getArgFloat(2);
    float width = playdate->lua->getArgFloat(3);
    float height = playdate->lua->getArgFloat(4);
    
    int len;
    HESprite** sprites = he_sprites_queryWithRect(x, y, width, height, &len);
    HELuaArray *luaArray = lua_array_new(sprites, len, sizeof(HESprite*), HELuaArrayItemSprite);
    playdate->system->realloc(sprites, 0);
    
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    playdate->lua->pushInt(luaArray->length);
    
    return 2;
}

static int lua_spriteQueryWithSegment(lua_State *L)
{
    float x1 = playdate->lua->getArgFloat(1);
    float y1 = playdate->lua->getArgFloat(2);
    float x2 = playdate->lua->getArgFloat(3);
    float y2 = playdate->lua->getArgFloat(4);
    
    int len;
    HESprite** sprites = he_sprites_queryWithSegment(x1, y1, x2, y2, &len);
    HELuaArray *luaArray = lua_array_new(sprites, len, sizeof(HESprite*), HELuaArrayItemSprite);
    playdate->system->realloc(sprites, 0);
    
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    playdate->lua->pushInt(luaArray->length);
    
    return 2;
}

static int lua_spriteQueryWithPoint(lua_State *L)
{
    float x = playdate->lua->getArgFloat(1);
    float y = playdate->lua->getArgFloat(2);
    
    int len;
    HESprite** sprites = he_sprites_queryWithPoint(x, y, &len);
    HELuaArray *luaArray = lua_array_new(sprites, len, sizeof(HESprite*), HELuaArrayItemSprite);
    playdate->system->realloc(sprites, 0);
    
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    playdate->lua->pushInt(luaArray->length);
    
    return 2;
}

static const lua_reg lua_spritePublic[] = {
    { "setDrawOffset", lua_spriteSetDrawOffset },
    { "setScreenClipRect", lua_spriteSetScreenClipRect },
    { "_removeAll", lua_spriteRemoveAll },
    { "move", lua_spritesMove },
    { "update", lua_spritesUpdate },
    { "draw", lua_spritesDraw },
    { "getCollisions", lua_spritesGetCollisions },
    { "resizeGrid", lua_spriteResizeGrid },
    { "queryWithRect", lua_spriteQueryWithRect },
    { "queryWithSegment", lua_spriteQueryWithSegment },
    { "queryWithPoint", lua_spriteQueryWithPoint },
    { NULL, NULL }
};

static const lua_val lua_spritePublicVal[] = {
    { "kCollisionTypeSlide", kInt, { .intval = HESpriteCollisionTypeSlide } },
    { "kCollisionTypeFreeze", kInt, { .intval = HESpriteCollisionTypeFreeze } },
    { "kCollisionTypeOverlap", kInt, { .intval = HESpriteCollisionTypeOverlap } },
    { "kCollisionTypeBounce", kInt, { .intval = HESpriteCollisionTypeBounce } },
    { "kCollisionTypeIgnore", kInt, { .intval = HESpriteCollisionTypeIgnore } },
    { "kClipRectRelative", kInt, { .intval = HESpriteClipRectRelative } },
    { "kClipRectAbsolute", kInt, { .intval = HESpriteClipRectAbsolute } },
    { NULL, kInt, { .intval = 0 } }
};
#endif

void he_sprite_init(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    HESprite_clearScreenClipRect();
    
    sprites = array_new();
    visibleSprites = array_new();
    movingSprites = array_new();
    followSprites = array_new();
    spriteCollisions = generic_array_new(sizeof(HESpriteCollision));
    
    HERectF gridRect = rectf_new(0, 0, LCD_COLUMNS, LCD_ROWS);
    
    collisionsGrid = grid_new(gridRect, 64);
    visibilityGrid = grid_new(gridRect, 64);
    
#if HE_LUA_BINDINGS
    if(enableLua)
    {
        playdate->lua->registerClass(lua_kSprite, lua_sprite, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kSpritePublic, lua_spritePublic, lua_spritePublicVal, 1, NULL);
        playdate->lua->registerClass(lua_kSpriteCollision, lua_spriteCollision, NULL, 0, NULL);
    }
#endif
}
