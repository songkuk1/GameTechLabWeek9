-- 게임 시작 연출: 이 스크립트가 붙은 액터(브로콜리 화산)를 먼저 비추고, 화산이 터진 뒤 원래 게임 카메라 시점으로 부드럽게 넘어간다.
-- 카메라가 게임 시점에 도착하면 GameState.FinishIntro로 알려서 GameHUD의 3, 2, 1 카운트다운이 시작된다.
-- 화산을 비추는 카메라는 시작할 때 만들고, 전환이 끝나면 지운다.
--
-- 터지는 연출은 지금은 스케일로만 표현한다.
--   우르릉 : 위아래로 늘었다 줄었다 하며 떨림이 점점 커진다 (늘어날 때 옆은 살짝 홀쭉해진다)
--   펑     : 위로 쭉 길어졌다가 원래 크기로 돌아온다 (길어질 때 옆은 살짝 홀쭉해진다)

local GameState = require("Assets.Scripts.Lib.GameState")

local HoldBefore = 0.3        -- 화산을 비추고 터지기 전까지 (초)
local EruptTime = 2.0         -- 우르릉 + 펑 전체 시간 (초)
local HoldAfter = 0.4         -- 터진 뒤 게임 카메라로 넘어가기 전까지 (초)
local BlendTime = 3.0         -- 게임 카메라로 넘어가는 시간 (초)
local BlendFunc = "EaseInOut" -- Linear, Cubic, EaseIn, EaseOut, EaseInOut
local BlendExp = 2.0          -- Ease 계열 곡선의 강도
local Distance = 1.6          -- 화산 중심에서 카메라까지 거리 (화산 반지름의 배수)
local Height = 0.4            -- 카메라를 올리는 높이 (화산 반지름의 배수)

local RumbleRatio = 0.7       -- EruptTime 중 우르릉이 차지하는 비율. 나머지가 펑
local RumbleFreq = 12         -- 우르릉 떨림 횟수 (초당)
local RumbleScale = 0.06      -- 우르릉 떨림의 최대 크기 (기본 크기 대비)
local PopScale = 0.6          -- 펑 할 때 위로 길어지는 크기 (기본 높이 대비)
local PopThin = 0.15          -- 펑 할 때 옆이 홀쭉해지는 크기 (기본 폭 대비). 0이면 폭은 그대로
local LookUp = 1.2            -- 펑 할 때 시선을 올리는 높이 (화산 중심에서 화산 반지름의 배수만큼 위). 올린 시선은 넘어갈 때까지 유지

local IntroCamera = nil
local Root = nil
local BaseScale = nil
local EruptElapsed = -1       -- 0 이상이면 터지는 중
local CameraLocation = nil    -- 인트로 카메라 위치와, 시선 계산에 쓰는 화산 중심·반지름
local VolcanoCenter = nil
local VolcanoRadius = 0

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

local function SetEruptScale(Side, Up)
    if IsValid(Root) then
        Root:SetRelativeScale3D(BaseScale * FVector(Side, Side, Up))
    end
end

-- Lift(0~1)만큼 시선을 화산 중심에서 위쪽으로 올린다
local function AimIntroCamera(Lift)
    if not IsValid(IntroCamera) then
        return
    end
    local Target = VolcanoCenter + FVector(0, 0, VolcanoRadius * LookUp * Lift)
    IntroCamera:GetCameraComponent():SetWorldRotation(FindLookAtRotation(CameraLocation, Target), false)
end

function BeginPlay()
    local MainCamera = GetMainCamera()
    if not MainCamera then
        GameState.FinishIntro() -- 연출을 못 하면 바로 카운트다운으로 넘어간다
        return
    end

    Root = obj:GetRootComponent()
    BaseScale = Root:GetRelativeScale3D()

    -- 게임 카메라가 있는 쪽에서 화산을 바라보도록 둔다. 그래야 넘어갈 때 화산에서 플레이어 쪽으로 자연스럽게 빠진다
    VolcanoCenter, VolcanoRadius = GetActorBounds(obj)
    local ToGameCamera = (GetCameraLocation() - VolcanoCenter):Normalized()
    CameraLocation = VolcanoCenter + ToGameCamera * (VolcanoRadius * Distance) + FVector(0, 0, VolcanoRadius * Height)

    IntroCamera = SpawnCamera(CameraLocation, FindLookAtRotation(CameraLocation, VolcanoCenter))
    if not IntroCamera then
        GameState.FinishIntro()
        return
    end
    SetViewTarget(IntroCamera)

    start_coroutine(function()
        wait(HoldBefore)

        EruptElapsed = 0
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
        local U = (T - RumbleRatio) / (1 - RumbleRatio)
        AimIntroCamera(U * U * (3 - 2 * U))
    end
end

function EndPlay()
    if BaseScale then
        SetEruptScale(1, 1)
    end
    if IsValid(IntroCamera) then
        IntroCamera:Destroy()
    end
end
