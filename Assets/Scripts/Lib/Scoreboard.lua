-- 점수 높은 순 상위 기록. 엔진의 SaveScoreboard/LoadScoreboard로 Saved/Scoreboard.json에 저장한다.
-- 기록 하나: { Name = "ABC", Score = 12 }

local Scoreboard = {
	MaxEntries = 5,
}

-- 파일에서 읽는다. 손으로 고친 파일 등에서 모양이 틀린 기록은 버린다
function Scoreboard.Load()
	local Board = {}
	for _, Entry in ipairs(LoadScoreboard()) do
		if type(Entry) == "table" and type(Entry.Name) == "string" and type(Entry.Score) == "number" then
			table.insert(Board, { Name = Entry.Name, Score = Entry.Score })
		end
	end
	table.sort(Board, function(A, B) return A.Score > B.Score end)
	while #Board > Scoreboard.MaxEntries do
		table.remove(Board)
	end
	return Board
end

-- 이 점수로 기록에 들어갈 수 있는지. 꼴찌와 같은 점수면 먼저 들어간 기록이 남는다
function Scoreboard.Qualifies(Board, Score)
	return #Board < Scoreboard.MaxEntries or Score > Board[#Board].Score
end

-- 같은 점수끼리는 먼저 들어간 기록이 위에 오도록, 자기보다 높거나 같은 기록 바로 뒤에 넣는다.
-- 넣은 순위(1부터)를 돌려준다. 저장까지 한다
function Scoreboard.Insert(Board, Name, Score)
	local Rank = #Board + 1
	for i, Entry in ipairs(Board) do
		if Score > Entry.Score then
			Rank = i
			break
		end
	end
	table.insert(Board, Rank, { Name = Name, Score = Score })

	while #Board > Scoreboard.MaxEntries do
		table.remove(Board)
	end

	if not SaveScoreboard(Board) then
		print("[Scoreboard] 저장 실패")
	end
	return Rank
end

return Scoreboard
