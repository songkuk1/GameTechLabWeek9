-- Player1 (치이카와). W/S = +X / -X (카운터 쪽 / 입구 쪽), A/D = -Y / +Y (화면 왼쪽 / 오른쪽)
-- 이동·회전은 Lib/PlayerController.lua 공통 코드를 쓴다. 벽·상대와의 충돌은 씬의 CollisionBox(UBoxComponent)가 맡는다.

local Match = require("Assets.Scripts.Lib.PopcornMatch")
local Controller = dofile("Assets/Scripts/Lib/PlayerController.lua")({
	Forward = "W", Back = "S", Left = "A", Right = "D",
	MeshYawOffset = 180,   -- 메시 정면이 -X를 보고 있어서 180도 돌려 맞춘다
	SpawnOffsetY = -1.0,   -- 시작 위치: 바닥 중앙에서 화면 왼쪽으로
})

function BeginPlay()
	Controller.BeginPlay(obj)
	Match.RegisterPlayer(1, obj)
end

function Tick(dt)
	if not Match.IsFinished() then Controller.Tick(dt) end
end

function EndPlay()
	Match.UnregisterPlayer(1, obj)
	Controller.EndPlay()
end

function OnOverlap(OtherActor)
end
