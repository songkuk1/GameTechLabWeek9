#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

// 시퀀스 에셋을 이루는 데이터. 대본 한 권을 다음 순서로 쪼갠다.
//   Binding(누구를) → Track(무엇을) → Section(언제) → Channel(값이 어떻게 변하나) → Key
// UE는 시간을 정수 틱(FFrameNumber)으로 저장하지만, 여기서는 단순하게 초(float)로 저장한다.

// 키와 다음 키 사이를 채우는 방식 (UE ERichCurveInterpMode)
enum class ESequenceInterp : uint8
{
	Constant,
	Linear,
	Cubic,
};

struct FSequenceKey
{
	float Time = 0.0f;
	float Value = 0.0f;
	ESequenceInterp Interp = ESequenceInterp::Cubic;
};


struct FSequenceChannel
{
	TArray<FSequenceKey> Keys;

	bool HasKeys() const { return !Keys.IsEmpty(); }
	float Evaluate(float Time, bool bStep = false) const;
	int32 AddKey(float Time, float Value, ESequenceInterp Interp = ESequenceInterp::Cubic);
	void SortKeys();
};

enum class ESequenceTrackType : uint8
{
	Transform,
	Float,
	Bool,
};

// 타임라인 위의 막대 하나. [StartTime, EndTime] 안에서만 값을 쓴다.
struct FSequenceSection
{
	float StartTime = 0.0f;
	float EndTime = 0.0f;
	// 트랙 종류가 정한 개수만큼 있다. 인덱스의 뜻은 FSequenceTrack::GetChannelName
	TArray<FSequenceChannel> Channels;

	bool Contains(float Time) const { return StartTime <= Time && Time <= EndTime; }
};

struct FSequenceTrack
{
	ESequenceTrackType Type = ESequenceTrackType::Transform;

	// Float·Bool 트랙이 쓰는 대상 프로퍼티. "Prop"이면 액터의 프로퍼티,
	// "ComponentName.Prop"이면 그 이름의 컴포넌트 프로퍼티다. Transform 트랙은 비워 둔다.
	FString PropertyPath;

	TArray<FSequenceSection> Sections;

	// 섹션이 겹치면 뒤에 있는 섹션이 이긴다 (UE는 블렌딩하지만 여기서는 하지 않는다)
	const FSequenceSection* FindSection(float Time) const;

	// 채널 수만큼 비어 있는 섹션을 붙이고 반환한다
	FSequenceSection& AddSection(float StartTime, float EndTime);

	static int32 GetChannelCount(ESequenceTrackType Type);
	static const char* GetChannelName(ESequenceTrackType Type, int32 ChannelIndex);
	static const char* GetTypeName(ESequenceTrackType Type);
	static bool ParseTypeName(const FString& Name, ESequenceTrackType& OutType);
};

// 시퀀스가 움직일 대상 하나
// 재생할 때 Tag로 씬에서 먼저 찾고(Possessable), 없으면 Template으로 직접 스폰한다(Spawnable).
struct FSequenceBinding
{
	FString Name;
	FString Tag;

	// null이 아니면 씬에 대상이 없을 때 이 내용으로 스폰한다.
	// 씬 파일의 액터 항목과 같은 형식이다: { "Class", "Components", "Properties" }
	json Template;

	TArray<FSequenceTrack> Tracks;
	bool HasTemplate() const { return Template.is_object(); }

	FSequenceTrack* FindTrack(ESequenceTrackType Type, const FString& PropertyPath);
	FSequenceTrack& FindOrAddTrack(ESequenceTrackType Type, const FString& PropertyPath = "");
};
