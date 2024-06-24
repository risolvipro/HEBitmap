local gfx <const> = playdate.graphics

-- configs
local jumpStartVelocity <const> = 400
local gravity <const> = 1000
local groundVelocity <const> = 300 
local legsInterval <const> = 0.2
local cactusMinWait <const> = 1
local cactusRandomWait <const> = 1

-- fixed values
local groundY <const> = 178
local speedIncrease <const> = 0.05
local cactusY <const> = groundY + 10 - 32
local dinoX <const> = 36
local dinoStartY <const> = groundY - 30

local jumpVelocity
local isJumping
local speed
local legsTime
local groundX
local state
local cactusTime
local cactusWait
local cactusActive

function reset()
    jumpVelocity = 0
    isJumping = false
    speed = 1
    legsTime = 0
    cactusTime = 0
    cactusWait = 2 -- initial wait
    cactusActive = false
    groundX = 0
    state = "play"
end

reset()

local dinoTable = he.bitmaptable.new("dino/dino")
local groundBitmap = he.bitmap.new("dino/ground")
local cactusBitmap = he.bitmap.new("dino/cactus")

local dino = he.sprite.new()
dino.tag = "dino"
dino:setBitmapFromTable(dinoTable, 1)
dino:setCollisionsEnabled(true)
dino:setCollisionRect(2, 2, 30, 34)
dino:setCollisionType(function(sprite, otherSprite)
    if otherSprite.tag == "ground" then
        return he.sprite.kCollisionTypeFreeze
    end
    return he.sprite.kCollisionTypeOverlap
end)
dino:setCenter(0, 0)
dino:setPosition(dinoX, dinoStartY)
dino:setZIndex(2)
dino:add()

local ground = he.sprite.new()
ground.tag = "ground"
ground:setTileBitmap(groundBitmap)
ground:setCollisionsEnabled(true)
ground:setCollisionRect(0, 2, 400, 50)
ground:setCenter(0, 0)
ground:setPosition(0, groundY)
ground:setSize(400, 13)
ground:add()

local cactus = he.sprite.new()
cactus.tag = "cactus"
cactus:setBitmap(cactusBitmap)
cactus:setVisible(false)
cactus:setCollisionType(function(sprite, otherSprite)
    if otherSprite.tag == "dino" then
        return he.sprite.kCollisionTypeOverlap
    end
    return he.sprite.kCollisionTypeIgnore
end)
cactus:setZIndex(1)
cactus:setCenter(0, 0)
cactus:add()

playdate.display.setRefreshRate(60)

function playdate.update()
    local dt = playdate.getElapsedTime()
    playdate.resetElapsedTime()

    if state == "game-over" then
        dt = 0
    end

    speed += speedIncrease * dt

    if state == "play" then
        if playdate.buttonJustPressed(playdate.kButtonA) and isJumping == false then
            isJumping = true
            jumpVelocity = -jumpStartVelocity
        end
    elseif state == "game-over" then
        if playdate.buttonJustPressed(playdate.kButtonA) then
            -- replay
            reset()

            cactus:setVisible(false)
            cactus:setCollisionsEnabled(false)

            dino:setPosition(dinoX, dinoStartY)
        end
    end

    if isJumping then
        jumpVelocity += gravity * dt
    end

    local legsPeriod = legsInterval / speed * 2
    if isJumping == false then
        legsTime = math.fmod(legsTime + dt, legsPeriod)
    end
    local legsIndex = math.floor(legsTime / legsPeriod + 0.5) + 1
    
    dino:setBitmapFromTable(dinoTable, legsIndex)

    local _, dinoY = dino:getPosition()
    dinoY += jumpVelocity * dt
    dino:moveTo(dinoX, dinoY)

    local groundDelta = groundVelocity * speed * dt

    groundX -= groundDelta
    groundX = math.fmod(groundX, 400)
    ground:setTileOffset(math.floor(groundX + 0.5), 0)

    if cactusActive == false then
        cactusTime += dt
        if cactusTime >= cactusWait then
            -- add cactus 
            cactusActive = true
            cactus:setPosition(playdate.display.getWidth(), cactusY)

            cactus:setVisible(true)
            cactus:setCollisionsEnabled(true)
        end
    else
        local cactusW, _ = cactus:getSize()
        local cactusX, _ = cactus:getPosition()
        cactusX -= groundDelta

        cactus:moveTo(cactusX, cactusY)

        if (cactusX + cactusW) <= 0 then
            cactusActive = false
            cactusTime = 0
            cactusWait = (cactusMinWait + cactusRandomWait * math.random()) / speed
            
            cactus:setVisible(false)
            cactus:setCollisionsEnabled(false)
        end
    end

    he.sprite.move(dt)
    he.sprite.update()

    if state == "play" then
        local collisions, len = he.sprite.getCollisions()

        for i=1, len do
            local collision = collisions[i]
            local sprite = collision:getSprite()
            local otherSprite = collision:getOtherSprite()

            if otherSprite.tag == "ground" then
                -- ground collision
                isJumping = false
                jumpVelocity = 0
            else
                -- game over
                state = "game-over"
            end
        end
    end

    playdate.graphics.clear()
    he.sprite.draw()

    playdate.drawFPS(0, 0)
end