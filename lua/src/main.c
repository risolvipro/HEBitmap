//
//  main.h
//  hebitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "pd_api.h"
#include "he_api.h"

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
    (void)arg; // arg is currently only used for event = kEventKeyPressed
    
    if(event == kEventInitLua)
    {
        he_library_init(pd, 1);
    }
    
    return 0;
}