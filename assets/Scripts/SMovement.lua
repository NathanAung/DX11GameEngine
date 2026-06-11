-- SMovement.lua
local speed = 10.0

function OnCreate()
    print(self:GetName() .. " initialized SMovement script.")
end

function OnUpdate(dt)
    local transform = self:GetTransform()
    local pos = transform.position

    local dx = 0
    local dz = 0

    -- Check WASD Input
    if Input:IsKeyDown(Key.W) then dz = dz + 1 end
    if Input:IsKeyDown(Key.S) then dz = dz - 1 end
    if Input:IsKeyDown(Key.A) then dx = dx - 1 end
    if Input:IsKeyDown(Key.D) then dx = dx + 1 end

    -- Apply movement using the explicit SetPosition method to trigger the isDirty flag
    if dx ~= 0 or dz ~= 0 then
        transform:SetPosition(pos.x + (dx * speed * dt), pos.y, pos.z + (dz * speed * dt))
    end
end

function OnDestroy()
    print(self:GetName() .. " destroyed SMovement script.")
end