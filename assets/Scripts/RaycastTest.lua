-- This script demonstrates how to perform a raycast in the direction the entity is facing.

function OnCreate()
    print("Raycast script initialized on: " .. self:GetName())
end

function OnUpdate(dt)
    -- Fire a raycast only when Space is pressed to avoid spamming the console
    if Input:IsKeyPressed(Key.Space) then
        local pos = self:GetTransform().position
        local fwd = self:GetForwardVector()
        
        -- Offset the ray origin slightly forward so we don't hit our own collider
        local originX = pos.x + (fwd.x * 0.6)
        local originY = pos.y + (fwd.y * 0.6)
        local originZ = pos.z + (fwd.z * 0.6)
        
        print("Casting ray forward...")
        
        -- Cast the ray in the direction we are facing
        local hitEntity = Physics.CastRay(originX, originY, originZ, fwd.x, fwd.y, fwd.z)
        
        if hitEntity ~= nil then
            print("Ray hit object: " .. hitEntity:GetName())
        else
            print("Ray hit nothing.")
        end
    end
end

function OnDestroy()
end