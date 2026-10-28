#pragma once

#include "UObject/Object.h"
#include "LevelSequence/MovieSceneTypes.h"
#include "UObject/Class.h"

class AActor;
class UWorld;

// 시퀀스 에셋 . 대본만 들고 있고 재생 상태는 모른다.
// 실제 액터 포인터도 들고 있지 않다. 액터는 재생할 때 ULevelSequencePlayer가 Binding으로 찾는다.
// 파일은 JSON이다 (Assets/Sequences/*.sequence).
class ULevelSequence : public UObject
{
	DECLARE_CLASS(ULevelSequence, UObject)

public:
	static constexpr const char* FileExtension = ".sequence";

	ULevelSequence() = default;
	virtual ~ULevelSequence() = default;

	// 실패하면 경고를 남기고 nullptr. 반환한 객체는 호출한 쪽이 delete 한다.
	static ULevelSequence* LoadFromFile(const FString& Path);
	bool SaveToFile(const FString& Path) const;

	void ToJson(json& Out) const;
	bool FromJson(const json& In);

	// 재생 범위는 [0, Duration]초
	float Duration = 5.0f;
	// 에디터가 키를 찍고 눈금을 그릴 때 맞추는 프레임 수. 재생에는 쓰지 않는다.
	float DisplayRate = 30.0f;

	TArray<FSequenceBinding> Bindings;

	// 마지막으로 읽거나 저장한 경로
	const FString& GetAssetPath() const { return AssetPath; }

	FSequenceBinding& AddBinding(const FString& Name);

	// Actor를 씬 파일과 같은 형식으로 적어 템플릿을 만든다 ({ "Class", "Components", "Properties" }).
	static json CaptureTemplate(AActor* Actor);
	// 템플릿으로 액터를 스폰한다. 씬에 저장되지 않도록 RF_Transient를 붙인다.
	static AActor* SpawnFromTemplate(UWorld* World, const json& Template);

private:
	mutable FString AssetPath;
};
