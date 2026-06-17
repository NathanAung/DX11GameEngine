local timer = 0.0

function OnCreate()
    timer = 0.0
end

function OnUpdate(dt)
    timer = timer + dt
    -- Destroy self after 5 seconds
    if timer > 5.0 then
        self:Destroy()
    end
end

function OnDestroy()
    print(self:GetName() .. " despawned.")
end