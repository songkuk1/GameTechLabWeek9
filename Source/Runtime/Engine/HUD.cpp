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