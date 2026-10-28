#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Component/ActorComponent.h"
#include "UObject/Casts.h"
#include <sol/sol.hpp>

namespace
{
	// 스크립트 주인의 게임 월드. 게임 월드가 아니면 nullptr (FindEffectManagerForObject와 같은 규칙)
	UWorld* FindGameWorldForObject(UObject* Object)
	{
		AActor* Actor = Cast<AActor>(Object);
		if (!Actor)
		{
			if (UActorComponent* Component = Cast<UActorComponent>(Object))
				Actor = Component->GetOwner();
		}

		UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		if (!World || !World->IsGameWorld())
			return nullptr;

		return World;
	}
}

void RegisterLuaTimeAPI(sol::environment& Env, UObject* Owner)
{
	// HitStop(Duration, Dilation = 0): 실제 시간으로 Duration 동안 게임 시간을 멈춘다(또는 Dilation배로 늦춘다).
	Env.set_function("HitStop",
		[Owner](float Duration, sol::optional<float> Dilation)
		{
			if (UWorld* World = FindGameWorldForObject(Owner))
				World->StartHitStop(Duration, Dilation.value_or(0.0f));
		});

	// Slomo(Dilation, Duration): 0.1초에 걸쳐 Dilation배로 내려가 Duration 동안 유지한 뒤 돌아온다.
	Env.set_function("Slomo",
		[Owner](float Dilation, float Duration)
		{
			if (UWorld* World = FindGameWorldForObject(Owner))
				World->StartSlomo(Dilation, Duration);
		});

	// 기본 배율. 슬로모·히트스톱은 이 값 위에 곱해진다.
	Env.set_function("SetTimeDilation",
		[Owner](float Dilation)
		{
			if (UWorld* World = FindGameWorldForObject(Owner))
				World->SetTimeDilation(Dilation);
		});

	// 이번 프레임에 실제로 적용되는 최종 배율. 게임 월드가 아니면 1
	Env.set_function("GetTimeDilation",
		[Owner]()
		{
			UWorld* World = FindGameWorldForObject(Owner);
			return World ? World->GetTimeDilation() : 1.0f;
		});
}

void RegisterLuaCameraAPI(sol::environment& Env, UObject* Owner)
{
	// CameraShake(Strength, Duration, Frequency = 20): 감쇠하는 사인파로 카메라를 흔든다. 여러 번 부르면 겹쳐서 흔들린다.
	Env.set_function("CameraShake",
		[Owner](float Strength, float Duration, sol::optional<float> Frequency)
		{
			if (APlayerCameraManager* Manager = FindCameraManagerForObject(Owner))
				Manager->StartCameraShake(Strength, Duration, Frequency.value_or(20.0f));
		});

	Env.set_function("StopCameraShakes",
		[Owner]()
		{
			if (APlayerCameraManager* Manager = FindCameraManagerForObject(Owner))
				Manager->StopAllCameraShakes();
		});

	// StartCameraFade(From, To, Duration, R = 0, G = 0, B = 0, Hold = false): 화면을 색으로 덮는 양을 From -> To로 바꾼다.
	// Hold가 false면 끝나는 순간 Fade가 꺼진다
	Env.set_function("StartCameraFade",
		[Owner](float From, float To, float Duration, sol::optional<float> R, sol::optional<float> G, sol::optional<float> B, sol::optional<bool> bHold)
		{
			if (APlayerCameraManager* Manager = FindCameraManagerForObject(Owner))
				Manager->StartCameraFade(From, To, Duration,
					FLinearColor(R.value_or(0.0f), G.value_or(0.0f), B.value_or(0.0f), 1.0f), bHold.value_or(false));
		});

	Env.set_function("StopCameraFade",
		[Owner]()
		{
			if (APlayerCameraManager* Manager = FindCameraManagerForObject(Owner))
				Manager->StopCameraFade();
		});
}
