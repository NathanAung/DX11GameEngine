function OnUpdate(deltaTime)
	if Input:IsKeyPressed(Key.Space) then
		self:SetLightColor(0.0, 0.0, 1.0) -- Set light color to blue)
		self:SetLightIntensity(100.0) -- Set light intensity to 100.0
		print("Light color set to blue and intensity set to 100.0 for entity: " .. self:GetName())
    end
end