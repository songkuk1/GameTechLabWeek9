-- HUD 글자 그리기 도우미와 스코어보드 표. 타이틀(GameTitle)과 인게임(GameHUD)이 같이 쓴다.
-- DrawHUDText·MeasureHUDText는 스크립트마다 따로 등록되는 함수라 dofile로 읽은 이 파일에서는 보이지 않는다.
-- 그래서 만들 때 넘겨받는다.
--   local HUDText = dofile("Assets/Scripts/Lib/HUDText.lua")({ DrawHUDText = DrawHUDText, MeasureHUDText = MeasureHUDText })

return function(Api)
	local HUDText = {
		White = FLinearColor(1, 1, 1, 1),
		Yellow = FLinearColor(1, 0.85, 0.2, 1),
		Gray = FLinearColor(0.75, 0.75, 0.75, 1),
		Shadow = FLinearColor(0, 0, 0, 0.6),
	}
	local ShadowOffset = 3

	-- 배경이 밝아도 읽히도록 살짝 어긋난 그림자를 먼저 그린다
	function HUDText.Draw(Text, X, Y, Size, Color)
		Api.DrawHUDText(Text, X + ShadowOffset, Y + ShadowOffset, Size, HUDText.Shadow)
		Api.DrawHUDText(Text, X, Y, Size, Color or HUDText.White)
	end

	function HUDText.Centered(Text, CenterX, Y, Size, Color)
		HUDText.Draw(Text, CenterX - Api.MeasureHUDText(Text, Size) * 0.5, Y, Size, Color)
	end

	function HUDText.RightAligned(Text, RightX, Y, Size, Color)
		HUDText.Draw(Text, RightX - Api.MeasureHUDText(Text, Size), Y, Size, Color)
	end

	-- 제목 + 1~MaxRows등 표. 빈 순위는 ---로 채운다. Highlight[기록] = true면 노랗게 (이번 판에 들어간 기록)
	-- 차지하는 높이는 80 + 62 * MaxRows
	function HUDText.Board(Board, CenterX, TopY, MaxRows, Highlight)
		HUDText.Centered("SCOREBOARD", CenterX, TopY, 48, HUDText.Yellow)

		local Left, Right = CenterX - 280, CenterX + 280
		for Rank = 1, MaxRows do
			local Y = TopY + 80 + (Rank - 1) * 62
			local Entry = Board[Rank]
			local Color = (Entry and Highlight and Highlight[Entry]) and HUDText.Yellow or HUDText.White
			HUDText.Draw(Rank .. ".", Left, Y, 44, Color)
			if Entry then
				HUDText.Draw(Entry.Name, Left + 80, Y, 44, Color)
				HUDText.RightAligned(tostring(Entry.Score), Right, Y, 44, Color)
			else
				HUDText.Draw("---", Left + 80, Y, 44, HUDText.Gray)
			end
		end
	end

	return HUDText
end
