#include "EnginePCH.h"
#include "MovieSceneTypes.h"

#include <algorithm>

namespace
{
	// 키 Index에서의 자동 기울기. 양 옆 키를 잇는 직선의 기울기를 쓰고 (Catmull-Rom),
	// 끝 키는 평평하게 둔다. 그래야 처음과 끝에서 부드럽게 멈춘다.
	float GetAutoTangent(const TArray<FSequenceKey>& Keys, int32 Index)
	{
		if (Index <= 0 || Index >= Keys.Num() - 1)
			return 0.0f;

		const FSequenceKey& Prev = Keys[Index - 1];
		const FSequenceKey& Next = Keys[Index + 1];
		const float Duration = Next.Time - Prev.Time;
		return Duration > 0.0f ? (Next.Value - Prev.Value) / Duration : 0.0f;
	}
}

float FSequenceChannel::Evaluate(float Time, bool bStep) const
{
	assert(HasKeys());

	if (Time <= Keys[0].Time)
		return Keys[0].Value;
	if (Time >= Keys.Last().Time)
		return Keys.Last().Value;

	// Time보다 늦은 첫 키. 위에서 범위를 걸렀으므로 1 이상, Num() 미만이다.
	const auto It = std::upper_bound(Keys.begin(), Keys.end(), Time,
		[](float T, const FSequenceKey& Key) { return T < Key.Time; });
	const int32 Index1 = static_cast<int32>(It - Keys.begin());
	const int32 Index0 = Index1 - 1;

	const FSequenceKey& Key0 = Keys[Index0];
	const FSequenceKey& Key1 = Keys[Index1];

	if (bStep || Key0.Interp == ESequenceInterp::Constant)
		return Key0.Value;

	const float Duration = Key1.Time - Key0.Time;
	if (Duration <= 0.0f)
		return Key1.Value;

	const float Alpha = (Time - Key0.Time) / Duration;
	if (Key0.Interp == ESequenceInterp::Linear)
		return Key0.Value + (Key1.Value - Key0.Value) * Alpha;

	// Cubic Hermite: 두 키의 값과 기울기로 3차 곡선을 만든다
	const float Tangent0 = GetAutoTangent(Keys, Index0) * Duration;
	const float Tangent1 = GetAutoTangent(Keys, Index1) * Duration;
	const float A2 = Alpha * Alpha;
	const float A3 = A2 * Alpha;
	return (2.0f * A3 - 3.0f * A2 + 1.0f) * Key0.Value
		+ (A3 - 2.0f * A2 + Alpha) * Tangent0
		+ (-2.0f * A3 + 3.0f * A2) * Key1.Value
		+ (A3 - A2) * Tangent1;
}

int32 FSequenceChannel::AddKey(float Time, float Value, ESequenceInterp Interp)
{
	// 같은 시간으로 보는 오차. 에디터에서 같은 프레임에 다시 키를 찍으면 덮어쓴다.
	constexpr float SameTimeTolerance = 1.0e-4f;

	int32 Index = 0;
	while (Index < Keys.Num() && Keys[Index].Time < Time - SameTimeTolerance)
		++Index;

	if (Index < Keys.Num() && std::abs(Keys[Index].Time - Time) <= SameTimeTolerance)
	{
		Keys[Index].Value = Value;
		return Index;
	}

	FSequenceKey Key;
	Key.Time = Time;
	Key.Value = Value;
	Key.Interp = Interp;
	Keys.Insert(Key, Index);
	return Index;
}

void FSequenceChannel::SortKeys()
{
	std::stable_sort(Keys.begin(), Keys.end(),
		[](const FSequenceKey& A, const FSequenceKey& B) { return A.Time < B.Time; });
}

const FSequenceSection* FSequenceTrack::FindSection(float Time) const
{
	for (int32 i = Sections.Num() - 1; i >= 0; --i)
	{
		if (Sections[i].Contains(Time))
			return &Sections[i];
	}
	return nullptr;
}

FSequenceSection& FSequenceTrack::AddSection(float StartTime, float EndTime)
{
	FSequenceSection& Section = Sections.AddDefaulted_GetRef();
	Section.StartTime = StartTime;
	Section.EndTime = EndTime;
	Section.Channels.SetNum(GetChannelCount(Type));
	return Section;
}

int32 FSequenceTrack::GetChannelCount(ESequenceTrackType Type)
{
	return Type == ESequenceTrackType::Transform ? 9 : 1;
}

const char* FSequenceTrack::GetChannelName(ESequenceTrackType Type, int32 ChannelIndex)
{
	static const char* TransformChannels[9] =
	{
		"Location.X", "Location.Y", "Location.Z",
		"Rotation.Pitch", "Rotation.Yaw", "Rotation.Roll",
		"Scale.X", "Scale.Y", "Scale.Z",
	};

	if (Type == ESequenceTrackType::Transform)
		return (ChannelIndex >= 0 && ChannelIndex < 9) ? TransformChannels[ChannelIndex] : "";
	return ChannelIndex == 0 ? "Value" : "";
}

const char* FSequenceTrack::GetTypeName(ESequenceTrackType Type)
{
	switch (Type)
	{
	case ESequenceTrackType::Transform: return "Transform";
	case ESequenceTrackType::Float:     return "Float";
	case ESequenceTrackType::Bool:      return "Bool";
	}
	return "";
}

bool FSequenceTrack::ParseTypeName(const FString& Name, ESequenceTrackType& OutType)
{
	for (ESequenceTrackType Type : { ESequenceTrackType::Transform, ESequenceTrackType::Float, ESequenceTrackType::Bool })
	{
		if (Name == GetTypeName(Type))
		{
			OutType = Type;
			return true;
		}
	}
	return false;
}

FSequenceTrack* FSequenceBinding::FindTrack(ESequenceTrackType Type, const FString& PropertyPath)
{
	for (FSequenceTrack& Track : Tracks)
	{
		if (Track.Type == Type && Track.PropertyPath == PropertyPath)
			return &Track;
	}
	return nullptr;
}

FSequenceTrack& FSequenceBinding::FindOrAddTrack(ESequenceTrackType Type, const FString& PropertyPath)
{
	if (FSequenceTrack* Found = FindTrack(Type, PropertyPath))
		return *Found;

	FSequenceTrack& Track = Tracks.AddDefaulted_GetRef();
	Track.Type = Type;
	Track.PropertyPath = PropertyPath;
	return Track;
}
