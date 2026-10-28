#include "EnginePCH.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Component/SceneComponent.h"
#include "UObject/Class.h"
#include "UObject/Property.h"

#include <algorithm>
#include <cmath>

ULevelSequencePlayer::~ULevelSequencePlayer()
{
	// 여기서는 액터를 Destroy 하지 않는다. 월드를 비울 때(ClearActors) 액터 배열을 도는 중에 불릴 수 있어서다.
	// 정리가 필요하면 지우기 전에 Stop을 부른다 (ALevelSequenceActor는 EndPlay에서 부른다).
	// 남은 스폰 액터는 RF_Transient라 씬에 저장되지 않고, 월드와 함께 지워진다.
	if (World)
		World->UnregisterSequencePlayer(this);
}

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UWorld* World, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor)
{
	OutActor = nullptr;
	if (!World || !LevelSequence)
		return nullptr;

	ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>();
	if (!Actor)
		return nullptr;

	// 실행 중에 만든 연출용 액터이므로 씬에 저장하지 않는다
	Actor->SetFlags(EObjectFlags::RF_Transient);
	Actor->SetPlaybackSettings(Settings);
	Actor->SetSequence(LevelSequence);

	OutActor = Actor;
	return Actor->GetSequencePlayer();
}

void ULevelSequencePlayer::Initialize(ULevelSequence* InSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	if (World != InWorld)
	{
		if (World)
			World->UnregisterSequencePlayer(this);
		if (InWorld)
			InWorld->RegisterSequencePlayer(this);
	}

	InvalidateBindings();

	Sequence = InSequence;
	World = InWorld;
	Settings = InSettings;
	CurrentTime = Settings.StartTime;
	LoopsDone = 0;
	bPlaying = false;
}

void ULevelSequencePlayer::Play()
{
	if (!Sequence || !World)
		return;

	// 끝에서 다시 누르면 처음부터 재생한다
	const float Duration = GetDuration();
	if (Settings.PlayRate >= 0.0f && CurrentTime >= Duration)
		CurrentTime = 0.0f;
	else if (Settings.PlayRate < 0.0f && CurrentTime <= 0.0f)
		CurrentTime = Duration;

	bPlaying = true;
	LoopsDone = 0;

	// 다음 Tick까지 기다리면 첫 프레임에 원래 자세가 한 번 보인다
	Evaluate(CurrentTime);
}

void ULevelSequencePlayer::Pause()
{
	bPlaying = false;
}

void ULevelSequencePlayer::Stop()
{
	bPlaying = false;
	CurrentTime = Settings.StartTime;
	LoopsDone = 0;
	InvalidateBindings();
}

void ULevelSequencePlayer::SetPlaybackPosition(float Time)
{
	if (!Sequence)
		return;

	CurrentTime = std::clamp(Time, 0.0f, GetDuration());
	Evaluate(CurrentTime);
}

void ULevelSequencePlayer::Tick(float DeltaTime)
{
	if (!bPlaying || !Sequence)
		return;

	const float Duration = GetDuration();
	if (Duration <= 0.0f)
	{
		Finish();
		return;
	}

	CurrentTime += DeltaTime * Settings.PlayRate;

	// 끝을 넘었으면 남은 반복이 있을 때 넘친 만큼 반대쪽에서 이어 가고, 없으면 끝에서 멈춘다
	const bool bPassedEnd = Settings.PlayRate >= 0.0f ? CurrentTime >= Duration : CurrentTime <= 0.0f;
	if (bPassedEnd)
	{
		const bool bCanLoop = Settings.LoopCount < 0 || LoopsDone < Settings.LoopCount;
		if (!bCanLoop)
		{
			CurrentTime = Settings.PlayRate >= 0.0f ? Duration : 0.0f;
			Evaluate(CurrentTime);
			Finish();
			return;
		}

		++LoopsDone;
		CurrentTime = std::fmod(CurrentTime, Duration);
		if (CurrentTime < 0.0f)
			CurrentTime += Duration;
	}

	Evaluate(CurrentTime);
}

void ULevelSequencePlayer::Finish()
{
	bPlaying = false;

	if (Settings.bRestoreState)
		InvalidateBindings();

	OnFinished.Broadcast();
}

