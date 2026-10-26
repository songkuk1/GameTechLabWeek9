-- 플레이어 공통 이동: 키 입력 이동 + 이동 방향 회전(lerp)
-- 충돌은 엔진 Box 컴포넌트가 맡는다. 플레이어의 CollisionBox와 씬의 벽(Wall*)이 모두 bBlockComponent라서
-- AddActorWorldOffset(Delta, true)로 옮기면 엔진이 Sweep해 벽·다른 플레이어 앞에서 멈춘다.
-- 플레이어 스크립트에서 dofile로 불러 Config를 넘겨 만든다.
--   local Controller = dofile("Assets/Scripts/Lib/PlayerController.lua")({ Forward = "W", Back = "S", Left = "A", Right = "D" })

-- Lua 5.1·LuaJIT는 math.atan2, 5.3 이상은 math.atan(y, x)를 쓴다
local Atan2 = math.atan2 or math.atan

-- 바닥 범위 (월드 좌표). 시작 위치를 정할 때만 쓴다. 이동 제한은 벽 Box가 한다
local Floor = dofile("Assets/Scripts/Lib/Floor.lua")

-- 각도 차이를 -180 ~ 180으로 접는다. 350도 → 10도로 갈 때 340도를 거꾸로 돌지 않고 20도만 돈다
local function WrapAngle(Degrees)
	return (Degrees + 180) % 360 - 180
end

return function(Config)
	local Speed = Config.Speed or 3.0               -- 초당 이동 거리
	local TurnSpeed = Config.TurnSpeed or 12.0      -- 회전 lerp 속도. 클수록 빨리 돈다
	local MeshYawOffset = Config.MeshYawOffset or 0 -- 메시 정면이 +X가 아닐 때 보정 (도)
	local SpawnOffsetY = Config.SpawnOffsetY        -- 있으면 시작할 때 바닥 중앙에서 Y로 이만큼 떨어진 곳에 선다
	local StartLockTime = Config.StartLockTime or 3.0 -- 씬 시작 후 이 시간(초) 동안은 움직이지 못한다

	local Actor = nil
	local Root = nil
	local TargetYaw = 0
	local LockTimer = 0

	local Controller = {}

	function Controller.BeginPlay(Owner)
		Actor = Owner
		Root = Owner:GetRootComponent()
		TargetYaw = Root:GetRelativeRotation().Yaw
		LockTimer = StartLockTime

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
		-- 시작 대기 중에는 입력을 받지 않는다
		if LockTimer > 0 then
			LockTimer = LockTimer - dt
			return
		end

		local DX, DY = 0, 0
		if Input.IsKeyDown(Config.Forward) then DX = DX + 1 end
		if Input.IsKeyDown(Config.Back)    then DX = DX - 1 end
		if Input.IsKeyDown(Config.Left)    then DY = DY - 1 end
		if Input.IsKeyDown(Config.Right)   then DY = DY + 1 end

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
