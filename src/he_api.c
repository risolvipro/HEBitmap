//
//  he_api.c
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "he_api.h"
#include "he_prv.h"

static PlaydateAPI *playdate;

static HEGraphicsContext gfx_stack[HE_GFX_STACK_SIZE] = {0};
static int gfx_stack_index = 0;

//
// Graphics
//
void he_graphics_pushContext(void)
{
    if((gfx_stack_index + 1) < HE_GFX_STACK_SIZE)
    {
        HEGraphicsContext *prevContext = he_graphics_context;
        gfx_stack_index++;
        he_graphics_context = &gfx_stack[gfx_stack_index];
        *he_graphics_context = *prevContext;
    }
}

void he_graphics_popContext(void)
{
    if(gfx_stack_index > 0)
    {
        gfx_stack_index--;
        he_graphics_context = &gfx_stack[gfx_stack_index];
    }
}

void he_graphics_setClipRect(int x, int y, int width, int height)
{
    he_graphics_context->_clipRect = he_rect_new(x, y, width, height);
    he_graphics_context->clipRect = he_rect_intersection(he_gfx_screenRect, he_graphics_context->_clipRect);
}

void he_graphics_clearClipRect(void)
{
    he_graphics_context->_clipRect = he_gfx_screenRect;
    he_graphics_context->clipRect = he_graphics_context->_clipRect;
}

HERect he_graphics_getClipRect(void){
    return he_graphics_context->_clipRect;
}

// Forward declarations
void he_bitmap_init(PlaydateAPI *pd);
void he_prv_init(PlaydateAPI *pd);

void he_library_init(PlaydateAPI *pd)
{
    playdate = pd;
    
    gfx_stack_index = 0;
    
    gfx_stack[gfx_stack_index] = (HEGraphicsContext){
        ._clipRect = he_gfx_screenRect,
        .clipRect = he_gfx_screenRect
    };
    he_graphics_context = &gfx_stack[gfx_stack_index];
    
    he_graphics_clearClipRect();
    
    he_prv_init(pd);
    he_bitmap_init(pd);
}
