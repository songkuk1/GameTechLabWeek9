-- 게임 시작 연출: 정해 둔 앵글(StartLocation)에서 이 스크립트가 붙은 액터(브로콜리 화산 + 옥수수 화산)를 먼저 비추고, 화산이 터진 뒤 원래 게임 카메라 시점으로 부드럽게 넘어간다.
-- 카메라가 게임 시점에 도착하면 GameState.FinishIntro로 알려서 GameHUD의 3, 2, 1 카운트다운이 시작된다.
-- 화산을 비추는 카메라는 시작할 때 만들고, 전환이 끝나면 지운다.
--
-- 화산은 이 액터의 메시 컴포넌트들이다 (Volcanoes 표). 두 화산이 같이 터진다.
--   우르릉 : 위아래로 늘었다 줄었다 하며 떨림이 점점 커진다 (늘어날 때 옆은 살짝 홀쭉해진다)
--   펑     : 위로 쭉 길어졌다가 원래 크기로 돌아온다 (길어질 때 옆은 살짝 홀쭉해진다).
--            꼭대기에서 브로콜리 화산은 브로콜리, 옥수수 화산은 팝콘이 터져 나온다
--   폭발 이펙트 : 우르릉 동안 꼭대기에서 작은 불꽃이 튀고, 펑 할 때 큰 불덩이가 솟는다 (explosionAtlas)
--   소리 : 우르릉 동안 EarthQuake, 펑 할 때 Explosion

local GameState = require("Assets.Scripts.Lib.GameState")

local HoldBefore = 1.0        -- 시작 앵글을 보여 주고 터지기 전까지 (초)
local EruptTime = 2.0         -- 우르릉 + 펑 전체 시간 (초)
local HoldAfter = 0.4         -- 터진 뒤 게임 카메라로 넘어가기 전까지 (초)
local BlendTime = 3.0         -- 게임 카메라로 넘어가는 시간 (초)
local BlendFunc = "EaseInOut" -- Linear, Cubic, EaseIn, EaseOut, EaseInOut
local BlendExp = 2.0          -- Ease 계열 곡선의 강도
-- 인트로 카메라 앵글. 에디터 뷰포트에서 원하는 앵글을 잡고 Viewport 패널의 Location, Rotation(Pitch, Yaw) 값을 옮겨 적는다
local StartLocation = FVector(37.801, 0.399, 49.278)
local StartPitch = -20.502
local StartYaw = 0.608
local LookUp = 15             -- 펑 할 때 시선을 더 올리는 각도 (도). 올린 시선은 넘어갈 때까지 유지

local RumbleRatio = 0.7       -- EruptTime 중 우르릉이 차지하는 비율. 나머지가 펑
local RumbleFreq = 12         -- 우르릉 떨림 횟수 (초당)
local RumbleScale = 0.06      -- 우르릉 떨림의 최대 크기 (기본 크기 대비)
local PopScale = 0.6          -- 펑 할 때 위로 길어지는 크기 (기본 높이 대비)
local PopThin = 0.15          -- 펑 할 때 옆이 홀쭉해지는 크기 (기본 폭 대비). 0이면 폭은 그대로

-- 카메라 흔들림. 세기는 카메라가 밀리는 거리라서 화산 크기에 맞춰 화산들 전체 반지름의 배수로 준다
local RumbleShake = 0.015     -- 우르릉 끝 무렵의 흔들림 세기. 짧은 흔들림을 점점 세게 겹쳐서 커지는 느낌을 낸다
local RumbleShakeSteps = 4    -- 우르릉 동안 흔들림을 몇 번 나눠 걸지
local RumbleShakeFreq = 25    -- 우르릉 흔들림 빠르기 (초당)
local PopShake = 0.07         -- 펑 할 때 흔들림 세기
local PopShakeTime = 1.2      -- 펑 흔들림이 잦아드는 시간 (초)
local PopShakeFreq = 15       -- 펑 흔들림 빠르기 (초당)

