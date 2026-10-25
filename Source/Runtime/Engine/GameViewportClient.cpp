#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/HUD.h"
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

void FGameViewportClient::SetInputEnabled(const bool bEnabled)
{
	if (UCameraComponent* Camera = GetCamera())
		Camera->SetExternalInputManaged(!bEnabled);

	// 입력 권한을 잃으면 HUD 입력도 비운다. 다른 화면을 클릭한 것이 버튼에 남지 않게 한다.
	if (!bEnabled)
		World->GetHUD().ClearInput();
}

FSceneView FGameViewportClient::CreateSceneView(const FVector2 OutputSize) const
{
	const UCameraComponent* Camera = GetCamera();
	assert(Camera);
	// PIE는 기존처럼 Roll을 제거한다. 카메라를 변경하지 않고 Forward에서 수평 기저만 만든다.
	// GetSafeNormal은 기존 inverse-length 곱셈 순서도 보존해 극점 포화 경계가 흔들리지 않는다.
	FVector Forward = Camera->GetWorldRotation().Quaternion().GetForwardVector().GetSafeNormal();
	FVector Right = FVector(-Forward.Y, Forward.X, 0.0f).Normalized();
	if (Right.SizeSquared() == 0.0f)
		Right = RightVector; // 수직 시선에서 기존 atan2(0,0)의 Yaw=0 정책.
	// 기존 asin(clamp(Z)) 경로는 float Z가 ±1로 반올림되면 정확한 수직 시선이 된다.
	// 이 포화 정책을 보존해야 큰 직교 후퇴 거리에서 수평 위치가 달라지지 않는다.
	const float Z = FMath::Clamp(Forward.Z, -1.0f, 1.0f);
	const float Horizontal = std::sqrt(std::max(0.0f, 1.0f - Z * Z));
	Forward = FVector(Right.Y * Horizontal, -Right.X * Horizontal, Z);
	FMatrix Rotation = FMatrix::Identity;
	Rotation.SetAxis(0, Forward);
	Rotation.SetAxis(1, Right);
	Rotation.SetAxis(2, Forward.Cross(Right).Normalized());
	FCameraProjection Projection = Camera->GetProjectionSettings();
	FSceneView::ConstrainOrthographicWidth(Projection, FRect{0.0f, 0.0f, OutputSize.X, OutputSize.Y});
	return FSceneView(Camera->GetWorldLocation(), Rotation, Projection, FRect{0.0f, 0.0f, OutputSize.X, OutputSize.Y}, GetViewRenderMode());
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
}