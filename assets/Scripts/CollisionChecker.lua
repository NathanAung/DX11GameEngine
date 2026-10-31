-- A script for testing the collision detection. 
-- prints a message when OnCollisionEnter or OnCollisionExit is called.

function OnCreate()
	print(self:GetName() .. " initialized CollisionChecker script.")
end

function OnCollisionEnter(other)
	print(self:GetName() .. " collided with " .. other:GetName())
end

function OnCollisionExit(other)
	print(self:GetName() .. " stopped colliding with " .. other:GetName())
end

function OnDestroy()
    print(self:GetName() .. " destroyed CollisionChecker script.")
end