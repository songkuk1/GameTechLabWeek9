-- 타이틀 화면: 배경 + 가운데 타이틀 로고 + 게임 시작 / 나가기 버튼, 그 아래 순위(RANK) / 조작법 버튼
-- 버튼은 HUDImageButton이라 마우스를 올리면 밝게, 누르는 중이면 어둡게 그려진다.
-- 조작법 버튼과 창은 이미지가 정해지기 전까지 글자로 둔다. 창은 오른쪽 위 X 버튼으로 닫는다.

local Scoreboard = require("Assets.Scripts.Lib.Scoreboard")
local HUDText = dofile("Assets/Scripts/Lib/HUDText.lua")({
    DrawHUDText = DrawHUDText, MeasureHUDText = MeasureHUDText, DrawHUDImage = DrawHUDImage,
    HUDImageButton = HUDImageButton })

local GameScene = "Assets/Scenes/GameJamMap0.scene"

local Background  = "Assets/UI/Backgrounds/Main.png"
local TitleImage  = "Assets/UI/Logos/Title.png"
local StartImage  = "Assets/UI/Buttons/START.png"
local ExitImage   = "Assets/UI/Buttons/EXIT.png"
local RankImage   = "Assets/UI/Buttons/RANK.png"

-- 이미지 원본 비율(가로/세로). 크기는 가로만 정하고 세로는 비율로 맞춰서 찌그러지지 않게 한다.
local TitleAspect  = 432 / 238
local ButtonAspect = 1645 / 962
local RankAspect   = 1463 / 691

local TitleWidth  = 700     -- HUD 기준 해상도(1920x1080) 단위
local TitleCenterY = 0.36   -- 화면 높이 대비 타이틀 중심 위치
local ButtonWidth = 280     -- 모든 버튼 가로 폭. 왼쪽 열: START·RANK, 오른쪽 열: EXIT·CONTROLS
local ButtonGap   = 60      -- 두 열 사이 가로 간격
local ButtonTopY  = 0.58    -- 화면 높이 대비 첫 줄(START·EXIT) 윗변 위치
local RowGap      = 16      -- 첫 줄과 둘째 줄 사이
local ControlsButtonH = 80  -- 조작법 글자 버튼 높이 (RANK 이미지와 세로 가운데를 맞춘다)

local CloseSize = 90        -- 창 오른쪽 위 X 버튼 크기

local DimColor = FLinearColor(0, 0, 0, 0.75)
local PlayerColors = { FLinearColor(1, 0.55, 0.6, 1), FLinearColor(0.55, 0.8, 1, 1) }

-- 조작법 창에 보일 내용. 키를 바꾸면 Player1/2.lua의 Config와 같이 고친다
local Controls = {
    { Title = "P1", Rows = { { "MOVE", "W A S D" }, { "PUSH", "SPACE" } } },
    { Title = "P2", Rows = { { "MOVE", "ARROW KEYS" }, { "PUSH", "ENTER" } } },
}

-- 열린 창: nil | "Scoreboard" | "Controls"
local OpenPanel = nil
local OpenBoard = nil   -- 스코어보드 창을 열 때마다 파일에서 다시 읽는다

-- RANK 팝업. 테두리 상자 오른쪽 위에 X
local function DrawScoreboardPanel(W, H)
    local PopupH = 820
    local CloseRight, CloseTop = HUDText.RankBoard(OpenBoard, W * 0.5, (H - PopupH) * 0.5, PopupH)
    if HUDText.CloseButton(CloseRight, CloseTop, CloseSize) then
        OpenPanel = nil
    end
end

-- P1은 왼쪽, P2는 오른쪽 칸. 칸마다 "동작   키" 줄을 그린다
local function DrawControlsPanel(W, H)
    local CenterX = W * 0.5
    local TopY = H * 0.5 - 240
    HUDText.Centered("CONTROLS", CenterX, TopY, 56, HUDText.Yellow)

    local ColumnW = 460
    for Index, Player in ipairs(Controls) do
        local Left = CenterX + (Index == 1 and -ColumnW - 40 or 40)
        HUDText.Draw(Player.Title, Left, TopY + 110, 52, PlayerColors[Index])
        for Row, Entry in ipairs(Player.Rows) do
            local Y = TopY + 190 + (Row - 1) * 70
            HUDText.Draw(Entry[1], Left, Y, 40, HUDText.Gray)
            HUDText.RightAligned(Entry[2], Left + ColumnW, Y, 40)
        end
    end

    HUDText.Centered("ESC  PAUSE", CenterX, TopY + 370, 36, HUDText.Gray)

    -- 두 칸의 오른쪽 끝 위
    if HUDText.CloseButton(CenterX + ColumnW + 40, TopY - 20, CloseSize) then
        OpenPanel = nil
    end
end

function Tick(dt)
    local W, H = GetHUDSize()

    -- 배경은 화면 전체를 덮는다
    DrawHUDImage(Background, 0, 0, W, H)

    -- 타이틀 로고: 화면 가운데
    local TitleH = TitleWidth / TitleAspect
    DrawHUDImage(TitleImage, (W - TitleWidth) * 0.5, H * TitleCenterY - TitleH * 0.5, TitleWidth, TitleH)

    -- 창이 열려 있으면 그 위에 창만 그리고, 아래 버튼은 누를 수 없게 한다
    if OpenPanel then
        DrawHUDRect(0, 0, W, H, DimColor)
        if OpenPanel == "Scoreboard" then
            DrawScoreboardPanel(W, H)
        else
            DrawControlsPanel(W, H)
        end
        return
    end

    -- 두 열을 가운데 기준으로 나란히 둔다
    local LeftX = W * 0.5 - ButtonGap * 0.5 - ButtonWidth
    local RightX = W * 0.5 + ButtonGap * 0.5

    -- 첫 줄: 시작 / 나가기
    local ButtonH = ButtonWidth / ButtonAspect
    local Y = H * ButtonTopY
    if HUDImageButton(StartImage, LeftX, Y, ButtonWidth, ButtonH) then
        OpenScene(GameScene)
    end
    if HUDImageButton(ExitImage, RightX, Y, ButtonWidth, ButtonH) then
        QuitGame()
    end

    -- 둘째 줄: 순위 / 조작법
    local RankH = ButtonWidth / RankAspect
    local Row2Y = Y + ButtonH + RowGap
    if HUDImageButton(RankImage, LeftX, Row2Y, ButtonWidth, RankH) then
        OpenBoard = Scoreboard.Load()
        OpenPanel = "Scoreboard"
    end
    if HUDButton("CONTROLS", RightX, Row2Y + (RankH - ControlsButtonH) * 0.5, ButtonWidth, ControlsButtonH, 40) then
        OpenPanel = "Controls"
    end
end
