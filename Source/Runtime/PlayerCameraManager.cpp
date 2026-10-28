#include "EnginePCH.h"
#include "PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/WaveCameraShake.h"

APlayerCameraManager::~APlayerCameraManager()
{
	ClearCameraModifiers();
}

void APlayerCameraManager::EndPlay()
{
	ClearCameraModifiers();
	Super::EndPlay();
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	PCOwner = PC;
	GetCameraShakeModifier();
	AActor* Initial = GetWorld() ? GetWorld()->GetMainCamera() : nullptr;
	SetViewTarget(Initial ? Initial : PC);
	UpdateCamera(0.0f);
}

void APlayerCameraManager::SetViewTarget(AActor* InActor)
{
	ViewTarget.Target = InActor;
	bCameraCacheValid = false;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	UpdateViewTarget(ViewTarget, DeltaTime);
	if (!ViewTarget.Target.IsValid())
	{
		bCameraCacheValid = false;
		return;
	}
	CameraCache = ViewTarget.POV;
	ApplyCameraModifiers(DeltaTime, CameraCache);
	bCameraCacheValid = true;
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& VT, float DeltaTime)
{
	AActor* Target = VT.Target.Get();
	if (!Target)
	{
		Target = GetWorld() ? GetWorld()->GetMainCamera() : nullptr;
		VT.Target = Target;
	}
	if (Target)
		Target->CalcCamera(DeltaTime, VT.POV);
}

UCameraModifier* APlayerCameraManager::AddNewCameraModifier(UClass* ModifierClass)
{
	if (!ModifierClass || !ModifierClass->IsChildOf(UCameraModifier::StaticClass()))
		return nullptr;
	UCameraModifier* Modifier = NewObject<UCameraModifier>(this, ModifierClass,
		NAME_None, EObjectFlags::RF_Transient);
	if (!Modifier)
		return nullptr;
	Modifier->InitializeFor(this);
	int32 Index = 0;
	while (Index < ModifierList.Num())
	{
		const UCameraModifier* Existing = ModifierList[Index].Get();
		if (Existing && Existing->Priority > Modifier->Priority)
			break;
		++Index;
	}
	ModifierList.Insert(TWeakObjectPtr<UCameraModifier>(Modifier), Index);
	if (auto* ShakeModifier = Cast<UCameraModifier_CameraShake>(Modifier))
		CachedCameraShakeMod = ShakeModifier;
	return Modifier;
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
	for (int32 Index = 0; Index < ModifierList.Num(); ++Index)
	{
		if (ModifierList[Index].Get() == Modifier)
		{
			ModifierList.RemoveAt(Index);
			if (CachedCameraShakeMod.Get() == Modifier)
				CachedCameraShakeMod.Reset();
			delete Modifier;
			return;
		}
	}
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	const auto Modifiers = ModifierList;
	for (const auto& Handle : Modifiers)
	{
		UCameraModifier* Modifier = Handle.Get();
		if (Modifier && !Modifier->IsDisabled() && Modifier->ModifyCamera(DeltaTime, InOutPOV))
			break;
	}
}

UCameraModifier_CameraShake* APlayerCameraManager::GetCameraShakeModifier()
{
	if (!CachedCameraShakeMod.IsValid())
		AddNewCameraModifier(UCameraModifier_CameraShake::StaticClass());
	return CachedCameraShakeMod.Get();
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(
	UClass* ShakeClass, float Scale, ECameraShakePlaySpace Space)
{
	UCameraModifier_CameraShake* Modifier = GetCameraShakeModifier();
	return Modifier ? Modifier->AddCameraShake(ShakeClass, Scale, Space) : nullptr;
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(float Strength, float Duration, float Frequency)
{
	UCameraModifier_CameraShake* Modifier = GetCameraShakeModifier();
	if (!Modifier)
		return nullptr;
	UWaveCameraShake* Shake = NewObject<UWaveCameraShake>(Modifier);
	if (!Shake)
		return nullptr;
	Shake->Configure(Strength, Duration, Frequency);
	return Modifier->AddCameraShake(Shake);
}

void APlayerCameraManager::StopCameraShake(UCameraShakeBase* Shake)
{
	if (auto* Modifier = CachedCameraShakeMod.Get())
		Modifier->RemoveCameraShake(Shake);
}

void APlayerCameraManager::StopAllCameraShakes()
{
	if (auto* Modifier = CachedCameraShakeMod.Get())
		Modifier->RemoveAllCameraShakes();
}

void APlayerCameraManager::ClearCameraModifiers()
{
	const auto Modifiers = ModifierList;
	ModifierList.Empty();
	CachedCameraShakeMod.Reset();
	for (const auto& Modifier : Modifiers)
		delete Modifier.Get();
}
