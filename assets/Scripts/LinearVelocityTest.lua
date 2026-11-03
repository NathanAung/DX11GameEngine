-- A script for testing the linear velocity of physics objects.
-- SetLinearVelocity and GetLinearVelocity are used to set and get the linear velocity of the object.

function OnCreate()
	print(self:GetName() .. " initialized LinearVelocityTest script.")
end

function OnUpdate(dt)
	-- Set the linear velocity of the object to (1, 0, 0) every frame
	if self:HasRigidBody() then
		self:SetLinearVelocity(1, 0, 0)
	end
	-- Get the linear velocity of the object and print it
	local velocity = self:GetLinearVelocity()

	print(
        self:GetName() ..
        " linear velocity: (" ..
        velocity.x .. ", " ..
        velocity.y .. ", " ..
        velocity.z .. ")"
    )
end