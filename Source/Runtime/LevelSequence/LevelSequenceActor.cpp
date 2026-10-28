#include "EnginePCH.h"
#include "LevelSequenceActor.h"

ALevelSequenceActor::~ALevelSequenceActor()
{
	delete SequencePlayer;
	SequencePlayer = nullptr;

	if (bOwnsSequence)
		delete Sequence;
	Sequence = nullptr;
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Sequence && !SequencePath.empty())
	{
		Sequence = ULevelSequence::LoadFromFile(SequencePath);
		bOwnsSequence = Sequence != nullptr;
	}

	if (!Sequence)
	{
		if (!SequencePath.empty())
			HTR_LOG(Warning, "LevelSequenceActor '{}': failed to load '{}'", GetName(), SequencePath);
		return;
	}

	// CreateLevelSequencePlayer로 만들었으면 SetSequence가 이미 초기화했다. 그 사이 Play를 불렀을 수 있으니 다시 하지 않는다.
	if (!SequencePlayer)
		InitializePlayer();
	if (bAutoPlay && !SequencePlayer->IsPlaying())
		Play();
}

void ALevelSequenceActor::EndPlay()
{
	// 바꾼 값을 되돌리고 스폰한 액터를 지운다. 소멸자에서는 액터를 지울 수 없으므로 여기서 한다.
	if (SequencePlayer)
		SequencePlayer->Stop();

	Super::EndPlay();
}

FMovieSceneSequencePlaybackSettings ALevelSequenceActor::GetPlaybackSettings() const
{
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bAutoPlay = bAutoPlay;
	Settings.bRestoreState = bRestoreState;
	Settings.LoopCount = LoopCount;
	Settings.PlayRate = PlayRate;
	Settings.StartTime = StartTime;
	return Settings;
}

void ALevelSequenceActor::SetPlaybackSettings(const FMovieSceneSequencePlaybackSettings& Settings)
{
	bAutoPlay = Settings.bAutoPlay;
	bRestoreState = Settings.bRestoreState;
	LoopCount = Settings.LoopCount;
	PlayRate = Settings.PlayRate;
	StartTime = Settings.StartTime;

	if (SequencePlayer)
		SequencePlayer->GetSettings() = Settings;
}

void ALevelSequenceActor::SetSequence(ULevelSequence* InSequence)
{
	if (Sequence == InSequence)
		return;

	if (bOwnsSequence)
		delete Sequence;
	Sequence = InSequence;
	bOwnsSequence = false;

	InitializePlayer();
}

void ALevelSequenceActor::InitializePlayer()
{
	if (!SequencePlayer)
		SequencePlayer = NewObject<ULevelSequencePlayer>(this);

	SequencePlayer->Initialize(Sequence, GetWorld(), GetPlaybackSettings());
}

void ALevelSequenceActor::Play()
{
	if (SequencePlayer)
		SequencePlayer->Play();
}

void ALevelSequenceActor::Pause()
{
	if (SequencePlayer)
		SequencePlayer->Pause();
}

void ALevelSequenceActor::Stop()
{
	if (SequencePlayer)
		SequencePlayer->Stop();
}

bool ALevelSequenceActor::IsPlaying() const
{
	return SequencePlayer && SequencePlayer->IsPlaying();
}

void ALevelSequenceActor::SetPlaybackPosition(float Time)
{
	if (SequencePlayer)
		SequencePlayer->SetPlaybackPosition(Time);
}
