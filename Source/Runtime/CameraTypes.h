#pragma once
#include "EnginePCH.h"

//카메라 시점 하나"를 표현하는 구조체.
struct FMinimalViewInfo
{
public:
	FMinimalViewInfo() = default;
	~FMinimalViewInfo() = default;
	FMinimalViewInfo(const FMinimalViewInfo&) = delete;

		//카메라 위치
		FVector Location;
		//카메라 방향
		FRotator Rotation;
		//시야각
		float Fov;
		//화면비
		float AspectRatio;
		//true면 AspectRatio를 강제하고 남는 영역은 검은 띠로 채움 -> letter box
		bool bConstrainAspectRatio;
};