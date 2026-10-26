-- 타이틀 화면: 배경 + 가운데 타이틀 로고 + 게임 시작 / 나가기 버튼
-- 버튼은 HUDImageButton이라 마우스를 올리면 밝게, 누르는 중이면 어둡게 그려진다.

local GameScene = "Assets/Scenes/GameJamMap0.scene"

local Background  = "Assets/UI/Backgrounds/Main.png"
local TitleImage  = "Assets/UI/Logos/Title.png"
local StartImage  = "Assets/UI/Buttons/START.png"
local ExitImage   = "Assets/UI/Buttons/EXIT.png"

-- 이미지 원본 비율(가로/세로). 크기는 가로만 정하고 세로는 비율로 맞춰서 찌그러지지 않게 한다.
local TitleAspect  = 432 / 238
local ButtonAspect = 1645 / 962

local TitleWidth  = 760     -- HUD 기준 해상도(1920x1080) 단위
local TitleCenterY = 0.40   -- 화면 높이 대비 타이틀 중심 위치
local ButtonWidth = 300
local ButtonGap   = 60      -- 두 버튼 사이 가로 간격
local ButtonTopY  = 0.68    -- 화면 높이 대비 버튼 윗변 위치

function Tick(dt)
    local W, H = GetHUDSize()

    -- 배경은 화면 전체를 덮는다
    DrawHUDImage(Background, 0, 0, W, H)

    -- 타이틀 로고: 화면 가운데
    local TitleH = TitleWidth / TitleAspect
    DrawHUDImage(TitleImage, (W - TitleWidth) * 0.5, H * TitleCenterY - TitleH * 0.5, TitleWidth, TitleH)

    -- 버튼 두 개를 가운데 기준으로 나란히 둔다
    local ButtonH = ButtonWidth / ButtonAspect
    local Y = H * ButtonTopY
    local StartX = W * 0.5 - ButtonGap * 0.5 - ButtonWidth
    local ExitX = W * 0.5 + ButtonGap * 0.5

    if HUDImageButton(StartImage, StartX, Y, ButtonWidth, ButtonH) then
        OpenScene(GameScene)
    end

    if HUDImageButton(ExitImage, ExitX, Y, ButtonWidth, ButtonH) then
        QuitGame()
    end
end
