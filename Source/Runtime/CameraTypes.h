#pragma once

#include "Camera/CameraProjection.h"
#include "Math/Rotator.h"

// A copyable view value. Modifiers edit a fresh base view every frame.
struct FMinimalViewInfo
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation{};
	FCameraProjection Projection{};
	float AspectRatio = 16.0f / 9.0f;
	bool bConstrainAspectRatio = false;

	// Preserve Hitori's game-view policy: a level right axis, ignoring roll.
	FMatrix GetCameraRotationMatrix() const;
};
