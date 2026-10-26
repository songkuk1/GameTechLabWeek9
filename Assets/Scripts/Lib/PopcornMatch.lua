-- 플레이 월드 하나의 게임잼 규칙. 플레이어와 스포너가 require로 같은 상태를 공유한다.
local Match = { WinningScore = 3, Scores = { 0, 0 }, Players = {} }
local ClipPaths = {
    [0] = "Assets/SubUV/Videos/Draw/clip.lua",
    [1] = "Assets/SubUV/Videos/Chikawa_5_11_clean/clip.lua",
    [2] = "Assets/SubUV/Videos/Usagi_0_10/clip.lua",
}

function Match.RegisterPlayer(Index, Actor)
    Match.Players[Index] = Actor
end

function Match.UnregisterPlayer(Index, Actor)
    if Match.Players[Index] == Actor then
        Match.Players[Index] = nil
    end
end

function Match.GetPlayerIndex(Actor)
    for Index = 1, 2 do
        if Match.Players[Index] == Actor then
            return Index
        end
    end
end

function Match.Reset()
    if Match.SoundId then Audio.Stop(Match.SoundId) end
    Match.Scores = { 0, 0 }
    Match.Result = nil
    Match.Clip = nil
    Match.SoundId = nil
    Match.Elapsed = 0
end

function Match.IsFinished()
    return Match.Result ~= nil
end

local function Finish(Result)
    if Match.IsFinished() then return end
    Match.Result = Result
    Match.Clip = dofile(ClipPaths[Result])
    Match.Elapsed = 0
    Audio.StopBGM()
    Match.SoundId = Audio.Play(Match.Clip.AudioPath)
end

function Match.AddPopcorn(Actor)
    local Index = Match.GetPlayerIndex(Actor)
    if not Index or Match.IsFinished() then return end
    Match.Scores[Index] = Match.Scores[Index] + 1
    if Match.Scores[Index] >= Match.WinningScore then Finish(Index) end
end

-- 나중에 타이머가 끝났을 때 호출한다. 동점은 Draw 영상으로 보낸다.
function Match.FinishByScore()
    if Match.Scores[1] == Match.Scores[2] then
        Finish(0)
    else
        Finish(Match.Scores[1] > Match.Scores[2] and 1 or 2)
    end
end

function Match.Update(dt)
    if not Match.Clip then return end
    if Match.SoundId ~= 0 then
        Match.Elapsed = Audio.IsPlaying(Match.SoundId)
            and Audio.GetPosition(Match.SoundId) or Match.Clip.Duration
    else
        -- 오디오 장치/파일이 없어도 영상은 끝까지 진행한다.
        Match.Elapsed = math.min(Match.Elapsed + dt, Match.Clip.Duration)
    end
end

function Match.GetVideoFrame()
    local Clip = Match.Clip
    if not Clip then return end
    local Frame = math.min(math.floor(Match.Elapsed * Clip.FrameRate), Clip.FrameCount - 1)
    for _, Page in ipairs(Clip.Pages) do
        if Frame >= Page.FirstFrame and Frame < Page.FirstFrame + Page.FrameCount then
            local LocalFrame = Frame - Page.FirstFrame
            local X = (LocalFrame % Clip.Columns) * Clip.CellWidth + Clip.Padding
            local Y = math.floor(LocalFrame / Clip.Columns) * Clip.CellHeight + Clip.Padding
            return Page.Texture, (X + 0.5) / Page.Width, (Y + 0.5) / Page.Height,
                (X + Clip.FrameWidth - 0.5) / Page.Width, (Y + Clip.FrameHeight - 0.5) / Page.Height
        end
    end
end

return Match
