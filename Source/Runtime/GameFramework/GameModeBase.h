#pragma once

#include "GameFramework/Actor.h"

class AGameStateBase;

// 게임 규칙을 맡는 액터. 플레이 월드(Game·PIE)마다 하나 스폰되고 월드와 함께 사라진다.
// 에디터 월드에는 없고 씬 파일에도 저장되지 않는다(RF_Transient).
class AGameModeBase : public AActor
{
	DECLARE_CLASS(AGameModeBase, AActor)
public:
	AGameModeBase() = default;
	virtual ~AGameModeBase() = default;

	// UE에서는 액터 초기화 중에 불린다. 여기서는 UWorld::SetGameMode가 스폰 직후 부른다.
	// GameStateClass로 GameState를 스폰하고 World에 알린다.
	void PreInitializeComponents();

	// UWorld::BeginPlay가 부른다. GameState를 거쳐 월드의 모든 액터 BeginPlay를 시작시킨다.
	virtual void StartPlay();

	AGameStateBase* GetGameState() const { return GameState; }

	UClass* GameStateClass = nullptr;

protected:
	AGameStateBase* GameState = nullptr;
};
