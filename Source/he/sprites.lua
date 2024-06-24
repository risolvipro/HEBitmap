local spriteLib <const> = he.sprite
local sprites <const> = {}

class('HESprite').extends()
spriteLib.new = HESprite

function HESprite:init()
    HESprite.super.init(self)
    self._sprite = he._sprite.new()
    self.drawCallback = nil
    self.updateCallback = nil
    self.collisionTypeCallback = nil
    self.tag = nil
end

function HESprite:resetType()
    self.drawCallback = nil
end

function HESprite:setEmpty()
    self:resetType()
    self._sprite:setEmpty()
end

function HESprite:setBitmap(bitmap)
    self:resetType()
    self._sprite:setBitmap(bitmap)
end

function HESprite:setTileBitmap(bitmap)
    self:resetType()
    self._sprite:setTileBitmap(bitmap)
end

function HESprite:setBitmapFromTable(table, i)
    self:resetType()
    self._sprite:setBitmapFromTable(table, i)
end

function HESprite:setTileOffset(dx, dy)
    self._sprite:setTileOffset(dx, dy)
end

function HESprite:setDrawCallback(callback)
    self:resetType()
    self.drawCallback = callback
    self._sprite:setDrawCallback()
end

function HESprite:moveTo(x, y)
    self._sprite:moveTo(x, y)
end

function HESprite:setPosition(x, y)
    self._sprite:setPosition(x, y)
end

function HESprite:getPosition()
    return self._sprite:getPosition()
end

function HESprite:setSize(w, h)
    self._sprite:setSize(w, h)
end

function HESprite:getSize()
    return self._sprite:getSize()
end

function HESprite:setCenter(cx, cy)
    self._sprite:setCenter(cx, cy)
end

function HESprite:setCollisionRect(x, y, w, h)
    self._sprite:setCollisionRect(x, y, w, h)
end

function HESprite:clearCollisionRect()
    self._sprite:clearCollisionRect()
end

function HESprite:setCollisionType(value)
    if type(value) == "function" then
        self.collisionTypeCallback = value
        self._sprite:setCollisionTypeCallback(value)
    else
        self.collisionTypeCallback = nil
        self._sprite:setCollisionType(value)
    end
end

function HESprite:cacheCollisionType(flag)
    self._sprite:cacheCollisionType(flag)
end

function HESprite:collisionTypeIsBeingCached()
    return self._sprite:collisionTypeIsBeingCached()
end

function HESprite:invalidateCollisionType()
    return self._sprite:invalidateCollisionType()
end

function HESprite:setUpdateCallback(callback)
    self.updateCallback = callback
    local flag = false
    if callback then
        flag = true
    end
    self._sprite:setUpdateCallback(flag)
end

function HESprite:setIgnoresDrawOffset(flag)
    self._sprite:setIgnoresDrawOffset(flag)
end

function HESprite:setZIndex(z)
    self._sprite:setZIndex(z)
end

function HESprite:setVisible(flag)
    self._sprite:setVisible(flag)
end

function HESprite:isVisible()
    return self._sprite:isVisible()
end

function HESprite:setCollisionsEnabled(flag)
    return self._sprite:setCollisionsEnabled(flag)
end

function HESprite:setClipRect(x, y, width, height)
    return self._sprite:setClipRect(x, y, width, height)
end

function HESprite:clearClipRect()
    return self._sprite:clearClipRect()
end

function HESprite:setClipRectReference(reference)
    return self._sprite:setClipRectReference(reference)
end

function HESprite:setIgnoresScreenClip()
    return self._sprite:setIgnoresScreenClip()
end

function HESprite:setFollowTarget(target)
    return self._sprite:setFollowTarget(target._sprite)
end

function HESprite:setFollowVelocity(velocity)
    return self._sprite:setFollowVelocity(velocity)
end

function HESprite:setFollowRefreshRate(rate)
    return self._sprite:setFollowRefreshRate(rate)
end

function HESprite:setFollowOffset(dx, dy)
    return self._sprite:setFollowOffset(dx, dy)
end

function HESprite:setFastCollisions(flag)
    return self._sprite:setFastCollisions(flag)
end

function HESprite:checkCollisions()
    return self._sprite:checkCollisions()
end

function HESprite:setNeedsCollisions()
    return self._sprite:setNeedsCollisions()
end

function HESprite:getCollisionType()
    local type, isCallback = self._sprite:getCollisionType()
    if isCallback then
        return self.collisionTypeCallback
    end
    return type
end

function HESprite:add()
    local index = self._sprite:add()
    if index >= 0 then
        table.insert(sprites, self)
    end
end

function HESprite:remove()
    local index = self._sprite:remove()
    if index >= 0 then
        table.remove(sprites, index + 1)
    end
end

spriteLib.getAll = function()
    return sprites
end

spriteLib.removeAll = function()
    he.sprite._removeAll()

    for k, _ in pairs(sprites) do
        sprites[k] = nil
    end
end

function he.spritecollision:getSprite()
    local i = self:_getSprite1()
    return sprites[i]
end

function he.spritecollision:getOtherSprite()
    local i = self:_getSprite2()
    return sprites[i]
end

function he_callSpriteDraw(i, x, y, w, h)
    local sprite = sprites[i]
    if sprite and sprite.drawCallback then
        sprite.drawCallback(sprite, x, y, w, h)
    end
end

function he_callSpriteUpdate(i)
    local sprite = sprites[i]
    if sprite and sprite.updateCallback then
        sprite.updateCallback(sprite)
    end
end

function he_callSpriteCollisionType(i1, i2)
    local sprite = sprites[i1]
    local otherSprite = sprites[i2]

    if sprite and otherSprite and sprite.collisionTypeCallback then
        local type = sprite.collisionTypeCallback(sprite, otherSprite)
        -- workaround to avoid stack overflow
        sprite._sprite:setCollisionTypeResult(type)
    end
end