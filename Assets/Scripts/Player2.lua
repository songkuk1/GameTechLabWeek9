-- Player2 (우사기). ↑/↓ = +X / -X (카운터 쪽 / 입구 쪽), ←/→ = -Y / +Y (화면 왼쪽 / 오른쪽), Enter = 밀치기
-- 이동·회전은 Lib/PlayerController.lua 공통 코드를 쓴다. 벽·상대와의 충돌은 씬의 CollisionBox(UBoxComponent)가 맡는다.

local GameState = require("Assets.Scripts.Lib.GameState")

local Controller = dofile("Assets/Scripts/Lib/PlayerController.lua")({
	Forward = "Up", Back = "Down", Left = "Left", Right = "Right",
	PlayerNumber = 2, PushKey = "Enter",   -- 앞의 상대를 밀친다 (쿨타임 2초)
	MeshYawOffset = 0,     -- usagi_chiikawa.obj는 정면이 +X를 보도록 변환했다
	SpawnOffsetY = 1.0,    -- 시작 위치: 바닥 중앙에서 화면 오른쪽으로
})

function BeginPlay()
	GameState.RegisterPlayer(2, obj)
	Controller.BeginPlay(obj, GetPlayerController(1), EnableInput(1))
end

function Tick(dt)
	Controller.Tick(dt)
end

function EndPlay()
	Controller.EndPlay()
end

function OnOverlap(OtherActor)
end
