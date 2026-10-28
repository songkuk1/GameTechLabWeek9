#include "EnginePCH.h"
#include "PlayerCameraManager.h"
#include "CameraTypes.h"
#include "GameFramework/PlayerController.h" 
#include "Engine/World.h"         
#include "Camera/CameraActor.h"   
#include "Camera/CameraComponent.h" 
void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	//FMinimalViewInfo DefaultFovCache = GetCameraChaceView();

	PCOwner = PC;

	// TODO: 언리얼은 여기서 PC를 임시 ViewTarget으로 두고, 이후
	//   - Possess 시 컨트롤러가 SetViewTarget(Pawn)
	//   - 또는 카메라 액터가 AutoActivateForPlayer로 SetViewTarget(this)
	// 로 실제 대상을 정한다. 우리 엔진엔 Pawn/AutoActivate가 없어 메인 카메라로 바로 잡는다.
	AActor* Initial = GetWorld()->GetMainCamera();
	SetViewTarget(Initial ? Initial : PC);
	UpdateCamera(0.0f);
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& TransitionParams)
{
	if (TransitionParams.BlendTime > 0.0f)
	{
		// 블렌드 도중 다시 블렌드하면, 지금 화면에 보이던 섞인 값을 새 출발점으로 고정한다
		// 이렇게 안 하면 출발점이 원래 ViewTarget으로 돌아가 화면이 튄다
		const bool bWasBlending = PendingViewTarget.Target.Get() != nullptr;
		if (bWasBlending)
		{
			ViewTarget.POV = CameraCachePOV;
			ViewTarget.Target = nullptr;
		}

		// 블렌드: 도착 대상은 Pending에 두고, 다 섞이면 UpdateCamera에서 ViewTarget으로 넘긴다
		PendingViewTarget.Target = NewTarget;
		BlendParams = TransitionParams;
		BlendParams.bLockOutgoing |= bWasBlending;
		BlendTimeToGo = TransitionParams.BlendTime;
	}
	else
	{
		// 컷: 바로 교체하고, 진행 중이던 블렌드는 취소
		ViewTarget.Target = NewTarget;
		PendingViewTarget.Target = nullptr;
		BlendTimeToGo = 0.0f;
	}
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	const bool bBlending = PendingViewTarget.Target.Get() != nullptr;

	// bLockOutgoing이면 블렌드 중엔 떠나는 쪽을 갱신하지 않아 전환 시작 순간의 POV가 남는다
	if (!bBlending || !BlendParams.bLockOutgoing)
		UpdateViewTarget(ViewTarget, DeltaTime);

	FMinimalViewInfo NewPOV = ViewTarget.POV;
	if (bBlending)
	{
		UpdateViewTarget(PendingViewTarget, DeltaTime);

		BlendTimeToGo -= DeltaTime;
		const float T = FMath::Clamp(1.0f - BlendTimeToGo / BlendParams.BlendTime, 0.0f, 1.0f);
		NewPOV.BlendViewInfo(PendingViewTarget.POV, GetBlendAlpha(T));

		// 다 섞였으면 도착 대상을 현재 대상으로 넘긴다
		if (BlendTimeToGo <= 0.0f)
		{
			ViewTarget = PendingViewTarget;
			PendingViewTarget.Target = nullptr;
			BlendTimeToGo = 0.0f;
			NewPOV = ViewTarget.POV;
		}
	}

	FillCameraCache(NewPOV);
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& OutVT, float DeltaTime)
{
	// 1. 기본값으로 리셋
	OutVT.POV = FMinimalViewInfo();
	OutVT.POV.Projection.FovDegrees = DefaultFOV;
	OutVT.POV.AspectRatio = DefaultAspectRatio;
	OutVT.POV.bConstrainAspectRatio = bDefaultConstrainAspectRatio;

	// 2. Target의 시점 계산
	AActor* Target = OutVT.Target.Get();
	if (!Target)
		return;

	//타겟이 CameraActor면 CameraComponent에서 계산하고 아니면 액터에 붙은 CameraComponent에서 계산
	if (ACameraActor* CamActor = Cast<ACameraActor>(Target))
		CamActor->GetCameraComponent()->GetCameraView(DeltaTime, OutVT.POV);
	else
		Target->CalcCamera(DeltaTime, OutVT.POV);     

	// 3. Modifier
	// ApplyCameraModifiers(DeltaTime, OutVT.POV);


}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& InCameraCachePov)
{
	CameraCachePOV = InCameraCachePov;


}

float APlayerCameraManager::GetBlendAlpha(float T) const
{
	const float Exp = BlendParams.BlendExp;
	switch (BlendParams.BlendFunction)
	{
	case EViewTargetBlendFunction::Linear:
		return T;
	case EViewTargetBlendFunction::Cubic:
		// 3t² - 2t³
		return T * T * (3.0f - 2.0f * T);
	case EViewTargetBlendFunction::EaseIn:
		return powf(T, Exp);
	case EViewTargetBlendFunction::EaseOut:
		return powf(T, 1.0f / Exp);
	case EViewTargetBlendFunction::EaseInOut:
		// 언리얼 InterpEaseInOut(0, 1, T, Exp)
		return T < 0.5f ? 0.5f * powf(2.0f * T, Exp)
		                : 1.0f - 0.5f * powf(2.0f * (1.0f - T), Exp);
	}
	return T;
}


