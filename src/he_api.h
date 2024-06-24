//
//  he_api.h
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#ifndef he_api_h
#define he_api_h

#define HE_SPRITE_MODULE

#include "pd_api.h"
#include "he_foundation.h"
#include "he_bitmap.h"
#ifdef HE_SPRITE_MODULE
#include "he_sprite.h"
#endif

void he_library_init(PlaydateAPI *pd, int enableLua);

//
// Graphics
//
void he_graphics_pushContext(void);
void he_graphics_popContext(void);
void he_graphics_setClipRect(int x, int y, int width, int height);
void he_graphics_clearClipRect(void);

#endif /* he_api_h */
