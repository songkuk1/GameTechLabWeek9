#pragma once

#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Math/Vector2.h"
#include "Math/Color.h"
#include "Render/Texture2D.h"

class UObject;

// Rect 위에 Text
enum class EHUDCommandType : uint8
{
	Rect,
	Text,
    Image,
};

struct FHUDDrawCommand
{
	EHUDCommandType Type = EHUDCommandType::Rect;
	FVector2 Position;              // 위치 (Pixel)
	FVector2 Size;                  // 너비, 높이 (Pixel)
	FLinearColor Color;
	FString Text;                   // Text 전용
	float FontSize = 0.0f;          // Text 전용
    UTexture2D* Texture = nullptr;  // Image 전용
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

    // UI 좌표, 크기의 기준 해상도
	static constexpr float ReferenceWidth = 1920.0f;
	static constexpr float ReferenceHeight = 1080.0f;

	// 기준 단위 -> 실제 픽셀 배율. 비율을 유지하도록 가로/세로 중 작은 쪽에 맞춤
	float GetScale() const
	{
		if (ScreenSize.X <= 0.0f || ScreenSize.Y <= 0.0f)
			return 1.0f;
		return std::min(ScreenSize.X / ReferenceWidth, ScreenSize.Y / ReferenceHeight);
	}

	// 기준 단위로 본 화면 크기
	FVector2 GetVirtualSize() const
	{
		const float Scale = GetScale();
		return FVector2(ScreenSize.X / Scale, ScreenSize.Y / Scale);
	}

    void AddImage(UTexture2D* Texture, const FVector2& Position, const FVector2& Size, const FLinearColor& Color);

private:
	TArray<FHUDDrawCommand> Commands;
	FVector2 ScreenSize;
};

// Script가 그릴 HUD 찾기
// GameWorld가 아니거나 월드 없으면 nullptr
FHUD* FindHUDForObject(UObject* Object);