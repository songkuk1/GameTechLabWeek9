-- 인게임 진행과 HUD
--   Ready   : 가운데 3, 2, 1 카운트다운. 이동·낙하 멈춤
--   Playing : 제한 시간(1분) 동안 줍기. 상단 가운데 남은 시간, 왼쪽 P1 점수, 오른쪽 P2 점수
--   Result  : 승자·무승부 영상 → 승자 표시 → 이름 입력 → 스코어보드와 다시하기·메인 메뉴 버튼
--   Esc     : Ready·Playing 중 일시정지 창 (재시작·메인 메뉴). 다시 누르면 이어 한다
-- 단계와 점수는 Lib/GameState.lua, 기록 저장은 Lib/Scoreboard.lua가 맡는다.
-- UI 이미지가 정해지면 DrawText·DrawCentered와 사각형 부분만 DrawHUDImage로 바꾸면 된다.

local GameState = require("Assets.Scripts.Lib.GameState")
local Scoreboard = require("Assets.Scripts.Lib.Scoreboard")
local ResultVideo = require("Assets.Scripts.Lib.ResultVideo")

local GameScene = "Assets/Scenes/GameJamMap0.scene"
local TitleScene = "Assets/Scenes/GameJamTitle.scene"

local TopMargin = 32        -- 화면 위에서 글자 윗변까지 (HUD 기준 해상도 1920x1080 단위)
local SideMargin = 64       -- 화면 양옆에서 점수 글자까지
local TimeFontSize = 72
local ScoreFontSize = 56
local WarningTime = 10      -- 남은 시간이 이 아래면 시간을 빨갛게
local GoShowTime = 0.8      -- 시작할 때 GO!를 보여 줄 시간 (초)

local MaxNameLength = 8
local NameChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

local HUDText = dofile("Assets/Scripts/Lib/HUDText.lua")({ DrawHUDText = DrawHUDText, MeasureHUDText = MeasureHUDText })
local DrawText, DrawCentered, DrawRightAligned = HUDText.Draw, HUDText.Centered, HUDText.RightAligned

local White, Yellow, Gray = HUDText.White, HUDText.Yellow, HUDText.Gray
local Red = FLinearColor(1, 0.3, 0.3, 1)
local DimColor = FLinearColor(0, 0, 0, 0.65)
local BoxColor = FLinearColor(1, 1, 1, 0.15)
local PlayerColors = { FLinearColor(1, 0.55, 0.6, 1), FLinearColor(0.55, 0.8, 1, 1) }

-- 결과 화면 상태
local Board = nil            -- Scoreboard.Load()로 읽은 기록
local EntryQueue = {}        -- 이름을 입력할 차례를 기다리는 플레이어 번호
local CurrentEntry = nil     -- { PlayerNumber, Name } 지금 이름을 입력하는 플레이어
local NewEntries = {}        -- 이번 판에 들어간 기록 (스코어보드에서 강조)

-- 초 → "MM:SS". 남은 시간은 올림해야 60초가 처음부터 00:59로 보이지 않는다
local function FormatTime(Seconds)
	local Total = math.ceil(Seconds)
	return string.format("%02d:%02d", math.floor(Total / 60), Total % 60)
end

-- 다음 플레이어 차례로 넘긴다. 앞 사람이 들어가서 순위가 밀렸으면 그때 다시 따진다
local function StartNextEntry()
	CurrentEntry = nil
	while #EntryQueue > 0 do
		local PlayerNumber = table.remove(EntryQueue, 1)
		if Scoreboard.Qualifies(Board, GameState.GetScore(PlayerNumber)) then
			CurrentEntry = { PlayerNumber = PlayerNumber, Name = "" }
			return
		end
	end
end

local function StartResult()
	GameState.SetPhase("Result")
	ResultVideo.Play(GameState.GetWinner())
	Board = Scoreboard.Load()
	NewEntries = {}

	-- 점수 높은 사람부터 입력한다. 같으면 P1 먼저
	EntryQueue = { 1, 2 }
	table.sort(EntryQueue, function(A, B)
		local SA, SB = GameState.GetScore(A), GameState.GetScore(B)
		if SA ~= SB then return SA > SB end
		return A < B
	end)
	StartNextEntry()
