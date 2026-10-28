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
}

void APlayerCameraManager::SetViewTarget(AActor* InActor)
{
	ViewTarget.Target = InActor;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	UpdateViewTarget(ViewTarget, DeltaTime);

	
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& VT, float DeltaTime)
{


}
