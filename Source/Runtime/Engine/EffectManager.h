#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Core/NameTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Component/ParticleSubUVComponent.h"

class UWorld;
class AActor;

// SpawnEffect가 돌려주는 핸들. 슬롯이 반환·재사용되면 Generation이 달라져 옛 핸들은 무시된다.
struct FEffectHandle
{
	int32 Index = -1;
	uint32 Generation = 0;

	bool IsValid() const { return Index >= 0; }
};

// 월드의 이펙트를 파티클 컴포넌트 풀에서 꺼내 재생하고, 끝나면 돌려받는다.
// UWorld가 소유한다. 게임 월드의 NotifyBeginPlay에서 Init, EndPlay·ClearWorld에서 Shutdown 한다.
// 시뮬레이션은 각 컴포넌트의 TickComponent가 하고, 매니저 Tick은 수명만 관리한다.
class FEffectManager
{
public:
	void Init(UWorld* InWorld);
	void Shutdown();
	// UWorld::Tick이 액터 Tick·겹침 이벤트·코루틴 뒤, FlushPendingKill 전에 부른다.
	void Tick(float DeltaTime);

	// 같은 이름이 있으면 덮어쓴다.
	void RegisterEffect(FName Name, const FEffectDesc& Desc);

	// Duration > 0: 그 시간 뒤 방출을 멈춘다.
	// Duration <= 0: 루프면 StopEffect까지 계속, 루프가 아니면 한 번 방출하고 끝난다.
	// 방출이 멈춘 뒤 남은 입자가 모두 사라지면 슬롯을 돌려받는다.
	FEffectHandle SpawnEffect(FName Effect, const FVector& Location, const FRotator& Rotation = FRotator(),
		bool bLoop = false, float Duration = 0.0f);

	// bImmediate가 false면 방출만 멈추고 남은 입자가 사라진 뒤 반환한다.
	void StopEffect(FEffectHandle Handle, bool bImmediate = false);
	bool IsEffectActive(FEffectHandle Handle) const;

	bool IsInitialized() const { return World != nullptr; }
	int32 GetPoolSize() const { return Slots.Num(); }
	int32 GetActiveCount() const;

private:
	enum class ESlotState : uint8
	{
		Free,       // 풀에서 쉬는 중
		Active,     // 방출 중
		Stopping,   // 방출을 멈추고 남은 입자가 사라지길 기다리는 중
	};

	struct FSlot
	{
		UParticleSubUVComponent* Component = nullptr;
		ESlotState State = ESlotState::Free;
		uint32 Generation = 0;
		float RemainingTime = -1.0f;    // 남은 방출 시간. 음수면 제한 없음
	};

	void RegisterDefaultEffects();
	bool AddPooledComponent();
	int32 AcquireSlot();
	void ReleaseSlot(int32 Index);
	const FSlot* FindSlot(FEffectHandle Handle) const;

	// 동시에 보일 이펙트 수 정도. 모자라면 MaxPoolSize까지 늘린다.
	static constexpr int32 InitialPoolSize = 16;
	static constexpr int32 MaxPoolSize = 64;

	UWorld* World = nullptr;
	// 풀 컴포넌트를 소유하는 숨은 액터. 컴포넌트는 Owner를 통해 월드와 Scene·Tick에 등록된다.
	AActor* PoolOwner = nullptr;

	TArray<FSlot> Slots;
	TArray<int32> FreeSlots;
	TMap<FName, FEffectDesc, FNameHash> Descs;

	// Init마다 올려 새 슬롯의 시작 Generation으로 쓴다. 이전 플레이의 핸들이 새 슬롯과 우연히 맞지 않게 한다.
	uint32 SessionGeneration = 0;
};
