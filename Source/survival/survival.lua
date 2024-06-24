local gfx <const> = playdate.graphics

local round <const> = function(number)
    return math.floor(number + 0.5)
end

local offsetX <const> = playdate.display.getWidth() / 2
local offsetY <const> = playdate.display.getHeight() / 2
local playerSpeed <const> = 100
local playerWalkInterval <const> = 0.25
local playerWalkPeriod <const> = playerWalkInterval * 2

local playerFace = 0
local playerWalkTime = 0
local playerIsMoving = false
local playerWalkIndex = 1

local grid_w = playdate.display.getWidth() * 2
local grid_h = playdate.display.getHeight() * 2

he.sprite.resizeGrid(-offsetX, -offsetY, grid_w, grid_h, 64)

local playerTable = he.bitmaptable.new("survival/player")
local enemyTable = he.bitmaptable.new("survival/enemy")
local gridBitmap = he.bitmap.new("survival/grid")

local background = he.sprite.new()
background:setCenter(0, 0)
background:setPosition(0, 0)
background:setSize(400, 240)
background:setTileBitmap(gridBitmap)
background:setIgnoresDrawOffset(true)
background:add()

local player = he.sprite.new()
player.tag = "player"
player:setCollisionsEnabled(true)
player:setCollisionRect(4, 2, 24, 30)
player:setCollisionType(he.sprite.kCollisionTypeOverlap)
player:setPosition(playerX, playerY)
player:setZIndex(1000)
player:add()

for i=1, 50 do
    local x = math.random(0, 400) - offsetX
    local y = math.random(0, 240) - offsetY

    local dx = math.random(-80, 80)
    local dy = math.random(-80, 80)

    local velocity = math.random(20, 30)

    local enemy = he.sprite.new()
    enemy.tag = "enemy"
    enemy:setBitmapFromTable(enemyTable, 1)
    enemy:setCenter(0.5, 0.62)
    enemy:setCollisionsEnabled(true)
    enemy:setCollisionRect(7, 8, 19, 24)
    -- kCollisionTypeIgnore is faster than kCollisionTypeOverlap
    enemy:setCollisionType(he.sprite.kCollisionTypeIgnore)
    enemy:setFollowTarget(player)
    enemy:setFollowVelocity(velocity)
    enemy:setFollowRefreshRate(1)
    enemy:setFollowOffset(dx, dy)
    enemy:setPosition(x, y)
    enemy:setZIndex(i)
    enemy:add()
end

playdate.display.setRefreshRate(60)

function playdate.update()
    local dt = playdate.getElapsedTime()
    playdate.resetElapsedTime()
    
    playerIsMoving = false
    
    local playerX, playerY = player:getPosition()

    if playdate.buttonIsPressed(playdate.kButtonLeft) then
        playerX -= playerSpeed * dt
        playerFace = 1
        playerIsMoving = true
    elseif playdate.buttonIsPressed(playdate.kButtonRight) then
        playerX += playerSpeed * dt
        playerFace = 2
        playerIsMoving = true
    elseif playdate.buttonIsPressed(playdate.kButtonUp) then
        playerY -= playerSpeed * dt
        playerFace = 3
        playerIsMoving = true
    elseif playdate.buttonIsPressed(playdate.kButtonDown) then
        playerY += playerSpeed * dt
        playerFace = 0
        playerIsMoving = true
    end

    he.sprite.setDrawOffset(round(-playerX + offsetX), round(-playerY + offsetY))
    background:setTileOffset(round(-playerX), round(-playerY))

    playerWalkIndex = 1
    if playerIsMoving then
        playerWalkTime = math.fmod(playerWalkTime + dt, playerWalkPeriod)
        playerWalkIndex = round(playerWalkTime / playerWalkPeriod)

        if playerWalkIndex == 1 then
            -- remap index
            playerWalkIndex = 2
        end
    end
    
    local playerTableIndex = playerFace * 3 + playerWalkIndex + 1
    player:setBitmapFromTable(playerTable, playerTableIndex)

    player:moveTo(playerX, playerY)

    he.sprite.move(dt)
    he.sprite.update()

    playdate.graphics.clear()
    he.sprite.draw()

    playdate.drawFPS(0, 0)
end