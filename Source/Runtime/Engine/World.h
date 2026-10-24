#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MovementComponent.h"
#include "Component/TextRenderComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Engine/Scene.h"
#include "Camera/CameraActor.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Engine/EngineTypes.h"

//class ACameraActor;
class ULevel;
class UBillboardComponent;
class UShapeComponent;
class FLuaScriptInsatance;

struct FLODViewContext;

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	void Reset() { *this = FRenderStats(); }
};

class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld() = default;
	virtual ~UWorld();

	bool Init();
	/*UPrimitiveComponent* SpawnPrimitive(FClass* Class);*/
	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	void Tick(float DeltaTime);

	void ClearWorld();

	void GatherRenderPackets(FRenderQueue& RenderArray, const FLODViewContext* LODView = nullptr, const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);

	void CreateMainCamera();

	// 카메라 Get/Set
	void SetMainCamera(ACameraActor* Camera);
	ACameraActor* GetMainCamera() const { return MainCamera; }

	// Level
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }

	int32 GetActorNum();

	bool DestroyActor(AActor* Actor);

	// 파괴 예정으로 표시. Frame 끝에서 delete
	void DeferDelete(UObject* Object);
	// 실제로 delete. Tick 끝과 ClearWorld 에서 호출
	void FlushPendingKill();

	// Coroutine을 World가 매 Frame 진행하도록 등록 / 해제 ( BeginPlay, EndPlay에서 호출 )
	void RegisterScriptInstance(FLuaScriptInstance* Instance);
	void UnregisterScriptInstance(FLuaScriptInstance* Instance);

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix(*)(const UBillboardComponent&, const void*);
	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
		FBillboardTraceTransform ResolveBillboard = nullptr, const void* ViewContext = nullptr);

	void BeginPlay();
	void EndPlay();

	// 움직인 컴포넌트를 표시해두면 Tick 끝에서 한 번 UpdateOverlaps를 불러준다. 에디터 월드에서는 무시한다.
	void MarkOverlapDirty(UPrimitiveComponent* Component);
	// 사라질 컴포넌트를 대기 목록에서 뺀다.
	void CancelOverlapUpdate(UPrimitiveComponent* Component);
	// Shape와 모양이 겹치는 이 월드의 Shape를 모두 모은다. 이벤트 규칙은 호출하는 쪽이 거른다 (UE의 ComponentOverlapMulti).
	void OverlapMulti(UShapeComponent* Shape, TArray<UPrimitiveComponent*>& OutOverlaps);
	// 넓은 단계. 월드 AABB가 Bounds와 겹치는 이 월드의 Shape를 모은다 (Ignore 제외). Overlap과 Sweep이 같이 쓴다.
	void GatherShapesInBounds(const FBox& Bounds, const UShapeComponent* Ignore, TArray<UShapeComponent*>& OutShapes);

	FScene& GetScene() { return Scene; }
	FTickTaskManager& GetTickTaskManager() { return TickTaskManager; }

	const FRenderStats& GetRenderStats() const { return RenderStats; }

	void SetWorldType(EWorldType InType) { WorldType = InType; }
	EWorldType GetWorldType() const { return WorldType; }

	static UWorld* CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None); /*, UPackage* InWorldPackage = NULL, bool bAddToRoot = true, ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num, const InitializationValues* InIVS = nullptr, bool bInSkipInitWorld = false);*/

	bool IsPlayInEditor() const { return WorldType == EWorldType::PIE; }

	static UWorld* GetDuplicatedWorldForPIE(UWorld* InWorld);

	virtual void PostDuplicate(bool bDuplicateForPIE) override;


	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
private:
	EWorldType WorldType = EWorldType::None;

	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	TArray<FGatherChunk> GatherChunks;

	// 등록된 Tick 함수만 실행한다. Actor보다 먼저 사라져도 남은 함수와의 연결을 스스로 끊는다.
	FTickTaskManager TickTaskManager;

	TQueue<AActor*> BeginPlayList;

	void UpdatePendingOverlaps();
	TArray<UPrimitiveComponent*> PendingOverlapUpdates;
	TArray<UPrimitiveComponent*> OverlapUpdatesInProgress;

	//메인 카메라 
	ACameraActor* MainCamera = nullptr;

	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;

	FScene Scene;

	// GatherRenderPackets가 매 프레임 채우는 컬링 결과. 용량을 재사용한다.
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;

	FRenderStats RenderStats;

	// Destroy 되었지만 아직 delete 하지 않은 객체 모아두는 배열 ( Frame 끝에서 지울 수 있도록 )
	TArray<UObject*> PendingKillObjects;

	void TickScriptCoroutines(float DeltaTime);

	// 비소유. Instance 는 Actor/Component 가 소유
	TArray<FLuaScriptInstance*> ScriptInstances;
	bool bTickingScripts = false;
};
