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