-- 터지는 화산. Component는 이 액터의 메시 컴포넌트 이름, Piece는 꼭대기에서 터져 나오는 메시
-- Count는 개수, Size는 하나의 크기(반지름). 크기·속도·중력은 그 화산 반지름의 배수
local Volcanoes = {
    { Component = "UStaticMeshComponent", Piece = "Models/Broccoli/broccoli.obj", Count = 50, Size = 0.07 },
    { Component = "CornVolcanoMesh",      Piece = "Models/Popcorn/T_1.obj",       Count = 50, Size = 0.05 },
}
local PieceTop = 0.6          -- 나오는 높이 (화산 중심에서 위로)
local PieceUpSpeed = 5.0      -- 위로 솟는 속도 (최소~최대의 최대, 최소는 절반)
local PieceSideSpeed = 1.0    -- 옆으로 퍼지는 속도 (최대)
local PieceGravity = 3.0      -- 떨어지는 가속도
local PieceSpin = 360         -- 굴러가는 속도 (도/초, 최대)
local PieceLife = 4.0         -- 사라질 때까지 (초). 마지막 PieceFade초 동안 작아지며 사라진다
local PieceFade = 0.6

-- 폭발 이펙트 (explosionAtlas: 5x5칸 중 23프레임). 크기·속도는 화산 반지름의 배수라서 BeginPlay에서 재고 등록한다
-- 프레임이 끝나면 처음으로 돌아가므로 수명은 23 / ExplosionFrameRate초를 넘지 않게 한다
local ExplosionTexture = "Assets/SubUV/explosionAtlas.png"
local ExplosionFrameRate = 26
local BlastCount = 9          -- 펑: 꼭대기에서 터지는 불덩이 개수
local BlastSize = 0.45        -- 펑 불덩이 크기 (시작, 끝은 BlastGrow배로 부푼다)
local BlastGrow = 1.8
local BlastSpread = 0.6       -- 펑 불덩이가 옆으로 퍼지는 속도
local BlastRise = 1.2         -- 펑 불덩이가 솟는 속도 (최대)
local PuffCount = 2           -- 우르릉: 흔들림이 세질 때마다 꼭대기 근처에서 터지는 작은 불꽃 개수
local PuffSize = 0.12

-- 소리. 구르릉은 우르릉이 시작될 때 틀고 펑 할 때 끊는다
local RumbleSound = "Assets/Audio/EarthQuake.mp3"
local RumbleVolume = 1.0
local PopSound = "Assets/Audio/Explosion.mp3"
local PopVolume = 1.0

local IntroCamera = nil
local EruptElapsed = -1       -- 0 이상이면 터지는 중
local VolcanoCenter = nil     -- 흔들림 세기에 쓰는 화산들 전체 중심·반지름
local VolcanoRadius = 0
local bBurst = false          -- 이번 분출에서 조각을 이미 내보냈는지
local RumbleSoundId = nil     -- 재생 중인 구르릉 소리
local Pieces = {}             -- { Actor, Mesh, Location, Velocity, Rotation, Spin, Scale, Gravity, Age }

-- 0~1 진행률에서 (옆 배율, 위 배율)
local function EruptScale(T)
    if T < RumbleRatio then
        local Strength = RumbleScale * (T / RumbleRatio)
        local Wave = math.sin(T * EruptTime * RumbleFreq * 2 * math.pi) * Strength
        return 1 - Wave * 0.5, 1 + Wave
    end
    local Pop = math.sin(math.pi * (T - RumbleRatio) / (1 - RumbleRatio))
    return 1 - Pop * PopThin, 1 + Pop * PopScale
end

-- 화산마다 자기 자리에서 늘어난다 (메시 바닥이 기준점이라 아래로는 안 늘어난다)
local function SetEruptScale(Side, Up)
    for _, Volcano in ipairs(Volcanoes) do
        if IsValid(Volcano.Mesh) then
            Volcano.Mesh:SetRelativeScale3D(Volcano.BaseScale * FVector(Side, Side, Up))
        end
    end
end

-- Lift(0~1)만큼 시작 앵글에서 시선을 위로 올린다. 이 엔진은 Pitch가 작아질수록 위를 본다
local function AimIntroCamera(Lift)
    if not IsValid(IntroCamera) then
        return
    end
    IntroCamera:GetCameraComponent():SetWorldRotation(FRotator(StartPitch - LookUp * Lift, StartYaw, 0), false)
end

local function StopRumbleSound()
    if RumbleSoundId then
        Audio.Stop(RumbleSoundId)
        RumbleSoundId = nil
    end
end

local function RandomRange(Min, Max)
    return Min + math.random() * (Max - Min)
end

