-- BGM·효과음(SFX) 음량. 일시정지 창 슬라이더가 바꾸고, 엔진 사운드 매니저(Audio)에 반영한다.
--   local Sound = require("Assets.Scripts.Lib.Sound")
-- require는 처음 한 번만 실행하므로 씬이 바뀌어도 값이 유지된다.
-- (require는 파일을 다시 읽지 않으므로 이 파일을 고치면 에디터를 다시 켜야 반영된다)
--
-- 슬라이더 값(0~1)은 엔진 기본 음량에 곱하는 비율이다. 1이면 기본 음량(BGM 0.2, SFX 1.0) 그대로, 0이면 무음.

local Sound = {
	Volumes = { BGM = 1.0, SFX = 1.0 },   -- 슬라이더 값

	-- 슬라이더 1일 때의 음량. 처음 불러올 때 엔진 기본값을 읽어 둔다
	BaseVolumes = { BGM = Audio.GetBGMVolume(), SFX = Audio.GetSFXVolume() },
	Setters = { BGM = Audio.SetBGMVolume, SFX = Audio.SetSFXVolume },
}

-- Channel: "BGM" | "SFX"
function Sound.GetVolume(Channel)
	return Sound.Volumes[Channel]
end

function Sound.SetVolume(Channel, Value)
	Value = math.max(0, math.min(1, Value))
	Sound.Volumes[Channel] = Value
	Sound.Setters[Channel](Sound.BaseVolumes[Channel] * Value)
end

return Sound
