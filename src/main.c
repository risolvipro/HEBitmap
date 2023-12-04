//
//  main.h
//  hebitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "hebitmap.h"

#define ENTITY_COUNT 1000

typedef struct {
    float x;
    float y;
    int dirX;
    int dirY;
} Entity;

static PlaydateAPI *playdate;
static HEBitmap *he_bitmap;
static LCDBitmap *lcd_bitmap;
static int use_sdk = 0;
static int debug_mode = 0;
static Entity* entities[ENTITY_COUNT];
static float velocity = 20;
static float x_delta = 0;
static float y_delta = 0;
static PDMenuItem *debugMenuItem;

static int update(void* userdata);
static void debugModeCallback(void *userdata);

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if(event == kEventInit)
    {
        playdate = pd;
        
        playdate->display->setRefreshRate(0);
        
        HEBitmapSetPlaydateAPI(pd);
        
        lcd_bitmap = playdate->graphics->loadBitmap("dvd", NULL);
        he_bitmap = HEBitmapNew(lcd_bitmap);
        
        for(int i = 0; i < ENTITY_COUNT; i++)
        {
            Entity *entity = playdate->system->realloc(NULL, sizeof(Entity));
            
            entity->x = rand() % (LCD_COLUMNS - he_bitmap->width + 1);
            entity->y = rand() % (LCD_ROWS - he_bitmap->height + 1);
            
            entity->dirX = rand() % 2;
            entity->dirY = rand() % 2;
            
            entities[i] = entity;
        }
        
        debugMenuItem = playdate->system->addCheckmarkMenuItem("Debug", debug_mode, debugModeCallback, NULL);
        
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}

static int update(void *userdata)
{
    float dt = playdate->system->getElapsedTime();
    playdate->system->resetElapsedTime();
    
    PDButtons pressed;
    PDButtons pushed;
    playdate->system->getButtonState(&pressed, &pushed, NULL);
    
    if(pushed & kButtonA)
    {
        use_sdk = !use_sdk;
    }
    
    playdate->graphics->clear(kColorWhite);
    
    if(!debug_mode)
    {
        float translation = velocity * dt;
        
        int max_x = LCD_COLUMNS - 1 - he_bitmap->width;
        int max_y = LCD_ROWS - 1 - he_bitmap->height;
        
        for(int i = 0; i < ENTITY_COUNT; i++)
        {
            Entity *entity = entities[i];
            
            entity->x += (entity->dirX ? translation : -translation);
            entity->y += (entity->dirY ? translation : -translation);
            
            int x1 = roundf(entity->x);
            int y1 = roundf(entity->y);
            
            int x2 = x1 + he_bitmap->width;
            int y2 = y1 + he_bitmap->height;
            
            if(x1 < 0)
            {
                entity->x = 0;
                entity->dirX = !entity->dirX;
            }
            else if(x2 >= LCD_COLUMNS)
            {
                entity->x = max_x;
                entity->dirX = !entity->dirX;
            }
            
            if(y1 < 0)
            {
                entity->y = 0;
                entity->dirY = !entity->dirY;
            }
            else if(y2 >= LCD_ROWS)
            {
                entity->y = max_y;
                entity->dirY = !entity->dirY;
            }
            
            int x = roundf(entity->x);
            int y = roundf(entity->y);
            
            if(use_sdk)
            {
                playdate->graphics->drawBitmap(lcd_bitmap, x, y, kBitmapUnflipped);
            }
            else
            {
                HEBitmapDraw(he_bitmap, x, y);
            }
        }
    }
    else
    {
        int delta = roundf(100 * dt);
        
        if(pressed & kButtonLeft)
        {
            x_delta -= delta;
        }
        else if(pressed & kButtonRight)
        {
            x_delta += delta;
        }
        
        if(pressed & kButtonUp)
        {
            y_delta -= delta;
        }
        else if(pressed & kButtonDown)
        {
            y_delta += delta;
        }
        
        int x = x_delta;
        int y = LCD_ROWS / 2 - he_bitmap->height / 2 + y_delta;
        
        if(use_sdk)
        {
            playdate->graphics->drawBitmap(lcd_bitmap, x, y, kBitmapUnflipped);
        }
        else
        {
            HEBitmapDraw(he_bitmap, x, y);
        }
    }
    
    playdate->system->drawFPS(0, 0);
    
	return 1;
}

void debugModeCallback(void *userdata)
{
    debug_mode = playdate->system->getMenuItemValue(debugMenuItem);
}
