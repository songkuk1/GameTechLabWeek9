#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Input/InputSystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include <cassert>
#include <cmath>

UCameraComponent* FGameViewportClient::GetCamera() const
{
	const ACameraActor* Actor = World->GetMainCamera();
	return Actor ? Actor->GetCameraComponent() : nullptr;
}

// 게임 카메라는 입력과 상관없이 기본 자유 비행(WASD·우클릭·휠)을 쓰지 않는다.
// WASD는 Lua 스크립트(플레이어 등)가 쓰고, 카메라를 움직이려면 스크립트가 위치·회전을 직접 바꾼다.
void FGameViewportClient::SetInputEnabled(const bool bEnabled)
{
	if (UCameraComponent* Camera = GetCamera())
		Camera->SetExternalInputManaged(true);

	// 입력 권한을 잃으면 HUD 입력과 키 상태를 비운다. 다른 화면을 클릭하거나 누른 것이 게임에 남지 않게 한다.
	if (!bEnabled)
	{
		World->GetHUD().ClearInput();
		for (APlayerController* Controller : World->GetPlayerControllers())
			Controller->FlushPressedKeys();
	}
}

APlayerCameraManager* FGameViewportClient::GetCameraManager() const
{
    APlayerController* Controller = World->GetPlayerController(0);
    return Controller ? Controller->GetCameraPlayerManager() : nullptr;
}

bool FGameViewportClient::HasCamera() const
{
    const APlayerCameraManager* Manager = GetCameraManager();
    return (Manager && Manager->HasValidCameraCache()) || GetCamera() != nullptr;
}

FSceneView FGameViewportClient::CreateSceneView(const FVector2 OutputSize) const
{
    FMinimalViewInfo POV;
    const APlayerCameraManager* Manager = GetCameraManager();
    if (Manager && Manager->HasValidCameraCache())
        POV = Manager->GetCameraCachePOV();
    else
    {
        // Viewport setup can precede the creation of the first PlayerController.
        const UCameraComponent* Camera = GetCamera();
        assert(Camera);
        Camera->GetCameraView(0.0f, POV);
    }
    const FRect Rect{0.0f, 0.0f, OutputSize.X, OutputSize.Y};
    FSceneView::ConstrainOrthographicWidth(POV.Projection, Rect);
    return FSceneView(POV.Location, POV.GetCameraRotationMatrix(), POV.Projection,
        Rect, GetViewRenderMode());
}

void FGameViewportClient::UpdateInput(const FViewportInput& Input, FVector2 OutputSize)
{
	SetInputEnabled(Input.bEnabled);

	// 렌더할 때와 같은 화면 크기로 배율을 맞추고, 화면 기준 마우스(픽셀)를 HUD 기준 단위로 바꾼다
	FHUD& HUD = World->GetHUD();
	HUD.SetScreenSize(OutputSize);
	const float Scale = HUD.GetScale();

	FHUDInput HUDInput;
	HUDInput.bEnabled = Input.bEnabled;
	HUDInput.MousePosition = FVector2(Input.MousePosition.X / Scale, Input.MousePosition.Y / Scale);
	HUDInput.bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);
	HUDInput.bMousePressed = FInputSystem::IsMousePressed(EMouseButton::Left);
	HUD.SetInput(HUDInput);

	// 포커스를 가진 이 뷰포트의 키 상태를 자기 월드의 PlayerController에게만 넘긴다.
	// 한 키보드를 여러 플레이어가 나눠 쓰므로 모두 같은 상태를 받고, 어떤 키를 쓸지는 각자의 매핑이 거른다.
	bool bKeyDown[256];
	for (int32 Key = 0; Key < 256; ++Key)
		bKeyDown[Key] = FInputSystem::IsKeyDown(static_cast<EKeyCode>(Key));
	for (APlayerController* Controller : World->GetPlayerControllers())
		Controller->InputKeyStates(bKeyDown);
}
