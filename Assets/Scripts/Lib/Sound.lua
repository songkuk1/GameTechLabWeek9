-- 음량 설정. 일시정지 창 슬라이더와 (나중에) 사운드 재생 쪽이 같은 값을 봐야 해서 require로 불러 쓴다.
--   local Sound = require("Assets.Scripts.Lib.Sound")
-- 사운드 매니저가 생기면 SetVolume 안에서 엔진의 음량 함수를 부르도록 연결한다.
-- (require는 파일을 다시 읽지 않으므로 이 파일을 고치면 에디터를 다시 켜야 반영된다)

local Sound = {
	Volume = 1.0,   -- 전체 음량 (0~1)
}

function Sound.SetVolume(Value)
	Sound.Volume = math.max(0, math.min(1, Value))
	-- TODO: 사운드 매니저 연결 (예: SetMasterVolume(Sound.Volume))
end

return Sound
