#pragma once
#include "Math/Color.h"
#include "Math/Vector2.h"
#include "Core/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "GameFramework/Actor.h"
#include "CameraTypes.h"
struct FViewTarget
{
public:
	FViewTarget() = default;
	~FViewTarget() = default;

	FViewTarget(const FViewTarget&) = delete;
	FViewTarget& operator=(FViewTarget&) = delete;

	TWeakObjectPtr<AActor> Target;
	
	FMinimalViewInfo POV;


};

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)

public:
	APlayerCameraManager() = default;
	~APlayerCameraManager() = default;

	APlayerCameraManager(const APlayerCameraManager&) = delete;
	APlayerCameraManager& operator=(APlayerCameraManager&) = delete;

	void InitializeFor(APlayerController* PC);

	void SetViewTarget(AActor* InActor);
	void UpdateCamera(float deltaTime);
	
private:
	void UpdateViewTarget(FViewTarget& VT, float DeltaTime);


private:
	APlayerController* PCOwner = nullptr;

	//현재 카메라 시점을 제공하는 대상과,그 대상으로부터 이번 프레임에 계산한 POV
	FViewTarget ViewTarget;

	//블렌드 전환 중 일 때 도착할 대상과, 그 대상으로부터 이번 프레임에 계산한 POV
	FViewTarget PendingViewTarget;

	//화면을 덮을 색
	FLinearColor FadeColor;

	//현재 프레임의 페이드 정도
	float FadeAmount;

	//시작값,끝값
	FVector2 FadeAlpha;

	//페이드에 걸리는 전체 시간
	float FadeTime;

	//카메라 모드 이름 UpdateViewTarget에서 POV를 어떻게 계산할 것인지 분기
	///Default,FreeCam,ThirdPerson 등이 존재함
	FName CameraStyle;

	//아무도 정하지 않았을 때 쓰는 값
	float DefaultFOV = 90.0f;
	float DefaultAspectRatio = 1.333f;
	bool bDefaultConstrainAspectRatio = false;

};