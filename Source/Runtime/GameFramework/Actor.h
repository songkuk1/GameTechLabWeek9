#pragma once

#include "UObject/Object.h"
#include "Component/PrimitiveComponent.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "Containers/Set.h"
#include "Engine/EngineBaseTypes.h"

class UWorld;
class ULevel;
class FLuaScriptInstance;

class AActor : public UObject
{
	DECLARE_CLASS(AActor, UObject)
	REFLECT_START(ClassName)
		PROPERTY_FLAGS(ScriptPath, CPF_EditAnywhere | CPF_LuaScriptPath)
		FUNCTION(GetRootComponent)
		FUNCTION(FindComponentByName)
		FUNCTION(GetActorLocation)
		FUNCTION(GetActorRotation)
		FUNCTION(GetActorScale3D)
		FUNCTION(GetActorTransform)
		FUNCTION(HasActorBegunPlay)
		FUNCTION(Destroy)
		FUNCTION(DestroyComponent)
		REFLECT_END()
public:
	AActor();
	virtual ~AActor();

	virtual void BeginPlay(); // xx World->AddPrimitive 책임이동 필요
	virtual void Tick(float DeltaTime) {}
	virtual void EndPlay();
	void TickActor(float DeltaTime);

	// World가 호출하는 진입점. BeginPlay를 받은 액터만 EndPlay를 받도록 짝을 맞춘다.
	void DispatchBeginPlay();
	void DispatchEndPlay();
	bool HasActorBegunPlay() const { return bActorHasBegunPlay; }

	// 비어 있으면 스크립트 없음. 씬 저장과 PIE 복제는 리플렉션(PROPERTY)이 처리한다.
	FString ScriptPath;
	FLuaScriptInstance* GetScriptInstance() const { return ScriptInstance.get(); }

	UWorld* GetWorld() const { return World; }
	ULevel* GetLevel() const { return Level; }

	const TArray<UActorComponent*>& GetComponents() const { return Components; }
	USceneComponent* GetRootComponent() const { return RootComponent; }
	void SetRootComponent(USceneComponent* SceneComponent) { RootComponent = SceneComponent; }

	void RemoveOwnedComponent(UActorComponent* Component);
	
	FVector GetActorLocation() const;
	FRotator GetActorRotation() const;
	FVector GetActorScale3D() const;
	FQuat GetActorQuat() const;           //xx 타입명변경
	FTransform GetActorTransform() const;
	bool TryGetActorBounds(FBox& OutBounds) const;

	bool Destroy();
	
	//xx삭제 예정
	friend class UWorld;

	UActorComponent* AddComponent(UClass* ComponentClass, FName Name);
	void DestroyComponent(UActorComponent* Component);

	// bCanEverTick이 켜진 액터·컴포넌트의 Tick 함수만 World의 FTickTaskManager에 등록하거나 해제한다.
	void RegisterAllActorTickFunctions(bool bRegister);

	// UE와 같이 기본값은 bCanEverTick = false. 생성자에서 Target = this
	FActorTickFunction PrimaryActorTick;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	UActorComponent* FindComponentByName(FName Name) const;
protected:
	virtual void OnDefaultSubobjectCreated(UObject* Subobject) override;

	//TSet<TObjectPtr<UActorComponent>> OwnedComponents;
	TArray<UActorComponent*> Components;
	USceneComponent* RootComponent = nullptr;

	UWorld* World = nullptr;
	ULevel* Level = nullptr;
private:
	bool bActorHasBegunPlay = false;

	// 플레이 중에만 존재한다. DispatchBeginPlay에서 만들고 DispatchEndPlay에서 지운다.
	TUniquePtr<FLuaScriptInstance> ScriptInstance;


};