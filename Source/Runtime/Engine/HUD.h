#pragma once

#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Math/Vector2.h"
#include "Math/Color.h"

// Rect 위에 Text
enum class EHUDCommandType : uint8
{
	Rect,
	Text,
};

struct FHUDDrawCommand
{
	EHUDCommandType Type = EHUDCommandType::Rect;
	FVector2 Position;         // 위치 (Pixel)
	FVector2 Size;             // 너비, 높이 (Pixel)
	FLinearColor Color;
	FString Text;              // Text 전용
	float FontSize = 0.0f;     // Text 전용
};

// 월드의 화면 위 UI. 매 프레임 비우고 새로 채움.
// 무엇을 그릴지 담아둠. ( FHUDRenderer 가 그림 )
class FHUD
{
public:
	// 월드 Tick 시작 시 호출. 지난 프레임 명령을 버린다.
	void BeginFrame() { Commands.Reset(); }

    // 사각형 추가
	void AddRect(const FVector2& Position, const FVector2& Size, const FLinearColor& Color)
	{
		FHUDDrawCommand Command;
		Command.Type = EHUDCommandType::Rect;
		Command.Position = Position;
		Command.Size = Size;
		Command.Color = Color;
		Commands.Add(std::move(Command));
	}

    // 텍스트 추가
	void AddText(const FString& Text, const FVector2& Position, float FontSize, const FLinearColor& Color)
	{
		FHUDDrawCommand Command;
		Command.Type = EHUDCommandType::Text;
		Command.Position = Position;
		Command.Text = Text;
		Command.FontSize = FontSize;
		Command.Color = Color;
		Commands.Add(std::move(Command));
	}

	const TArray<FHUDDrawCommand>& GetCommands() const { return Commands; }

	// HUD가 그려지는 화면 크기. ( Viewport가 채움 )
	const FVector2& GetScreenSize() const { return ScreenSize; }
	void SetScreenSize(const FVector2& InSize) { ScreenSize = InSize; }

private:
	TArray<FHUDDrawCommand> Commands;
	FVector2 ScreenSize;
};