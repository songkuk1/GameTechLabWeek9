#pragma once

#include "GameFramework/Actor.h"
#include "LevelSequencePlayer.h"

// 레벨에 놓는 시퀀스 재생기 (UE ALevelSequenceActor). SequencePath의 에셋을 읽어 플레이어에 넘긴다.
// 플레이어는 BeginPlay 이후(게임·PIE)에만 만든다. 에디터 미리보기는 Sequencer가 따로 플레이어를 만든다.
// Lua에서는 Play / Pause / Stop / IsPlaying / SetPlaybackPosition 으로 제어한다.
class ALevelSequenceActor : public AActor
{
	DECLARE_CLASS(ALevelSequenceActor, AActor)
	REFLECT_START(ClassName)
		PROPERTY_FLAGS(SequencePath, CPF_EditAnywhere)
		PROPERTY_FLAGS(bAutoPlay, CPF_EditAnywhere)
		PROPERTY_FLAGS(bRestoreState, CPF_EditAnywhere)
		PROPERTY_FLAGS(LoopCount, CPF_EditAnywhere)
		PROPERTY_FLAGS(PlayRate, CPF_EditAnywhere)
		PROPERTY_FLAGS(StartTime, CPF_EditAnywhere)
		FUNCTION(Play)
		FUNCTION(Pause)
		FUNCTION(Stop)
		FUNCTION(IsPlaying)
		FUNCTION(SetPlaybackPosition)
	REFLECT_END()

public:
	ALevelSequenceActor() = default;
	virtual ~ALevelSequenceActor();

	virtual void BeginPlay() override;
	virtual void EndPlay() override;

	// 작업 디렉터리 기준 경로 (예: Assets/Sequences/Intro.sequence)
	FString SequencePath;

	// FMovieSceneSequencePlaybackSettings를 디테일 패널에 펼친 것. 비트필드는 리플렉션할 수 없어 따로 둔다.
	bool bAutoPlay = false;
	bool bRestoreState = true;
	int32 LoopCount = 0;
	float PlayRate = 1.0f;
	float StartTime = 0.0f;

	FMovieSceneSequencePlaybackSettings GetPlaybackSettings() const;
	void SetPlaybackSettings(const FMovieSceneSequencePlaybackSettings& Settings);

	// 이미 읽은 에셋을 쓴다 (소유하지 않는다). SequencePath보다 우선한다.
	void SetSequence(ULevelSequence* InSequence);
	ULevelSequence* GetSequence() const { return Sequence; }
	ULevelSequencePlayer* GetSequencePlayer() const { return SequencePlayer; }

	void Play();
	void Pause();
	void Stop();
	bool IsPlaying() const;
	void SetPlaybackPosition(float Time);

private:
	// 플레이어가 없으면 만들고, 지금 에셋과 설정으로 다시 초기화한다
	void InitializePlayer();

	ULevelSequence* Sequence = nullptr;
	// SequencePath로 직접 읽은 에셋이면 소멸할 때 지운다
	bool bOwnsSequence = false;

	ULevelSequencePlayer* SequencePlayer = nullptr;
};
