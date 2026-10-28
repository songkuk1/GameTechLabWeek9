#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Property.h"
#include "UObject/WeakObjectPtr.h"
#include "Core/Delegates/Delegate.h"
#include "Math/Transform.h"
#include "LevelSequence.h"

class UWorld;
class AActor;
class ALevelSequenceActor;
class USceneComponent;

struct FMovieSceneSequencePlaybackSettings
{
	FMovieSceneSequencePlaybackSettings()
		: bAutoPlay(false)
		, bRestoreState(true)
	{}

	/** Auto-play the sequence when created */
	uint32 bAutoPlay : 1;

	/** 끝나면 시퀀스가 바꾼 값을 원래대로 되돌리고, 시퀀스가 스폰한 액터를 지운다 (UE의 When Finished: Restore State) */
	uint32 bRestoreState : 1;

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	int32 LoopCount = 0;

	/** The rate at which to playback the animation. 음수면 거꾸로 재생한다 */
	float PlayRate = 1.0f;

	/** Start playback at the specified offset from the start of the sequence's playback range */
	float StartTime = 0.0f;
};

// 시퀀스 하나를 재생한다 (UE UMovieSceneSequencePlayer). 하는 일은 매 프레임 세 가지다.
//   ① 시간 진행  ② 지금 켜진 섹션의 채널 보간  ③ 바인딩된 액터에 값 쓰기
// 게임에서는 World가 액터 Tick 전에 Tick을 부르고, 에디터 Sequencer는 SetPlaybackPosition으로 직접 움직인다.
class ULevelSequencePlayer : public UObject
{
	DECLARE_CLASS(ULevelSequencePlayer, UObject)

public:
	ULevelSequencePlayer() = default;
	virtual ~ULevelSequencePlayer();

	/**
	 * Create a new level sequence player.
	 *
	 * @param World The world to play in.
	 * @param LevelSequence The level sequence to play. 호출한 쪽이 소유한다.
	 * @param Settings The desired playback settings
	 * @param OutActor The level sequence actor created to play this sequence.
	 */
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UWorld* World, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor);

	// World에 등록한다. Sequence는 소유하지 않는다.
	void Initialize(ULevelSequence* InSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings);

	void Play();
	// 시간과 액터 상태를 그대로 둔 채 멈춘다
	void Pause();
	// 멈추고 처음으로 돌아간다. 바꾼 값을 되돌리고 스폰한 액터를 지운다.
	void Stop();

	// 재생 여부와 상관없이 Time으로 옮겨 바로 평가한다 (에디터 스크럽, 스크립트 점프)
	void SetPlaybackPosition(float Time);

	// World가 매 프레임 부른다. 재생 중이 아니면 아무것도 하지 않는다.
	void Tick(float DeltaTime);

	bool IsPlaying() const { return bPlaying; }
	float GetCurrentTime() const { return CurrentTime; }
	float GetDuration() const { return Sequence ? Sequence->Duration : 0.0f; }
	ULevelSequence* GetSequence() const { return Sequence; }
	FMovieSceneSequencePlaybackSettings& GetSettings() { return Settings; }

	// 시퀀스가 처음 건드리기 전 값으로 모두 되돌린다
	void RestoreState();

	// 에셋이 바뀌었을 때(에디터에서 바인딩·트랙 편집) 다음 평가에서 대상을 다시 찾게 한다.
	// 스폰한 액터는 지우고, 바꾼 값은 되돌린다.
	void InvalidateBindings();

	TArray<AActor*> GetBoundActors(int32 BindingIndex) const;
	static void FindActorsByTag(UWorld* World, const FString& Tag, TArray<AActor*>& OutActors);

	// 끝까지 재생해 멈췄을 때 (Stop·Pause로는 불리지 않는다)
	TMulticastDelegate<> OnFinished;

private:
	void ResolveBindings();
	void Evaluate(float Time);
	void EvaluateTrack(AActor* Actor, const FSequenceTrack& Track, const FSequenceSection& Section, float Time);
	void DestroySpawnedActors();
	void Finish();

	// PropertyPath("Prop" 또는 "Component.Prop")가 가리키는 객체와 프로퍼티. 못 찾거나 타입이 다르면 false
	static bool FindTargetProperty(AActor* Actor, const FString& PropertyPath, EPropertyType Type, UObject*& OutObject, const FProperty*& OutProperty);

	// 처음 건드리기 직전에 원래 값을 저장해 둔다 (UE의 Pre-Animated State)
	void SavePreAnimatedTransform(USceneComponent* Component);
	void SavePreAnimatedProperty(UObject* Object, const FProperty* Property);

	struct FPreAnimatedValue
	{
		FWeakObjectPtr Object;
		const FProperty* Property = nullptr;     // nullptr면 Transform
		FTransform Transform;
		float FloatValue = 0.0f;
		bool BoolValue = false;
	};
	TArray<FPreAnimatedValue> PreAnimatedValues;

	// Sequence->Bindings와 같은 순서
	TArray<TArray<FWeakObjectPtr>> BoundActors;
	TArray<FWeakObjectPtr> SpawnedActors;
	bool bBindingsResolved = false;

	ULevelSequence* Sequence = nullptr;
	UWorld* World = nullptr;
	FMovieSceneSequencePlaybackSettings Settings;

	float CurrentTime = 0.0f;
	bool bPlaying = false;
	// 지금까지 되감은 횟수. LoopCount와 비교한다.
	int32 LoopsDone = 0;
};
