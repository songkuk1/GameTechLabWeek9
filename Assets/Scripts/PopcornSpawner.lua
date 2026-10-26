-- 팝콘 낙하: 바닥 범위 위에서 랜덤 위치로 팝콘 또는 마늘이 떨어진다.
-- 그림자는 씬의 수직 Directional Light(PCSS)가 그린다. 높을수록 흐리고, 내려올수록 진하고 선명해진다.
-- Lua에서 액터를 새로 만들 수 없어서, 이 액터에 미리 넣어 둔 Drop0~N 컴포넌트를 돌려쓴다(풀링).
-- DropN은 충돌용 Box(움직이는 대상), 자식 DropNMesh는 보이는 모양이다. 플레이어 CollisionBox와 겹치면 엔진이
-- OnComponentOverlap을 불러 주고, 그때 그 낙하물을 줍는다.
-- 떨어뜨릴 때마다 종류를 뽑아 DropNMesh의 메시·크기를 바꾼다. Box는 회전·크기를 바꾸지 않아 판정 크기가 일정하다.
-- 액터 루트는 원점에 두므로 컴포넌트의 Relative 위치 = 월드 위치다.

local Floor = dofile("Assets/Scripts/Lib/Floor.lua")

local PoolSize = 8               -- 씬에 넣어 둔 Drop 컴포넌트 개수와 같아야 한다
local SpawnIntervalMin = 0.5     -- 다음 낙하까지 대기 시간 (초, 이 범위에서 랜덤)
local SpawnIntervalMax = 1.2
local StartHeight = 7.0          -- 낙하 시작 높이
local SpinSpeedMin = 60         -- 떨어지면서 도는 속도 (도/초, 축마다 이 범위에서 랜덤)
local SpinSpeedMax = 180
local BounceHeight = 0.8       -- 착지 후 한 번 튀어 오르는 높이
local BounceTime = 0.6          -- 튀어 올랐다가 다시 바닥에 닿기까지 걸리는 시간 (초)
local FallTime = 4.0           -- 바닥까지 걸리는 시간 (초)
local EdgeMargin = 0.6          -- 벽에 붙어 떨어지지 않게 경계에서 뺄 여유

-- 떨어지는 종류. Weight는 상대 비율이라 합이 1일 필요는 없다 (예: 7과 3이면 70% / 30%).
-- Scale: 모델 원본 크기가 달라서 둘 다 지름 약 0.9가 되도록 맞춘 값
-- Bottom: 모델 원점에서 바닥까지 거리(Scale 적용 후). 착지 높이를 맞출 때 쓴다
local DropTypes = {
	{ Name = "Popcorn", Weight = 1, Mesh = "Models/Popcorn/T_1.obj",    Scale = 0.24, Bottom = 0.53 },
	{ Name = "Garlic",  Weight = 1, Mesh = "Models/Garlic/garlic.obj",  Scale = 0.30, Bottom = 0.39 },
}

local HiddenZ = -10.0           -- 쉬는 슬롯을 숨겨 둘 바닥 아래 높이. 플레이어와 겹치지 않게 Box째 내려 둔다

-- 이펙트 (프리셋은 EffectManager.cpp의 RegisterDefaultEffects)
local PenaltyText = "-3"                              -- 팝콘이 아닌 것에 맞았을 때 머리 위에 뜨는 글자 (표시만, 점수는 따로)
local PenaltyColor = FLinearColor(1.0, 0.45, 0.75, 1.0)
local PenaltyTextHeight = 1.4                         -- 플레이어 위치에서 글자를 띄울 높이

local Slots = {}                 -- { Drop(Box), Mesh, Active, Type, X, Y, Time }
local SpawnTimer = 0
local TotalWeight = 0

-- 축 하나의 회전 속도(도/초). 방향도 랜덤이라 시계·반시계가 섞인다
local function RandomSpin()
	local Speed = SpinSpeedMin + math.random() * (SpinSpeedMax - SpinSpeedMin)
	return math.random() < 0.5 and -Speed or Speed
end

