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
