-- BGMPlayer.lua
function OnCreate()
    -- AddAudioComponent(filepath, is3D, loop, playOnCreate)
    -- We set is3D to 'false' so the music ignores distance and camera panning!
    self:AddAudioComponent("assets/audio/game_clear.mp3", false, true, true)
    print("Background Music Initialized!")
end

function OnUpdate(dt)
end

function OnDestroy()
end