-- 화산 크기에 맞춘 폭발 이펙트를 등록한다. VolcanoBlast는 펑, VolcanoPuff는 우르릉 중 작은 불꽃
local function RegisterExplosionEffects()
    local Radius = 0
    for _, Volcano in ipairs(Volcanoes) do
        Radius = Radius + Volcano.Radius / #Volcanoes
    end
    if Radius <= 0 then
        return
    end

    local Life = 23 / ExplosionFrameRate
    RegisterEffect("VolcanoBlast", {
        TexturePath = ExplosionTexture, Cols = 5, Rows = 5, FrameCount = 23,
        FrameRate = ExplosionFrameRate, bRandomStartFrame = false, bBurst = true,
        ParticleCount = BlastCount,
        MinLifeTime = Life * 0.85, MaxLifeTime = Life,
        MinVelocity = FVector(-BlastSpread, -BlastSpread, BlastRise * 0.3) * Radius,
        MaxVelocity = FVector(BlastSpread, BlastSpread, BlastRise) * Radius,
        Acceleration = FVector(0, 0, -BlastRise * 0.5) * Radius,
        StartScale = BlastSize * Radius, EndScale = BlastSize * BlastGrow * Radius,
        FadeStart = 0.7,
    })
    RegisterEffect("VolcanoPuff", {
        TexturePath = ExplosionTexture, Cols = 5, Rows = 5, FrameCount = 23,
        FrameRate = ExplosionFrameRate * 1.5, bRandomStartFrame = false, bBurst = true,
        ParticleCount = PuffCount,
        MinLifeTime = Life / 1.5 * 0.8, MaxLifeTime = Life / 1.5,
        MinVelocity = FVector(-0.2, -0.2, 0.1) * Radius,
        MaxVelocity = FVector(0.2, 0.2, 0.4) * Radius,
        StartScale = PuffSize * Radius, EndScale = PuffSize * 1.5 * Radius,
        FadeStart = 0.6,
    })
end

local function VolcanoTop(Volcano)
    return Volcano.Center + FVector(0, 0, Volcano.Radius * PieceTop)
end

-- 화산 꼭대기에서 조각을 사방으로 터뜨린다
local function BurstPieces(Volcano)
    local Radius = Volcano.Radius
    local Origin = VolcanoTop(Volcano)
    local Scale = nil -- 메시 원래 크기를 첫 조각에서 재서 Size에 맞춘다

    for i = 1, Volcano.Count do
        local Actor = SpawnMeshActor(Volcano.Piece, Origin)
        if not Actor then
            print("[CameraIntro] 메시를 찾을 수 없습니다: " .. Volcano.Piece)
            return
        end

        local Mesh = Actor:GetStaticMeshComponent()
        if not Scale then
            local _, MeshRadius = GetActorBounds(Actor)
            Scale = MeshRadius > 0 and Radius * Volcano.Size / MeshRadius or 1
        end

        local Angle = (i / Volcano.Count + RandomRange(-0.3, 0.3) / Volcano.Count) * 2 * math.pi
        local Side = RandomRange(0.3, 1) * PieceSideSpeed * Radius
        table.insert(Pieces, {
            Actor = Actor,
            Mesh = Mesh,
            Location = Origin,
            Velocity = FVector(math.cos(Angle) * Side, math.sin(Angle) * Side,
                RandomRange(0.5, 1) * PieceUpSpeed * Radius),
            Rotation = { RandomRange(0, 360), RandomRange(0, 360), RandomRange(0, 360) },
            Spin = { RandomRange(-1, 1) * PieceSpin, RandomRange(-1, 1) * PieceSpin, RandomRange(-1, 1) * PieceSpin },
            Scale = Scale * RandomRange(0.8, 1.2),
            Gravity = PieceGravity * Radius,
            Age = 0,
        })
    end
end

local function DestroyPieces()
    for _, Piece in ipairs(Pieces) do
        if IsValid(Piece.Actor) then
            Piece.Actor:Destroy()
        end
    end
    Pieces = {}
end

-- 날아가는 조각을 포물선으로 움직이고, 수명이 다하면 작아지며 사라진다
local function UpdatePieces(dt)
    for i = #Pieces, 1, -1 do
        local Piece = Pieces[i]
        Piece.Age = Piece.Age + dt
        if Piece.Age >= PieceLife or not IsValid(Piece.Actor) then
            if IsValid(Piece.Actor) then
                Piece.Actor:Destroy()
            end
            table.remove(Pieces, i)
        else
            Piece.Velocity = Piece.Velocity - FVector(0, 0, Piece.Gravity * dt)
            Piece.Location = Piece.Location + Piece.Velocity * dt
            for Axis = 1, 3 do
                Piece.Rotation[Axis] = Piece.Rotation[Axis] + Piece.Spin[Axis] * dt
            end

            local Fade = math.min(1, (PieceLife - Piece.Age) / PieceFade)
            local Size = Piece.Scale * Fade
            Piece.Mesh:SetRelativeScale3D(FVector(Size, Size, Size))
            Piece.Mesh:SetWorldLocationAndRotation(Piece.Location,
                FRotator(Piece.Rotation[1], Piece.Rotation[2], Piece.Rotation[3]), false)
        end
    end
