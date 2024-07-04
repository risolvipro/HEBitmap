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
    he_graphics_context->_clipRect = rect_new(x, y, width, height);
    he_graphics_context->clipRect = rect_intersection(gfx_screenRect, he_graphics_context->_clipRect);
}

void he_graphics_clearClipRect(void)
{
    he_graphics_context->_clipRect = gfx_screenRect;
    he_graphics_context->clipRect = he_graphics_context->_clipRect;
}

#if HE_LUA_BINDINGS
//
// Lua bindings
//
static int lua_setClipRect(lua_State *L)
{
    int x = playdate->lua->getArgFloat(1);
    int y = playdate->lua->getArgFloat(2);
    int width = playdate->lua->getArgFloat(3);
    int height = playdate->lua->getArgFloat(4);
    he_graphics_setClipRect(x, y, width, height);
    return 0;
}

static int lua_clearClipRect(lua_State *L)
{
    he_graphics_clearClipRect();
    return 0;
}

static const lua_reg lua_graphics[] = {
    { "setClipRect", lua_setClipRect },
    { "clearClipRect", lua_clearClipRect },
    { NULL, NULL }
};
#endif

// Forward declarations
void he_bitmap_init(PlaydateAPI *pd, int enableLua);
#if HE_SPRITE_MODULE
void he_sprite_init(PlaydateAPI *pd, int enableLua);
#endif
void he_prv_init(PlaydateAPI *pd, int enableLua);

void he_library_init(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    
    gfx_stack_index = 0;
    
    gfx_stack[gfx_stack_index] = (HEGraphicsContext){
        ._clipRect = gfx_screenRect,
        .clipRect = gfx_screenRect
    };
    he_graphics_context = &gfx_stack[gfx_stack_index];
    
    he_graphics_clearClipRect();
    
#if HE_LUA_BINDINGS
    if(enableLua)
    {
        playdate->lua->registerClass(lua_kGraphics, lua_graphics, NULL, 1, NULL);
    }
#endif
    
    he_prv_init(pd, enableLua);
    
    he_bitmap_init(pd, enableLua);
#if HE_SPRITE_MODULE
    he_sprite_init(pd, enableLua);
#endif
}