end

-- 이름 입력 키. 결과 화면에서 입력 차례일 때만 받는다
local function TypeChar(C)
	if not ResultVideo.IsPlaying() and CurrentEntry and #CurrentEntry.Name < MaxNameLength then
		CurrentEntry.Name = CurrentEntry.Name .. C
	end
end

local function Backspace()
	if not ResultVideo.IsPlaying() and CurrentEntry and #CurrentEntry.Name > 0 then
		CurrentEntry.Name = CurrentEntry.Name:sub(1, -2)
	end
end

local function ConfirmName()
	if ResultVideo.IsPlaying() or not CurrentEntry then
		return
	end
	-- 비워 두고 Enter를 누르면 플레이어 이름으로 남긴다
	local Name = CurrentEntry.Name ~= "" and CurrentEntry.Name or ("P" .. CurrentEntry.PlayerNumber)
	local Rank = Scoreboard.Insert(Board, Name, GameState.GetScore(CurrentEntry.PlayerNumber))
	NewEntries[Board[Rank]] = true
	StartNextEntry()
end

function BeginPlay()
	ResultVideo.Reset()
	GameState.Reset()
	Audio.PlayBGM("Assets/Audio/BGM.wav")

	-- 키보드는 하나라 0번 컨트롤러로 이름을 받는다. 이동 바인딩과 같이 올라가지만 단계가 달라 겹치지 않는다
	local Input = EnableInput(0)
	if Input then
		for i = 1, #NameChars do
			local C = NameChars:sub(i, i)
			Input:BindKey(C, "Pressed", function() TypeChar(C) end)
		end
		Input:BindKey("Backspace", "Pressed", Backspace)
		Input:BindKey("Enter", "Pressed", ConfirmName)
		Input:BindKey("Escape", "Pressed", GameState.TogglePause)
	end
end

-- 일시정지 창. 이미지가 정해지면 배경과 버튼만 DrawHUDImage·HUDImageButton으로 바꾼다
local function DrawPause(W, H)
	local CenterX = W * 0.5
	DrawHUDRect(0, 0, W, H, DimColor)
	DrawCentered("PAUSED", CenterX, H * 0.5 - 220, 120, Yellow)
	DrawCentered("ESC: RESUME", CenterX, H * 0.5 - 60, 32, Gray)

	local ButtonW, ButtonH, Gap, ButtonY = 300, 80, 40, H * 0.5 + 40
	if HUDButton("RESTART", CenterX - Gap * 0.5 - ButtonW, ButtonY, ButtonW, ButtonH, 40) then
		OpenScene(GameScene)
	end
	if HUDButton("MAIN MENU", CenterX + Gap * 0.5, ButtonY, ButtonW, ButtonH, 40) then
		OpenScene(TitleScene)
	end
end

local function DrawTopBar(W)
	-- 상단 가운데: 남은 시간
	local Remaining = GameState.GetRemainingTime()
	local TimeColor = (GameState.IsPlaying() and Remaining <= WarningTime) and Red or White
	DrawCentered(FormatTime(Remaining), W * 0.5, TopMargin, TimeFontSize, TimeColor)

	-- 시간 글자와 세로 가운데를 맞춘다
	local ScoreY = TopMargin + (TimeFontSize - ScoreFontSize) * 0.5
	DrawText("P1  " .. GameState.GetScore(1), SideMargin, ScoreY, ScoreFontSize, PlayerColors[1])
	DrawRightAligned("P2  " .. GameState.GetScore(2), W - SideMargin, ScoreY, ScoreFontSize, PlayerColors[2])
end