end

function BeginPlay()
    local MainCamera = GetMainCamera()
    if not MainCamera then
        GameState.FinishIntro() -- 연출을 못 하면 바로 카운트다운으로 넘어간다
        return
    end

    -- 씬에 없는 화산은 빼고 진행한다
    for i = #Volcanoes, 1, -1 do
        local Volcano = Volcanoes[i]
        Volcano.Mesh = obj:FindComponentByName(Volcano.Component)
        if Volcano.Mesh then
            Volcano.BaseScale = Volcano.Mesh:GetRelativeScale3D()
            Volcano.Center, Volcano.Radius = GetComponentBounds(Volcano.Mesh)
        else
            print("[CameraIntro] 화산 컴포넌트가 없습니다: " .. Volcano.Component)
            table.remove(Volcanoes, i)
        end
    end
    RegisterExplosionEffects()

    VolcanoCenter, VolcanoRadius = GetActorBounds(obj)

    IntroCamera = SpawnCamera(StartLocation, FRotator(StartPitch, StartYaw, 0))
    if not IntroCamera then
        GameState.FinishIntro()
        return
    end
    SetViewTarget(IntroCamera)

    start_coroutine(function()
        wait(HoldBefore)

        EruptElapsed = 0
        RumbleSoundId = Audio.Play(RumbleSound, RumbleVolume)

        -- 우르릉: 흔들림이 점점 세진다
        local RumbleTime = EruptTime * RumbleRatio
        local StepTime = RumbleTime / RumbleShakeSteps
        for Step = 1, RumbleShakeSteps do
            CameraShake(VolcanoRadius * RumbleShake * Step / RumbleShakeSteps, StepTime * 1.5, RumbleShakeFreq)
            -- 꼭대기 근처에서 작은 불꽃이 튄다
            for _, Volcano in ipairs(Volcanoes) do
                local R = Volcano.Radius
                SpawnEffect("VolcanoPuff", VolcanoTop(Volcano) + FVector(RandomRange(-0.25, 0.25) * R, RandomRange(-0.25, 0.25) * R, RandomRange(-0.2, 0) * R))
            end
            wait(StepTime)
        end

        -- 펑: 크게 한 번 흔들리고 잦아든다
        CameraShake(VolcanoRadius * PopShake, PopShakeTime, PopShakeFreq)
        wait_until(function() return EruptElapsed < 0 end)

        wait(HoldAfter)
        SetViewTarget(MainCamera, BlendTime, BlendFunc, BlendExp)
        wait_until(function() return not IsViewTargetBlending() end)

        if IsValid(IntroCamera) then
            IntroCamera:Destroy()
        end
        IntroCamera = nil
        GameState.FinishIntro()
    end)
end

-- 코루틴은 프레임 시간을 받지 못해서 터지는 연출은 Tick에서 진행한다
function Tick(dt)
    UpdatePieces(dt)

    if EruptElapsed < 0 then
        return
    end

    EruptElapsed = EruptElapsed + dt
    if EruptElapsed >= EruptTime then
        SetEruptScale(1, 1)
        AimIntroCamera(1)
        EruptElapsed = -1
        return
    end

    local T = EruptElapsed / EruptTime
    SetEruptScale(EruptScale(T))

    -- 펑 구간 동안 시선을 위로 부드럽게 올린다 (3t² - 2t³)
    if T >= RumbleRatio then
        if not bBurst then
            bBurst = true
            StopRumbleSound()
            Audio.Play(PopSound, PopVolume)
            for _, Volcano in ipairs(Volcanoes) do
                SpawnEffect("VolcanoBlast", VolcanoTop(Volcano))
                BurstPieces(Volcano)
            end
        end
        local U = (T - RumbleRatio) / (1 - RumbleRatio)
        AimIntroCamera(U * U * (3 - 2 * U))
    end
end

function EndPlay()
    StopRumbleSound()
    SetEruptScale(1, 1)
    DestroyPieces()
    if IsValid(IntroCamera) then
        IntroCamera:Destroy()
    end
end
