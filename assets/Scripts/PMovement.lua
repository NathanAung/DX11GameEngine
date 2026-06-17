-- PMovement.lua
local moveForce = 0.1
local jumpForce = 5.0

function OnCreate()
    print(self:GetName() .. " initialized PMovement script.")
end

function OnUpdate(dt)
    local dx = 0
    local dz = 0

    -- Check WASD Input for directional movement
    if Input:IsKeyDown(Key.W) then dz = dz + 1 end
    if Input:IsKeyDown(Key.S) then dz = dz - 1 end
    if Input:IsKeyDown(Key.A) then dx = dx - 1 end
    if Input:IsKeyDown(Key.D) then dx = dx + 1 end

    -- Apply a continuous small impulse to act as a moving force
    if dx ~= 0 or dz ~= 0 then
        self:ApplyLinearImpulse(dx * moveForce, 0, dz * moveForce)
    end

    -- Check Spacebar for jumping
    if Input:IsKeyPressed(Key.Space) then
        self:ApplyLinearImpulse(0, jumpForce, 0)
    end
end

function OnDestroy()
    print(self:GetName() .. " destroyed PMovement script.")
end