void ULevelSequencePlayer::RestoreState()
{
	// 거꾸로 되돌려야 같은 대상을 여러 번 저장했을 때도 가장 처음 값이 남는다
	for (int32 i = PreAnimatedValues.Num() - 1; i >= 0; --i)
	{
		const FPreAnimatedValue& Saved = PreAnimatedValues[i];
		UObject* Object = Saved.Object.Get();
		if (!Object)
			continue;

		if (!Saved.Property)
		{
			if (USceneComponent* Component = Cast<USceneComponent>(Object))
				Component->SetTransform(Saved.Transform);
			continue;
		}

		void* Ptr = reinterpret_cast<uint8*>(Object) + Saved.Property->Offset;
		if (Saved.Property->Type == EPropertyType::Float)
			*static_cast<float*>(Ptr) = Saved.FloatValue;
		else
			*static_cast<bool*>(Ptr) = Saved.BoolValue;
		Object->PostEditChangeProperty(*Saved.Property);
	}

	PreAnimatedValues.Reset();
}

void ULevelSequencePlayer::InvalidateBindings()
{
	RestoreState();
	DestroySpawnedActors();
	BoundActors.Reset();
	bBindingsResolved = false;
}

TArray<AActor*> ULevelSequencePlayer::GetBoundActors(int32 BindingIndex) const
{
	TArray<AActor*> Result;
	if (BindingIndex < 0 || BindingIndex >= BoundActors.Num())
		return Result;

	for (const FWeakObjectPtr& Weak : BoundActors[BindingIndex])
	{
		if (AActor* Actor = Cast<AActor>(Weak.Get()))
			Result.Add(Actor);
	}
	return Result;
}

void ULevelSequencePlayer::FindActorsByTag(UWorld* World, const FString& Tag, TArray<AActor*>& OutActors)
{
	if (!World || !World->GetPersistentLevel() || Tag.empty())
		return;

	for (AActor* Actor : World->GetPersistentLevel()->GetActors())
	{
		if (::IsValid(Actor) && Actor->BindingTag == Tag)
			OutActors.Add(Actor);
	}
}

void ULevelSequencePlayer::ResolveBindings()
{
	BoundActors.Reset();
	BoundActors.SetNum(Sequence->Bindings.Num());

	for (int32 i = 0; i < Sequence->Bindings.Num(); ++i)
	{
		const FSequenceBinding& Binding = Sequence->Bindings[i];

		// 1) 씬에 있으면 빌려 쓴다 (Possessable)
		TArray<AActor*> Found;
		FindActorsByTag(World, Binding.Tag, Found);
		if (Found.Num() > 1)
			HTR_LOG(Warning, "LevelSequence: tag '{}' matches {} actors, all of them are animated", Binding.Tag, Found.Num());

		// 2) 없으면 템플릿으로 스폰한다 (Spawnable)
		if (Found.IsEmpty() && Binding.HasTemplate())
		{
			if (AActor* Spawned = ULevelSequence::SpawnFromTemplate(World, Binding.Template))
			{
				Found.Add(Spawned);
				SpawnedActors.Add(FWeakObjectPtr(Spawned));
			}
		}

		// 3) 둘 다 없으면 이 바인딩만 건너뛴다
		if (Found.IsEmpty())
			HTR_LOG(Warning, "LevelSequence: binding '{}' (tag '{}') is unresolved", Binding.Name, Binding.Tag);

		for (AActor* Actor : Found)
			BoundActors[i].Add(FWeakObjectPtr(Actor));
	}

	bBindingsResolved = true;
}

void ULevelSequencePlayer::DestroySpawnedActors()
{
	for (const FWeakObjectPtr& Weak : SpawnedActors)
	{
		if (AActor* Actor = Cast<AActor>(Weak.Get()))
			Actor->Destroy();
	}
	SpawnedActors.Reset();
}

void ULevelSequencePlayer::Evaluate(float Time)
{
	if (!Sequence || !World)
		return;

	if (!bBindingsResolved)
		ResolveBindings();

	for (int32 i = 0; i < Sequence->Bindings.Num() && i < BoundActors.Num(); ++i)
	{
		for (const FWeakObjectPtr& Weak : BoundActors[i])
		{
			AActor* Actor = Cast<AActor>(Weak.Get());
			if (!Actor)
				continue;

			for (const FSequenceTrack& Track : Sequence->Bindings[i].Tracks)
			{
				// 켜진 섹션이 없으면 값을 쓰지 않는다. 마지막으로 쓴 값이 그대로 남는다.
				if (const FSequenceSection* Section = Track.FindSection(Time))
					EvaluateTrack(Actor, Track, *Section, Time);
			}
		}
	}
}

