-- 플레이어 공통 이동: 키 입력 이동 + 이동 방향 회전(lerp)
-- 충돌은 엔진 Box 컴포넌트가 맡는다. 플레이어의 CollisionBox와 씬의 벽(Wall*)이 모두 bBlockComponent라서
-- AddActorWorldOffset(Delta, true)로 옮기면 엔진이 Sweep해 벽·다른 플레이어 앞에서 멈춘다.
-- 플레이어 스크립트에서 dofile로 불러 Config를 넘겨 만든다.
--   local Controller = dofile("Assets/Scripts/Lib/PlayerController.lua")({ Forward = "W", Back = "S", Left = "A", Right = "D" })
--
-- 입력은 키를 직접 읽지 않고 엔진의 PlayerController에서 받는다.
--   포커스된 뷰포트 → PlayerController(키 매핑) → 이 액터의 InputComponent → BindAxis 함수
-- dofile로 읽은 파일은 플레이어 스크립트의 environment를 모르므로 EnableInput·GetPlayerController 결과를 BeginPlay로 넘겨받는다.

-- Lua 5.1·LuaJIT는 math.atan2, 5.3 이상은 math.atan(y, x)를 쓴다
local Atan2 = math.atan2 or math.atan

-- 바닥 범위 (월드 좌표). 시작 위치를 정할 때만 쓴다. 이동 제한은 벽 Box가 한다
local Floor = dofile("Assets/Scripts/Lib/Floor.lua")

-- 시작 대기·결과 화면에서는 움직이지 않는다
local GameState = require("Assets.Scripts.Lib.GameState")

-- 각도 차이를 -180 ~ 180으로 접는다. 350도 → 10도로 갈 때 340도를 거꾸로 돌지 않고 20도만 돈다
local function WrapAngle(Degrees)
	return (Degrees + 180) % 360 - 180
end

