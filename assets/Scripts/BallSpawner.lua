-- BallSpawner.lua
function OnCreate()
    print("Spawner Ready! Press Space to spawn a bouncing sphere, or C to spawn a heavy cube.")
end

function OnUpdate(dt)
    -- Spawn a Sphere
    if Input:IsKeyPressed(Key.Space) then
        local ball = self:Instantiate("Bouncing Ball")
        local pos = self:GetTransform().position
        ball:GetTransform():SetPosition(pos.x, pos.y + 2.0, pos.z)
        
        ball:AddMeshRenderer(MeshType.Sphere)
        ball:AddRigidBody(RBShape.Sphere, RBMotion.Dynamic, 0.5, 1.0, 0.8) 
        ball:AddLuaScript("assets/scripts/BallDestroy.lua")

        ball:AddAudioComponent("assets/audio/spawn.mp3", true, false, true)
    end

    -- Spawn a Cube
    if Input:IsKeyPressed(Key.C) then
        local box = self:Instantiate("Heavy Box")
        local pos = self:GetTransform().position
        box:GetTransform():SetPosition(pos.x, pos.y + 2.0, pos.z)
        
        box:AddMeshRenderer(MeshType.Cube)
        -- Uses half-extent of 0.5, mass of 5.0, no bounce
        box:AddRigidBody(RBShape.Box, RBMotion.Dynamic, 0.5, 5.0, 0.0) 
        box:AddLuaScript("assets/scripts/BallDestroy.lua")
    end
end

function OnDestroy()
end