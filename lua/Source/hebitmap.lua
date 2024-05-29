-- HEBitmap lua

local gfx = playdate.graphics

function hebitmap.bitmap:colorAt(x, y)
    local color = self:_colorAt(x, y)
    if color == 1 then
       return gfx.kColorBlack
    elseif color == 2 then
        return gfx.kColorWhite
    end
    return gfx.kColorClear
end