void ULevelSequencePlayer::EvaluateTrack(AActor* Actor, const FSequenceTrack& Track, const FSequenceSection& Section, float Time)
{
	switch (Track.Type)
	{
	case ESequenceTrackType::Transform:
	{
		USceneComponent* Root = Actor->GetRootComponent();
		if (!Root || Section.Channels.Num() < 9)
			return;

		SavePreAnimatedTransform(Root);

		// 키가 없는 채널은 지금 값을 그대로 둔다 (X만 키를 찍었으면 Y·Z는 손대지 않는다)
		FTransform Transform = Root->GetTransform();
		float* Targets[9] =
		{
			&Transform.Location.X, &Transform.Location.Y, &Transform.Location.Z,
			&Transform.Rotation.Pitch, &Transform.Rotation.Yaw, &Transform.Rotation.Roll,
			&Transform.Scale.X, &Transform.Scale.Y, &Transform.Scale.Z,
		};
		for (int32 c = 0; c < 9; ++c)
		{
			if (Section.Channels[c].HasKeys())
				*Targets[c] = Section.Channels[c].Evaluate(Time);
		}
		Root->SetTransform(Transform);
		break;
	}

	case ESequenceTrackType::Float:
	case ESequenceTrackType::Bool:
	{
		if (Section.Channels.IsEmpty() || !Section.Channels[0].HasKeys())
			return;

		const bool bIsBool = Track.Type == ESequenceTrackType::Bool;
		UObject* Target = nullptr;
		const FProperty* Property = nullptr;
		if (!FindTargetProperty(Actor, Track.PropertyPath, bIsBool ? EPropertyType::Bool : EPropertyType::Float, Target, Property))
			return;

		SavePreAnimatedProperty(Target, Property);

		void* Ptr = reinterpret_cast<uint8*>(Target) + Property->Offset;
		if (bIsBool)
			*static_cast<bool*>(Ptr) = Section.Channels[0].Evaluate(Time, true) >= 0.5f;
		else
			*static_cast<float*>(Ptr) = Section.Channels[0].Evaluate(Time);

		// 디테일 패널에서 값을 고쳤을 때와 같은 후처리 (렌더 상태 갱신 등)
		Target->PostEditChangeProperty(*Property);
		break;
	}
	}
}

bool ULevelSequencePlayer::FindTargetProperty(AActor* Actor, const FString& PropertyPath, EPropertyType Type, UObject*& OutObject, const FProperty*& OutProperty)
{
	OutObject = Actor;
	FString PropertyName = PropertyPath;

	const size_t Dot = PropertyPath.find('.');
	if (Dot != FString::npos)
	{
		OutObject = Actor->FindComponentByName(FName(PropertyPath.substr(0, Dot)));
		PropertyName = PropertyPath.substr(Dot + 1);
	}

	if (!OutObject)
		return false;

	for (UClass* Class = OutObject->GetClass(); Class; Class = Class->Super)
	{
		for (const FProperty& Property : Class->GetProperties())
		{
			if (Property.Name == PropertyName && Property.Type == Type)
			{
				OutProperty = &Property;
				return true;
			}
		}
	}
	return false;
}

void ULevelSequencePlayer::SavePreAnimatedTransform(USceneComponent* Component)
{
	for (const FPreAnimatedValue& Saved : PreAnimatedValues)
	{
		if (!Saved.Property && Saved.Object.Get() == Component)
			return;
	}

	FPreAnimatedValue& Saved = PreAnimatedValues.AddDefaulted_GetRef();
	Saved.Object = FWeakObjectPtr(Component);
	Saved.Transform = Component->GetTransform();
}

void ULevelSequencePlayer::SavePreAnimatedProperty(UObject* Object, const FProperty* Property)
{
	for (const FPreAnimatedValue& Saved : PreAnimatedValues)
	{
		if (Saved.Property == Property && Saved.Object.Get() == Object)
			return;
	}

	FPreAnimatedValue& Saved = PreAnimatedValues.AddDefaulted_GetRef();
	Saved.Object = FWeakObjectPtr(Object);
	Saved.Property = Property;

	const void* Ptr = reinterpret_cast<const uint8*>(Object) + Property->Offset;
	if (Property->Type == EPropertyType::Float)
		Saved.FloatValue = *static_cast<const float*>(Ptr);
	else
		Saved.BoolValue = *static_cast<const bool*>(Ptr);
}