return function(Config)
	local Speed = Config.Speed or 3.0               -- 초당 이동 거리
	local TurnSpeed = Config.TurnSpeed or 12.0      -- 회전 lerp 속도. 클수록 빨리 돈다
	local MeshYawOffset = Config.MeshYawOffset or 0 -- 메시 정면이 +X가 아닐 때 보정 (도)
	local SpawnOffsetY = Config.SpawnOffsetY        -- 있으면 시작할 때 바닥 중앙에서 Y로 이만큼 떨어진 곳에 선다

	-- 밀쳐내기: 앞쪽 부채꼴 안에 상대가 있으면 상대를 뒤로 밀어낸다
	local PlayerNumber = Config.PlayerNumber        -- 1 = P1, 2 = P2
	local PushCooldown = Config.PushCooldown or 2.0 -- 다시 밀 수 있을 때까지 (초). 헛쳐도 돈다
	local PushRange = Config.PushRange or 1.5       -- 이 거리 안의 상대만 민다
	local PushHalfAngle = Config.PushHalfAngle or 60 -- 바라보는 방향에서 좌우 이 각도(도) 안의 상대만 민다
	local KnockbackDistance = Config.KnockbackDistance or 2.0 -- 밀려나는 총 거리
	local KnockbackTime = Config.KnockbackTime or 0.3         -- 밀려나는 데 걸리는 시간. 이동안은 조작 불가

	local Actor = nil
	local Root = nil
	local TargetYaw = 0
	-- PlayerController가 매 프레임 넣어 주는 Axis 값 (-1 ~ 1)
	local AxisForward = 0
	local AxisRight = 0

	local PushCooldownLeft = 0
	local Knockback = nil   -- { X, Y, Remaining } 밀려나는 중일 때만 있다

	local Controller = {}

	-- 앞에 상대가 있으면 밀친다. Push 바인딩이 부른다 (액터 Tick보다 먼저)
	local function TryPush()
		if not GameState.IsPlaying() or PushCooldownLeft > 0 or Knockback then
			return
		end
		PushCooldownLeft = PushCooldown

		local Other = GameState.Players[3 - PlayerNumber]
		if not IsValid(Other) then
			return
		end

		local MyLocation = Actor:GetActorLocation()
		local OtherLocation = Other:GetActorLocation()
		local DX = OtherLocation.X - MyLocation.X
		local DY = OtherLocation.Y - MyLocation.Y
		local Distance = math.sqrt(DX * DX + DY * DY)
		if Distance > PushRange or Distance < 0.001 then
			return
		end

		-- 바라보는 방향은 메시 보정을 뺀 TargetYaw. 회전 lerp가 끝나기 전에도 누른 방향을 기준으로 한다
		local FacingRad = math.rad(TargetYaw - MeshYawOffset)
		local Dot = (DX * math.cos(FacingRad) + DY * math.sin(FacingRad)) / Distance
		if Dot < math.cos(math.rad(PushHalfAngle)) then
			return
		end

		-- 나에게서 상대 쪽으로 민다
		GameState.PushPlayer(3 - PlayerNumber, DX / Distance, DY / Distance)
	end

	-- 밀려나는 중이면 한 프레임만큼 밀고 true. 처음엔 빠르고 점점 느려진다
	local function UpdateKnockback(dt)
		local Incoming = GameState.TakeKnockback(PlayerNumber)
		if Incoming then
			Knockback = { X = Incoming.X, Y = Incoming.Y, Remaining = KnockbackTime }
		end
		if not Knockback then
			return false
		end

		-- 속도를 시간에 따라 1 → 0으로 줄인다. 그 넓이가 KnockbackDistance가 되도록 처음 속도는 2D/T
		local Step = math.min(dt, Knockback.Remaining)
		local Speed = 2 * KnockbackDistance / KnockbackTime * (Knockback.Remaining / KnockbackTime)
		local MoveX = Knockback.X * Speed * Step
		local MoveY = Knockback.Y * Speed * Step
		-- 벽에 닿으면 멈추도록 Sweep한다. 이동처럼 축을 나눠 벽을 따라 미끄러지게 한다
		if MoveX ~= 0 then Actor:AddActorWorldOffset(FVector(MoveX, 0, 0), true) end
		if MoveY ~= 0 then Actor:AddActorWorldOffset(FVector(0, MoveY, 0), true) end

		Knockback.Remaining = Knockback.Remaining - dt
		if Knockback.Remaining <= 0 then
			Knockback = nil
		end
		return true
	end

	-- PlayerController: GetPlayerController(i), Input: EnableInput(i)
	function Controller.BeginPlay(Owner, PlayerController, Input)
		Actor = Owner
		Root = Owner:GetRootComponent()
		TargetYaw = Root:GetRelativeRotation().Yaw

		-- 어떤 키가 어떤 Axis인지는 PlayerController가 들고 있다. 플레이어마다 컨트롤러가 달라 같은 이름에 다른 키를 쓴다
		PlayerController:AddAxisMapping("MoveForward", Config.Forward, 1.0)
		PlayerController:AddAxisMapping("MoveForward", Config.Back, -1.0)
		PlayerController:AddAxisMapping("MoveRight", Config.Right, 1.0)
		PlayerController:AddAxisMapping("MoveRight", Config.Left, -1.0)

		-- 액터 Tick보다 먼저 불리므로 Tick에서는 이번 프레임 값을 쓴다
		Input:BindAxis("MoveForward", function(Value) AxisForward = Value end)
		Input:BindAxis("MoveRight", function(Value) AxisRight = Value end)

		PlayerController:AddActionMapping("Push", Config.PushKey)
		Input:BindAction("Push", "Pressed", TryPush)

		PushCooldownLeft = 0
		Knockback = nil

		-- 바닥 중앙을 기준으로 좌우에 세운다. 높이(Z)는 씬에 배치한 값을 그대로 쓴다
		if SpawnOffsetY then
			local Location = Root:GetRelativeLocation()
			Location.X = (Floor.MinX + Floor.MaxX) * 0.5
			Location.Y = (Floor.MinY + Floor.MaxY) * 0.5 + SpawnOffsetY
			Root:SetRelativeLocation(Location)

			-- 중앙 쪽(상대 쪽)을 바라보게 한다. 왼쪽(-Y)에 서면 +Y(Yaw 90), 오른쪽이면 -Y(Yaw -90)
			TargetYaw = WrapAngle((SpawnOffsetY < 0 and 90 or -90) + MeshYawOffset)
			local Rotation = Root:GetRelativeRotation()
			Rotation.Yaw = TargetYaw
			Root:SetRelativeRotation(Rotation)
		end
	end

	function Controller.Tick(dt)
		-- 제한 시간 안에서만 움직인다 (시작 대기·결과 화면에서는 멈춤)
		if not GameState.IsPlaying() then
			return
		end

		PushCooldownLeft = math.max(0, PushCooldownLeft - dt)

		-- 밀려나는 동안은 입력으로 움직이지 않는다
		if UpdateKnockback(dt) then
			return
		end

		local DX, DY = AxisForward, AxisRight

		if DX ~= 0 or DY ~= 0 then
			-- 대각선으로 눌러도 같은 속도가 되게 정규화한다
			local Len = math.sqrt(DX * DX + DY * DY)
			local MoveX = DX / Len * Speed * dt
			local MoveY = DY / Len * Speed * dt

			-- 축을 나눠 Sweep한다. 한 번에 옮기면 벽에 비스듬히 닿을 때 통째로 멈추지만,
			-- 나눠 옮기면 막힌 축만 멈추고 나머지 축으로는 벽을 따라 미끄러진다
			if MoveX ~= 0 then Actor:AddActorWorldOffset(FVector(MoveX, 0, 0), true) end
			if MoveY ~= 0 then Actor:AddActorWorldOffset(FVector(0, MoveY, 0), true) end

			-- 엔진은 Yaw 0이 +X, Yaw 90이 +Y (단위는 도)
			TargetYaw = math.deg(Atan2(DY, DX)) + MeshYawOffset
		end

		-- 이동 방향을 바라보게 매 프레임 조금씩 돈다. Alpha를 1 - e^(-k·dt)로 두면 프레임 속도와 무관하다
		local Rotation = Root:GetRelativeRotation()
		local Alpha = 1 - math.exp(-TurnSpeed * dt)
		Rotation.Yaw = WrapAngle(Rotation.Yaw + WrapAngle(TargetYaw - Rotation.Yaw) * Alpha)
		Root:SetRelativeRotation(Rotation)
	end

	function Controller.EndPlay()
		Actor = nil
		Root = nil   -- 참조를 놓아서 삭제된 뒤 접근하지 않게 함
	end

	return Controller
end
