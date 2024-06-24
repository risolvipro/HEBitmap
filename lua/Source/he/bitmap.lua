import "CoreLibs/object"

local gfx <const> = playdate.graphics

function he.bitmap:colorAt(x, y)
    local color = self:_colorAt(x, y)
    if color == 1 then
       return gfx.kColorBlack
    elseif color == 2 then
        return gfx.kColorWhite
    end
    return gfx.kColorClear
end