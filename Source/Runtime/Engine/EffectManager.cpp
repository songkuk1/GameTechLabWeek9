#include "EnginePCH.h"
#include "Engine/EffectManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void FEffectManager::Init(UWorld* InWorld)
{
	if (!InWorld || World == InWorld)
		return;

	if (World)
		Shutdown();

	World = InWorld;
	++SessionGeneration;

	PoolOwner = World->SpawnActor<AActor>(FName("EffectPool"));
	if (!PoolOwner)
	{
		HTR_LOG(Error, "[Effect] failed to spawn pool owner");
		World = nullptr;
		return;
	}

	Slots.Reserve(InitialPoolSize);
	FreeSlots.Reserve(InitialPoolSize);
	for (int32 i = 0; i < InitialPoolSize; ++i)
	{
		if (!AddPooledComponent())
			break;
	}

	RegisterDefaultEffects();
}

// 풀 액터와 컴포넌트는 월드가 액터를 정리할 때 함께 지운다. 여기서는 참조만 끊는다.
void FEffectManager::Shutdown()
{
	Slots.Empty();
	FreeSlots.Empty();
	Descs.Empty();
	PoolOwner = nullptr;
	World = nullptr;
}

void FEffectManager::Tick(float DeltaTime)
{
	if (!World)
		return;

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		FSlot& Slot = Slots[i];
		if (Slot.State == ESlotState::Free)
			continue;

		UParticleSubUVComponent* Component = Slot.Component;
		if (Slot.State == ESlotState::Active)
		{
			if (Slot.RemainingTime >= 0.0f)
			{
				Slot.RemainingTime -= DeltaTime;
				if (Slot.RemainingTime <= 0.0f)
					Component->StopEmitting();
			}

			// 루프가 아닌 이펙트는 컴포넌트가 스스로 방출을 멈춘다.
			if (!Component->IsEmitting())
				Slot.State = ESlotState::Stopping;
		}

		// 방출을 멈춰도 남은 입자가 사라질 때까지 기다려야 뚝 끊기지 않는다.
		if (Slot.State == ESlotState::Stopping && Component->IsFinished())
			ReleaseSlot(i);
	}
}

void FEffectManager::RegisterEffect(FName Name, const FEffectDesc& Desc)
{
	Descs[Name] = Desc;
}

FEffectHandle FEffectManager::SpawnEffect(FName Effect, const FVector& Location, const FRotator& Rotation, bool bLoop, float Duration)
{
	if (!World)
		return FEffectHandle();

	const FEffectDesc* Desc = Descs.Find(Effect);
	if (!Desc)
	{
		HTR_LOG(Warning, "[Effect] unknown effect: {}", Effect.ToString());
		return FEffectHandle();
	}

	const int32 Index = AcquireSlot();
	if (Index < 0)
		return FEffectHandle();

	FSlot& Slot = Slots[Index];
	UParticleSubUVComponent* Component = Slot.Component;

	// Activate가 방출 지점과 방향을 읽으므로 트랜스폼을 먼저 정한다.
	Component->SetWorldLocationAndRotation(Location, Rotation, false);
	Component->ApplyDesc(*Desc);
	Component->SetLoop(bLoop);
	Component->Activate();

	Slot.State = ESlotState::Active;
	Slot.RemainingTime = Duration > 0.0f ? Duration : -1.0f;

	return FEffectHandle{ Index, Slot.Generation };
}

void FEffectManager::StopEffect(FEffectHandle Handle, bool bImmediate)
{
	if (!FindSlot(Handle))
		return;

	if (bImmediate)
	{
		ReleaseSlot(Handle.Index);
		return;
	}

	FSlot& Slot = Slots[Handle.Index];
	Slot.Component->StopEmitting();
	Slot.State = ESlotState::Stopping;
}

bool FEffectManager::IsEffectActive(FEffectHandle Handle) const
{
	return FindSlot(Handle) != nullptr;
}

int32 FEffectManager::GetActiveCount() const
{
	int32 Count = 0;
	for (const FSlot& Slot : Slots)
	{
		if (Slot.State != ESlotState::Free)
			++Count;
	}
	return Count;
}

