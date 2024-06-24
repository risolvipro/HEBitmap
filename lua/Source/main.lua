import "he/bitmap"

local bitmap = he.bitmap.new("catbus")
-- local bitmap = he.bitmap.loadHEB("catbus.heb")

function playdate.update()
    local w, h = bitmap:getSize()
    local display_width, display_height = playdate.display.getSize()
    --he.graphics.setClipRect(200, 0, 200, 240)
	bitmap:draw((display_width - w) / 2, (display_height - h) / 2)
end