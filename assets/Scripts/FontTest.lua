function OnCreate()
	self:AddTextRenderer()
	self:SetText("Hello World!")
	self:SetFont("assets/Fonts/FiraCode-VariableFont_wght.ttf", 40)
	self:SetTextColor(0.0, 1.0, 0.0, 1.0)
	self:SetTextPosition(50.0, 50.0)
	print("SpriteTest script created!")
end