-- Weight 비율대로 종류 하나를 뽑는다
local function PickType()
	local Roll = math.random() * TotalWeight
	for _, Type in ipairs(DropTypes) do
		Roll = Roll - Type.Weight
		if Roll <= 0 then
			return Type
		end
	end
	return DropTypes[#DropTypes]
end

local function Hide(Slot)
	Slot.Active = false
	Slot.Mesh:SetVisible(false)
	Slot.Drop:SetRelativeLocation(FVector(0, 0, HiddenZ))
end

-- 플레이어가 낙하물을 주웠을 때. 점수·효과는 여기에 붙이면 된다 (Slot.Type.Name으로 팝콘/마늘 구분)
local function Collect(Slot, Player)
	print("[PopcornSpawner] " .. tostring(Player) .. " got " .. Slot.Type.Name)

	local Location = Slot.Drop:GetWorldLocation()
	if Slot.Type.Name == "Popcorn" then
		SpawnEffect("PopcornCatch", Location)
	else
		SpawnEffect("HitSplash", Location)
		SpawnTextEffect(PenaltyText, Player:GetActorLocation() + FVector(0, 0, PenaltyTextHeight), PenaltyColor)
	end

	Hide(Slot)
end

local function NextInterval()
	return SpawnIntervalMin + math.random() * (SpawnIntervalMax - SpawnIntervalMin)
end

-- 쉬고 있는 슬롯 하나를 골라 랜덤 위치 위에서 떨어뜨리기 시작한다
local function Spawn()
	for _, Slot in ipairs(Slots) do
		if not Slot.Active then
			Slot.Active = true
			Slot.Time = 0
			Slot.Landed = false
			Slot.Type = PickType()
			Slot.Mesh:SetStaticMesh(Slot.Type.MeshAsset)
			Slot.Mesh:SetRelativeScale3D(FVector(Slot.Type.Scale, Slot.Type.Scale, Slot.Type.Scale))
			-- 시작 자세와 회전 속도를 축마다 랜덤으로 정해 물체마다 다르게 돈다
			Slot.Pitch, Slot.Yaw, Slot.Roll = math.random() * 360, math.random() * 360, math.random() * 360
			Slot.SpinPitch = RandomSpin()
			Slot.SpinYaw = RandomSpin()
			Slot.SpinRoll = RandomSpin()
			Slot.Mesh:SetRelativeRotation(FRotator(Slot.Pitch, Slot.Yaw, Slot.Roll))
			Slot.X = Floor.MinX + EdgeMargin + math.random() * (Floor.MaxX - Floor.MinX - EdgeMargin * 2)
			Slot.Y = Floor.MinY + EdgeMargin + math.random() * (Floor.MaxY - Floor.MinY - EdgeMargin * 2)
			Slot.Drop:SetRelativeLocation(FVector(Slot.X, Slot.Y, Floor.Z + StartHeight))
			Slot.Mesh:SetVisible(true)
			return
		end
	end
	-- 모든 슬롯이 떨어지는 중이면 이번 차례는 건너뛴다
end

function BeginPlay()
	-- 메시는 매번 찾지 않고 시작할 때 한 번 찾아 둔다. 못 찾은 종류는 뽑지 않는다
	local Valid = {}
	for _, Type in ipairs(DropTypes) do
		Type.MeshAsset = FindStaticMesh(Type.Mesh)
		if Type.MeshAsset then
			table.insert(Valid, Type)
			TotalWeight = TotalWeight + Type.Weight
		else
			print("[PopcornSpawner] 메시를 찾을 수 없습니다: " .. Type.Mesh)
		end
	end
	DropTypes = Valid

	for i = 0, PoolSize - 1 do
		local Drop = obj:FindComponentByName("Drop" .. i)
		local Mesh = obj:FindComponentByName("Drop" .. i .. "Mesh")
		if Drop and Mesh then
			local Slot = { Drop = Drop, Mesh = Mesh }
			Hide(Slot)
			table.insert(Slots, Slot)
		else
			print("[PopcornSpawner] Drop" .. i .. " 또는 Drop" .. i .. "Mesh 컴포넌트가 없습니다")
		end
	end
	SpawnTimer = NextInterval()
end

function Tick(dt)
	if #DropTypes == 0 then
		return
	end

	SpawnTimer = SpawnTimer - dt
	if SpawnTimer <= 0 then
		Spawn()
		SpawnTimer = NextInterval()
	end

	for _, Slot in ipairs(Slots) do
		if Slot.Active then
			Slot.Time = Slot.Time + dt
			local Bottom = Slot.Type.Bottom
			local GroundZ = Floor.Z + Bottom
			local Z

			if Slot.Time < FallTime then
				-- 낙하: 중력처럼 점점 빨라지게 T²로 내려온다
				local T = Slot.Time / FallTime   -- 0(시작) ~ 1(착지)
				Z = GroundZ + (StartHeight - Bottom) * (1 - T * T)
			else
				-- 바운스: 착지 후 한 번 튀어 올랐다가 다시 바닥에 닿는 포물선
				local U = math.min((Slot.Time - FallTime) / BounceTime, 1.0)   -- 0(착지) ~ 1(재착지)
				Z = GroundZ + BounceHeight * 4 * U * (1 - U)

				-- 팝콘이 아닌 것이 바닥에 처음 닿는 순간 튄다
				if not Slot.Landed then
					Slot.Landed = true
					if Slot.Type.Name ~= "Popcorn" then
						SpawnEffect("HitSplash", FVector(Slot.X, Slot.Y, Floor.Z))
					end
				end
			end

			Slot.Drop:SetRelativeLocation(FVector(Slot.X, Slot.Y, Z))

			-- 다시 바닥에 닿으면 사라진다. 아무도 못 주운 것
			if Slot.Time >= FallTime + BounceTime then
				Hide(Slot)
			end

			-- 낙하·바운스 내내 돌린다
			Slot.Pitch = (Slot.Pitch + Slot.SpinPitch * dt) % 360
			Slot.Yaw = (Slot.Yaw + Slot.SpinYaw * dt) % 360
			Slot.Roll = (Slot.Roll + Slot.SpinRoll * dt) % 360
			Slot.Mesh:SetRelativeRotation(FRotator(Slot.Pitch, Slot.Yaw, Slot.Roll))
		end
	end
end

function EndPlay()
	Slots = {}   -- 컴포넌트 참조를 놓아서 삭제된 뒤 접근하지 않게 함
end

-- 엔진이 부른다: 내 컴포넌트(MyComp)가 다른 액터의 컴포넌트와 겹치기 시작할 때.
-- 낙하물 Box와 겹치는 건 플레이어 CollisionBox뿐이다 (벽은 겹침 이벤트를 끄고 있다)
function OnComponentOverlap(MyComp, OtherActor, OtherComp)
	for _, Slot in ipairs(Slots) do
		if Slot.Active and Slot.Drop == MyComp then
			Collect(Slot, OtherActor)
			return
		end
	end
end
