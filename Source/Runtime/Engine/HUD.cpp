#include "EnginePCH.h"
#include "Engine/HUD.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Component/ActorComponent.h"
#include "UObject/Casts.h"

FHUD* FindHUDForObject(UObject* Object)
{
	AActor* Actor = Cast<AActor>(Object);
	if (!Actor)
	{
		if (UActorComponent* Component = Cast<UActorComponent>(Object))
			Actor = Component->GetOwner();
	}

	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	// Editor 에서는 HUD를 그리지 않는다
	if (!World || !World->IsGameWorld())
		return nullptr;

	return &World->GetHUD();
}

// Color는 틴트. 흰색이면 텍스처 원래 색
void FHUD::AddImage(UTexture2D* Texture, const FVector2& Position, const FVector2& Size, const FLinearColor& Color)
{
    if (!Texture)
        return;

    FHUDDrawCommand Command;
    Command.Type = EHUDCommandType::Image;
    Command.Position = Position;
    Command.Size = Size;
    Command.Color = Color;
    Command.Texture = Texture;
    Commands.Add(std::move(Command));
}

FHUDButtonState FHUD::GetButtonState(const FVector2& Position, const FVector2& Size) const
{
    FHUDButtonState State;
    State.bHovered = Input.bEnabled
        && Input.MousePosition.X >= Position.X && Input.MousePosition.X < Position.X + Size.X
        && Input.MousePosition.Y >= Position.Y && Input.MousePosition.Y < Position.Y + Size.Y;
    State.bHeld = State.bHovered && Input.bMouseDown;
    State.bClicked = State.bHovered && Input.bMousePressed;
    return State;
}

namespace
{
    // Todo : 정렬 기능 추가...
    // 정렬 없이 왼쪽 여백 + 세로 가운데 근사로 버튼 라벨 위치를 정한다
    FVector2 GetButtonLabelPosition(const FVector2& Position, const FVector2& Size, float FontSize)
    {
        constexpr float Padding = 12.0f;
        return FVector2(Position.X + Padding, Position.Y + (Size.Y - FontSize) * 0.5f);
    }
}

bool FHUD::Button(const FString& Label, const FVector2& Position, const FVector2& Size, float FontSize)
{
    const FHUDButtonState State = GetButtonState(Position, Size);

    // 상태별 배경색: Default / Hover / Pressed
    const FLinearColor Background =
        State.bHeld    ? FLinearColor(0.12f, 0.12f, 0.12f, 0.9f) :
        State.bHovered ? FLinearColor(0.38f, 0.38f, 0.38f, 0.9f) :
                         FLinearColor(0.25f, 0.25f, 0.25f, 0.9f);
    AddRect(Position, Size, Background);

    AddText(Label, GetButtonLabelPosition(Position, Size, FontSize), FontSize, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));

    return State.bClicked;
}

bool FHUD::ImageButton(UTexture2D* Texture, const FVector2& Position, const FVector2& Size,
    const FString& Label, float FontSize)
{
    const FHUDButtonState State = GetButtonState(Position, Size);

    // 이미지 셰이더가 텍스처 색 × 틴트라서 틴트 밝기로 상태를 표현한다. 1을 넘는 값은 표시용 타깃에서 잘린다.
    const float Brightness = State.bHeld ? 0.7f : (State.bHovered ? 1.2f : 1.0f);
    AddImage(Texture, Position, Size, FLinearColor(Brightness, Brightness, Brightness, 1.0f));

    if (!Label.empty())
        AddText(Label, GetButtonLabelPosition(Position, Size, FontSize), FontSize, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));

    // 텍스처가 없어도(경로 오류) 판정은 동작한다
    return State.bClicked;
}