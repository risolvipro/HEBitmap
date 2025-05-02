//
//  he_foundation.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#ifndef he_foundation_h
#define he_foundation_h

typedef struct HERect {
    int x;
    int y;
    int width;
    int height;
} HERect;

HERect he_rect_new(int x, int y, int width, int height);
HERect he_rect_zero(void);

#endif /* he_foundation_h */
