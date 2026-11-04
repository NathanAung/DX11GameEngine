-- this script is for testing the global delta time retreival, and the restart and quit functions

function OnCreate()
	print("dtRestartQuitTest script initialized on: " .. self:GetName())
end

function OnUpdate(dt)
	-- Get the global delta time and print it
	print(self:GetName() .. " global delta time: " .. Time.deltaTime)
	-- Check for input to restart or quit the application
	if Input:IsKeyPressed(Key.R) then
		print("Restarting application...")
		Game.Restart()
	elseif Input:IsKeyPressed(Key.Q) then
		print("Quitting application...")
		Game.Quit()
	end
end