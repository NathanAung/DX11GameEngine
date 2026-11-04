-- this script is used for testing model and texture addition functions
-- the mesh renderer component is added to the entity, and a model and texture are assigned to it

function OnCreate()
end

function OnUpdate(dt)
	if Input:IsKeyPressed(Key.Space) then
		self:AddMeshRenderer(MeshType.Custom)
		-- assign a model to the mesh renderer
		self:SetMesh("assets/Models/container/model/container.obj?mesh=0")
		-- assign a texture to the mesh renderer
		self:SetAlbedo("assets/Models/container/textures/Container _BaseColor.png")
		print("Model and texture assigned to entity: " .. self:GetName())
    end
end