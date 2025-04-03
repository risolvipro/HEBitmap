//
//  he_api.h
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#ifndef he_api_h
#define he_api_h

#ifndef HE_GFX_STACK_SIZE
#define HE_GFX_STACK_SIZE 1024
#endif

#include "pd_api.h"
#include "he_foundation.h"
#include "he_bitmap.h"

void he_library_init(PlaydateAPI *pd);

//
// Graphics
//
void he_graphics_pushContext(void);
void he_graphics_popContext(void);
void he_graphics_setClipRect(int x, int y, int width, int height);
void he_graphics_clearClipRect(void);
HERect he_graphics_getClipRect(void);

#endif /* he_api_h */