local function DrawNameEntry(CenterX)
	local PlayerNumber = CurrentEntry.PlayerNumber
	DrawCentered("P" .. PlayerNumber .. "  NEW RECORD!  ENTER YOUR NAME", CenterX, 450, 44, PlayerColors[PlayerNumber])

	local BoxW, BoxH = 520, 90
	DrawHUDRect(CenterX - BoxW * 0.5, 520, BoxW, BoxH, BoxColor)
	-- 깜빡이는 커서
	local Cursor = (math.floor(GameState.PhaseTime * 2) % 2 == 0 and #CurrentEntry.Name < MaxNameLength) and "_" or ""
	DrawCentered(CurrentEntry.Name .. Cursor, CenterX, 537, 56)

	DrawCentered("A-Z 0-9   BACKSPACE: DELETE   ENTER: OK", CenterX, 640, 28, Gray)
end

local function DrawBoard(CenterX)
	HUDText.Board(Board, CenterX, 430, Scoreboard.MaxEntries, NewEntries)

	-- 버튼 두 개를 가운데 기준으로 나란히. 이미지는 나중에 HUDImageButton으로 바꾼다
	local ButtonW, ButtonH, Gap, ButtonY = 300, 80, 40, 860
	if HUDButton("RETRY", CenterX - Gap * 0.5 - ButtonW, ButtonY, ButtonW, ButtonH, 40) then
		OpenScene(GameScene)
	end
	if HUDButton("MAIN MENU", CenterX + Gap * 0.5, ButtonY, ButtonW, ButtonH, 40) then
		OpenScene(TitleScene)
	end
end

local function DrawResult(W, H)
	local CenterX = W * 0.5
	DrawHUDRect(0, 0, W, H, DimColor)

	local Winner = GameState.GetWinner()
	if Winner then
		DrawCentered("P" .. Winner .. " WINS!", CenterX, 170, 120, PlayerColors[Winner])
	else
		DrawCentered("DRAW", CenterX, 170, 120, Yellow)
	end
	DrawCentered("P1  " .. GameState.GetScore(1) .. "   :   " .. GameState.GetScore(2) .. "  P2", CenterX, 320, 56)

	if CurrentEntry then
		DrawNameEntry(CenterX)
	else
		DrawBoard(CenterX)
	end
end

local function DrawVideo(W, H)
	DrawHUDRect(0, 0, W, H, FLinearColor(0, 0, 0, 1))
	local Texture, U0, V0, U1, V1 = ResultVideo.GetVideoFrame()
	if Texture then
		local Clip = ResultVideo.Clip
		local Scale = math.min(960 / Clip.FrameWidth, W / Clip.FrameWidth, H / Clip.FrameHeight)
		local Width, Height = Clip.FrameWidth * Scale, Clip.FrameHeight * Scale
		DrawHUDImageRegion(Texture, (W - Width) * 0.5, (H - Height) * 0.5, Width, Height, U0, V0, U1, V1)
	end
end

function Tick(dt)
	ResultVideo.Update(dt)
	-- 일시정지 중에는 시간이 흐르지 않는다 (카운트다운·제한 시간 모두)
	if not GameState.bPaused then
		GameState.PhaseTime = GameState.PhaseTime + dt

		if GameState.Phase == "Ready" and GameState.PhaseTime >= GameState.ReadyTime then
			GameState.SetPhase("Playing")
		elseif GameState.Phase == "Playing" and GameState.PhaseTime >= GameState.TimeLimit then
			StartResult()
		end
	end

	local W, H = GetHUDSize()
	if ResultVideo.IsPlaying() then
		DrawVideo(W, H)
		return
	end
	DrawTopBar(W)

	if GameState.bPaused then
		DrawPause(W, H)
	elseif GameState.Phase == "Ready" then
		local Count = math.ceil(GameState.ReadyTime - GameState.PhaseTime)
		DrawCentered(tostring(Count), W * 0.5, H * 0.5 - 120, 200, Yellow)
	elseif GameState.Phase == "Playing" and GameState.PhaseTime < GoShowTime then
		DrawCentered("GO!", W * 0.5, H * 0.5 - 120, 200, Yellow)
	elseif GameState.Phase == "Result" then
		DrawResult(W, H)
	end
end

function EndPlay()
	ResultVideo.Reset()
end
