#pragma once
#include "EnginePCH.h"
#include "Camera/CameraProjection.h"

//카메라 시점 하나"를 표현하는 구조체.
struct FMinimalViewInfo
{
public:
	FMinimalViewInfo() = default;
	~FMinimalViewInfo() = default;

		//카메라 위치
		FVector Location;
		//카메라 방향
		FRotator Rotation;
		//투영 설정 (원근/직교, 시야각, 직교 폭, Near/Far)
		FCameraProjection Projection;
		//화면비
		float AspectRatio = 1.333f;
		//true면 AspectRatio를 강제하고 남는 영역은 검은 띠로 채움 -> letter box
		bool bConstrainAspectRatio = false;

	// 이 시점에서 Other 쪽으로 Alpha(0~1)만큼 섞는다
	void BlendViewInfo(const FMinimalViewInfo& Other, float Alpha)
	{
		Location = FMath::Lerp(Location, Other.Location, Alpha);
		Rotation = FQuat::Slerp(Rotation.Quaternion(), Other.Rotation.Quaternion(), Alpha).ToFRotator();
		Projection.FovDegrees = FMath::Lerp(Projection.FovDegrees, Other.Projection.FovDegrees, Alpha);
		Projection.OrthoWidth = FMath::Lerp(Projection.OrthoWidth, Other.Projection.OrthoWidth, Alpha);
		Projection.NearClip = FMath::Lerp(Projection.NearClip, Other.Projection.NearClip, Alpha);
		Projection.FarClip = FMath::Lerp(Projection.FarClip, Other.Projection.FarClip, Alpha);
		AspectRatio = FMath::Lerp(AspectRatio, Other.AspectRatio, Alpha);

		// 원근/직교, 레터박스 여부는 중간값이 없어서 절반을 넘으면 도착 쪽 값으로 바꾼다
		if (Alpha >= 0.5f)
		{
			Projection.Mode = Other.Projection.Mode;
			bConstrainAspectRatio = Other.bConstrainAspectRatio;
		}
	}
};
