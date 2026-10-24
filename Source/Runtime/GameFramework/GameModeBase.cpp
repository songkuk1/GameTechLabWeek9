#include "EnginePCH.h"
#include "GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

void AGameModeBase::PreInitializeComponents()
{
	if (!GameStateClass)
		GameStateClass = AGameStateBase::StaticClass();

	UWorld* OwningWorld = GetWorld();
	if (!OwningWorld)
		return;

	GameState = Cast<AGameStateBase>(OwningWorld->SpawnActor(GameStateClass));
	if (!GameState)
	{
		HTR_LOG(Error, "[GameMode] failed to spawn GameState");
		return;
	}
	// GameMode와 마찬가지로 씬 파일에 저장하지 않는다.
	GameState->SetFlags(EObjectFlags::RF_Transient);
	OwningWorld->SetGameState(GameState);
}

void AGameModeBase::StartPlay()
{
	if (GameState)
		GameState->HandleBeginPlay();
}
