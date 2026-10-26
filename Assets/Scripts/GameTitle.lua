-- 타이틀 화면: 배경 + 가운데 타이틀 로고 + 게임 시작 / 나가기 버튼 + 조작법·스코어보드 버튼
-- 버튼은 HUDImageButton이라 마우스를 올리면 밝게, 누르는 중이면 어둡게 그려진다.
-- 조작법·스코어보드 버튼과 창은 이미지가 정해지기 전까지 글자 버튼(HUDButton)으로 둔다.

local Scoreboard = require("Assets.Scripts.Lib.Scoreboard")
local HUDText = dofile("Assets/Scripts/Lib/HUDText.lua")({ DrawHUDText = DrawHUDText, MeasureHUDText = MeasureHUDText })

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

-- 오른쪽 아래 구석에 세로로 쌓는 글자 버튼 (위: CONTROLS, 아래: SCOREBOARD)
local CornerButtonW, CornerButtonH = 320, 70
local CornerButtonMargin = 40   -- 화면 오른쪽 아래 모서리에서 떨어진 거리
local CornerButtonGap = 20

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

-- 창 아래 가운데 닫기 버튼
local function CloseButton(CenterX, Y)
    local CloseW, CloseH = 260, 80
    if HUDButton("CLOSE", CenterX - CloseW * 0.5, Y, CloseW, CloseH, 40) then
        OpenPanel = nil
    end
end

local function DrawScoreboardPanel(W, H)
    local CenterX = W * 0.5
    local TopY = H * 0.5 - 280
    HUDText.Board(OpenBoard, CenterX, TopY, Scoreboard.MaxEntries)
    CloseButton(CenterX, TopY + 80 + 62 * Scoreboard.MaxEntries + 40)
end

-- P1은 왼쪽, P2는 오른쪽 칸. 칸마다 "동작   키" 줄을 그린다
local function DrawControlsPanel(W, H)
    local CenterX = W * 0.5
    local TopY = H * 0.5 - 280
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
    CloseButton(CenterX, TopY + 460)
end

local ActionPending = false
local StartSound = nil
local QuitSound = nil

function BeginPlay()
    ActionPending = false
    StartSound = nil
    QuitSound = nil
    OpenPanel = nil
    OpenBoard = nil
    Audio.PlayBGM("Assets/Audio/BGM.wav")
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

    -- 버튼 두 개를 가운데 기준으로 나란히 둔다
    local ButtonH = ButtonWidth / ButtonAspect
    local Y = H * ButtonTopY
    local StartX = W * 0.5 - ButtonGap * 0.5 - ButtonWidth
    local ExitX = W * 0.5 + ButtonGap * 0.5

    local StartClicked = HUDImageButton(StartImage, StartX, Y, ButtonWidth, ButtonH)
    local ExitClicked = HUDImageButton(ExitImage, ExitX, Y, ButtonWidth, ButtonH)

    if ActionPending then
        if StartSound ~= nil and not Audio.IsPlaying(StartSound) then
            StartSound = nil
            OpenScene(GameScene)
        elseif QuitSound ~= nil and not Audio.IsPlaying(QuitSound) then
            QuitSound = nil
            QuitGame()
        end
        return
    end

    if StartClicked then
        ActionPending = true
        StartSound = Audio.Play("Assets/Audio/Start_Oi.wav")
        if StartSound == 0 then
            StartSound = nil
            OpenScene(GameScene)
        end
        return
    elseif ExitClicked then
        ActionPending = true
        Audio.StopBGM()
        QuitSound = Audio.Play("Assets/Audio/Quit.wav")
        if QuitSound == 0 then
            QuitSound = nil
            QuitGame()
        end
        return
    end

    -- 오른쪽 아래: 조작법 / 스코어보드 열기
    local CornerX = W - CornerButtonMargin - CornerButtonW
    local ScoreboardY = H - CornerButtonMargin - CornerButtonH
    local ControlsY = ScoreboardY - CornerButtonGap - CornerButtonH

    if HUDButton("CONTROLS", CornerX, ControlsY, CornerButtonW, CornerButtonH, 40) then
        OpenPanel = "Controls"
    end
    if HUDButton("SCOREBOARD", CornerX, ScoreboardY, CornerButtonW, CornerButtonH, 40) then
        OpenBoard = Scoreboard.Load()
        OpenPanel = "Scoreboard"
    end
end
