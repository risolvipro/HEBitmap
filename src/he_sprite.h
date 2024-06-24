//
//  he_sprite.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#ifndef he_sprite_h
#define he_sprite_h

#include "he_foundation.h"
#include "he_bitmap.h"

typedef enum {
    HESpriteCollisionTypeSlide,
    HESpriteCollisionTypeFreeze,
    HESpriteCollisionTypeOverlap,
    HESpriteCollisionTypeBounce,
    HESpriteCollisionTypeIgnore
} HESpriteCollisionType;

typedef struct HESprite {
    void *prv;
} HESprite;

typedef struct HESpriteCollision {
    HESpriteCollisionType type;
    HESprite *sprite;
    HESprite *otherSprite;
    HEVec2 normal;
    HEVec2 touch;
    HEVec2 goal;
    HERectF rect;
    HERectF otherRect;
} HESpriteCollision;

typedef enum {
    HESpriteClipRectRelative,
    HESpriteClipRectAbsolute
} HESpriteClipRectReference;

//
// Callbacks
//
typedef void(HESpriteUpdateCallback)(HESprite *sprite);
typedef void(HESpriteDrawCallback)(HESprite *sprite, HERect rect);
typedef HESpriteCollisionType(HESpriteCollisionTypeCallback)(HESprite *sprite, HESprite *other);

//
// Sprite system
//
HESprite* HESprite_new(void);
void HESprite_add(HESprite *sprite);
void HESprite_remove(HESprite *sprite);
void HESprite_setIgnoresDrawOffset(HESprite *sprite, int flag);
int HESprite_isVisible(HESprite *sprite);
void HESprite_setSize(HESprite *sprite, float width, float height);
void HESprite_setCenter(HESprite *sprite, float cx, float cy);
void HESprite_setPosition(HESprite *sprite, float x, float y);
void HESprite_setZIndex(HESprite *sprite, int z);
void HESprite_moveTo(HESprite *sprite, float x, float y);
void HESprite_getSize(HESprite *sprite, float *width, float *height);
HEVec2 HESprite_getPosition(HESprite *sprite);
void HESprite_setTileBitmap(HESprite *sprite, HEBitmap *bitmap);
void HESprite_setTileOffset(HESprite *sprite, int dx, int dy);
void HESprite_setEmpty(HESprite *sprite);
void HESprite_setBitmap(HESprite *sprite, HEBitmap *bitmap);
void HESprite_setDrawCallback(HESprite *sprite, HESpriteDrawCallback *callback);
void HESprite_setCollisionType(HESprite *sprite, HESpriteCollisionType type);
void HESprite_setCollisionTypeCallback(HESprite *sprite, HESpriteCollisionTypeCallback *callback);
HESpriteCollisionType HESprite_getCollisionType(HESprite *sprite, int *isCallback);
void HESprite_invalidateCollisionType(HESprite *sprite);
void HESprite_cacheCollisionType(HESprite *sprite, int flag);
int HESprite_collisionTypeIsBeingCached(HESprite *sprite);
void HESprite_setUserdata(HESprite *sprite, void *userdata);
void* HESprite_getUserdata(HESprite *sprite);
void HESprite_setUpdateCallback(HESprite *sprite, HESpriteUpdateCallback *callback);
void HESprite_setCollisionsEnabled(HESprite *sprite, int flag);
void HESprite_setVisible(HESprite *sprite, int flag);
int HESprite_isVisibleOnScreen(HESprite *sprite);
void HESprite_setNeedsCollisions(HESprite *sprite);
void HESprite_setClipRect(HESprite *sprite, int x, int y, int width, int height);
void HESprite_setClipRectReference(HESprite *sprite, HESpriteClipRectReference reference);
void HESprite_clearClipRect(HESprite *sprite);
void HESprite_setIgnoresScreenClipRect(HESprite *sprite, int flag);
void HESprite_setFollowTarget(HESprite *sprite, HESprite *target);
void HESprite_setFollowVelocity(HESprite *sprite, float velocity);
void HESprite_setFollowOffset(HESprite *sprite, float dx, float dy);
void HESprite_setFollowRefreshRate(HESprite *sprite, float delay);
HESpriteCollision* HESprite_checkCollisions(HESprite *sprite, int *len);
void HESprite_setFastCollisions(HESprite *sprite, int flag);
void HESprite_free(HESprite *sprite);

void he_sprites_move(float deltaTime);
void he_sprites_update(void);
void he_sprites_draw(void);

void he_sprites_setDrawOffset(int dx, int dy);
void HESprite_setScreenClipRect(int x, int y, int width, int height);
HESprite** he_sprites_getAll(int *len);
void he_sprites_removeAll(void);
void he_sprites_resizeGrid(float x, float y, float width, float height, float cellSize);

HESpriteCollision* he_sprites_getCollisions(int *len);
HESprite** he_sprites_queryWithRect(float x, float y, float width, float height, int *len);
HESprite** he_sprites_queryWithSegment(float x1, float y1, float x2, float y2, int *len);
HESprite** he_sprites_queryWithPoint(float x, float y, int *len);

#endif /* he_sprite_h */
