-- script for testing finding and reparenting an entity in the scene

function OnCreate()
    print("Reparent script initialized on: " .. self:GetName())
    local childEntity = Scene.FindEntityByName("ChildEntity")
    if(childEntity ~= nil) then
        print("Found child entity: " .. childEntity:GetName())
        childEntity:SetParent(self)
        print("Child entity reparented to: " .. self:GetName())
    else
        print("Child entity not found.")
    end
end

function OnUpdate(dt)
end

function OnDestroy()
end