// 텍스처는 임시로 기존 아틀라스를 쓴다. 전용 아틀라스가 준비되면 TexturePath와 칸 수만 바꾼다.
void FEffectManager::RegisterDefaultEffects()
{
	// 팝콘을 받았을 때: 컵 위에서 작게 반짝이고 사라진다.
	FEffectDesc PopcornCatch;
	PopcornCatch.bBurst = true;
	PopcornCatch.ParticleCount = 6;
	PopcornCatch.MinLifeTime = 0.4f;
	PopcornCatch.MaxLifeTime = 0.7f;
	PopcornCatch.MinVelocity = FVector(-0.5f, -0.5f, 0.5f);
	PopcornCatch.MaxVelocity = FVector(0.5f, 0.5f, 1.5f);
	PopcornCatch.StartScale = 0.6f;
	PopcornCatch.EndScale = 0.2f;
	PopcornCatch.FadeStart = 0.5f;
	PopcornCatch.FrameRate = 24.0f;
	RegisterEffect(FName("PopcornCatch"), PopcornCatch);

	// 팝콘이 아닌 것이 바닥·캐릭터에 부딪혔을 때: 사방으로 튀고 떨어진다.
	FEffectDesc HitSplash;
	HitSplash.bBurst = true;
	HitSplash.ParticleCount = 10;
	HitSplash.MinLifeTime = 0.5f;
	HitSplash.MaxLifeTime = 0.8f;
	HitSplash.MinVelocity = FVector(-2.0f, -2.0f, 1.5f);
	HitSplash.MaxVelocity = FVector(2.0f, 2.0f, 3.0f);
	HitSplash.Acceleration = FVector(0.0f, 0.0f, -6.0f);
	HitSplash.StartScale = 0.5f;
	HitSplash.EndScale = 0.3f;
	HitSplash.FadeStart = 0.6f;
	HitSplash.FrameRate = 24.0f;
	RegisterEffect(FName("HitSplash"), HitSplash);

	// 발밑 연기: 걷는 동안 루프로 켜 두고 멈출 때 StopEffect 한다.
	FEffectDesc FootSmoke;
	FootSmoke.TexturePath = "Assets/SubUV/CloudAtlas.png";
	FootSmoke.ParticleCount = 8;
	FootSmoke.MinLifeTime = 0.6f;
	FootSmoke.MaxLifeTime = 1.0f;
	FootSmoke.MinVelocity = FVector(-0.2f, -0.2f, 0.2f);
	FootSmoke.MaxVelocity = FVector(0.2f, 0.2f, 0.5f);
	FootSmoke.StartScale = 0.4f;
	FootSmoke.EndScale = 0.9f;
	FootSmoke.FadeStart = 0.4f;
	RegisterEffect(FName("FootSmoke"), FootSmoke);
}

bool FEffectManager::AddPooledComponent()
{
	UParticleSubUVComponent* Component = Cast<UParticleSubUVComponent>(
		PoolOwner->AddComponent(UParticleSubUVComponent::StaticClass(), NAME_None));
	if (!Component)
		return false;

	// AddComponent가 바로 BeginPlay를 보내 자동으로 켜진다. 끄고, 풀 액터가 나중에 BeginPlay를
	// 받으며 컴포넌트 BeginPlay를 한 번 더 부를 때 다시 켜지지 않게 한다.
	Component->SetAutoActivate(false);
	Component->Deactivate();

	FSlot Slot;
	Slot.Component = Component;
	Slot.Generation = SessionGeneration << 16;
	Slots.Add(Slot);
	FreeSlots.Add(Slots.Num() - 1);
	return true;
}

int32 FEffectManager::AcquireSlot()
{
	if (FreeSlots.Num() == 0)
	{
		if (Slots.Num() >= MaxPoolSize || !AddPooledComponent())
		{
			HTR_LOG(Warning, "[Effect] pool exhausted ({} effects active)", Slots.Num());
			return -1;
		}
	}

	const int32 Index = FreeSlots[FreeSlots.Num() - 1];
	FreeSlots.Pop();
	return Index;
}

void FEffectManager::ReleaseSlot(int32 Index)
{
	FSlot& Slot = Slots[Index];
	Slot.Component->Deactivate();
	Slot.State = ESlotState::Free;
	Slot.RemainingTime = -1.0f;
	++Slot.Generation;
	FreeSlots.Add(Index);
}

const FEffectManager::FSlot* FEffectManager::FindSlot(FEffectHandle Handle) const
{
	if (Handle.Index < 0 || Handle.Index >= Slots.Num())
		return nullptr;

	const FSlot& Slot = Slots[Handle.Index];
	if (Slot.Generation != Handle.Generation || Slot.State == ESlotState::Free)
		return nullptr;

	return &Slot;
}
