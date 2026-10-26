#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Script/LuaReflection.h"
#include "Engine/World.h"
#include "Engine/EffectManager.h"
#include "GameFramework/Actor.h"
#include "Component/ActorComponent.h"
#include "UObject/Casts.h"
#include <sol/sol.hpp>

namespace
{
	const FLinearColor DefaultTextColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 스크립트 주인의 월드 이펙트 매니저. 게임 월드가 아니면 nullptr (FindHUDForObject와 같은 규칙)
	FEffectManager* FindEffectManagerForObject(UObject* Object)
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

		return &World->GetEffectManager();
	}

	// RegisterEffect 테이블 값 읽기. 타입이 틀리면 어느 키인지 알려 주는 Lua 에러를 낸다.
	float ReadNumber(const sol::object& Value, const FString& Effect, const std::string& Key)
	{
		if (!Value.is<float>())
			throw sol::error(std::format("RegisterEffect({}): {}는 숫자여야 합니다", Effect, Key));
		return Value.as<float>();
	}

	bool ReadBool(const sol::object& Value, const FString& Effect, const std::string& Key)
	{
		if (!Value.is<bool>())
			throw sol::error(std::format("RegisterEffect({}): {}는 true/false여야 합니다", Effect, Key));
		return Value.as<bool>();
	}

	FVector ReadVector(const sol::object& Value, const FString& Effect, const std::string& Key)
	{
		if (!Value.is<FVector>())
			throw sol::error(std::format("RegisterEffect({}): {}는 FVector여야 합니다", Effect, Key));
		return Value.as<FVector>();
	}

	uint32 ReadCount(const sol::object& Value, const FString& Effect, const std::string& Key, uint32 Min)
	{
		const float Number = ReadNumber(Value, Effect, Key);
		if (Number < static_cast<float>(Min))
			throw sol::error(std::format("RegisterEffect({}): {}는 {} 이상이어야 합니다", Effect, Key, Min));
		return static_cast<uint32>(Number);
	}

	// 테이블의 키를 FEffectDesc 필드에 넣는다. 키 이름은 C++ 필드 이름과 같다.
	// 모르는 키는 오타일 가능성이 커서 경고만 하고 넘어간다.
	void ApplyEffectTable(const FString& Effect, const sol::table& Table, FEffectDesc& Desc)
	{
		for (const auto& [KeyObject, Value] : Table)
		{
			if (!KeyObject.is<std::string>())
				continue;

			const std::string Key = KeyObject.as<std::string>();
			if (Key == "Base")
				continue;
			else if (Key == "TexturePath")
			{
				if (!Value.is<std::string>())
					throw sol::error(std::format("RegisterEffect({}): TexturePath는 문자열이어야 합니다", Effect));
				Desc.TexturePath = Value.as<std::string>();
			}
			else if (Key == "Cols")              Desc.Cols = ReadCount(Value, Effect, Key, 1);
			else if (Key == "Rows")              Desc.Rows = ReadCount(Value, Effect, Key, 1);
			else if (Key == "FrameCount")        Desc.FrameCount = ReadCount(Value, Effect, Key, 0);
			else if (Key == "FrameRate")         Desc.FrameRate = ReadNumber(Value, Effect, Key);
			else if (Key == "bRandomStartFrame") Desc.bRandomStartFrame = ReadBool(Value, Effect, Key);
			else if (Key == "ParticleCount")     Desc.ParticleCount = static_cast<int32>(ReadCount(Value, Effect, Key, 0));
			else if (Key == "MinLifeTime")       Desc.MinLifeTime = ReadNumber(Value, Effect, Key);
			else if (Key == "MaxLifeTime")       Desc.MaxLifeTime = ReadNumber(Value, Effect, Key);
			else if (Key == "MinVelocity")       Desc.MinVelocity = ReadVector(Value, Effect, Key);
			else if (Key == "MaxVelocity")       Desc.MaxVelocity = ReadVector(Value, Effect, Key);
			else if (Key == "Acceleration")      Desc.Acceleration = ReadVector(Value, Effect, Key);
			else if (Key == "StartScale")        Desc.StartScale = ReadNumber(Value, Effect, Key);
			else if (Key == "EndScale")          Desc.EndScale = ReadNumber(Value, Effect, Key);
			else if (Key == "FadeStart")         Desc.FadeStart = ReadNumber(Value, Effect, Key);
			else if (Key == "bBurst")            Desc.bBurst = ReadBool(Value, Effect, Key);
			else
				HTR_LOG(Warning, "[Lua] RegisterEffect({}): unknown key '{}' ignored", Effect, Key);
		}
	}
}

void RegisterLuaEffectAPI(sol::environment& Env, UObject* Owner)
{
	// local h = SpawnEffect(name, location [, rotation [, loop [, duration]]])
	// 핸들은 숫자다. 실패하면 -1
	Env.set_function("SpawnEffect",
		[Owner](const FString& Name, const FVector& Location, sol::optional<FRotator> Rotation,
			sol::optional<bool> bLoop, sol::optional<float> Duration) -> int64
		{
			FEffectManager* Effects = FindEffectManagerForObject(Owner);
			if (!Effects)
				return -1;
			return Effects->SpawnEffect(FName(Name), Location, Rotation.value_or(FRotator()),
				bLoop.value_or(false), Duration.value_or(0.0f)).Pack();
		});

	// local h = SpawnEffectAttached(name, actor [, offset [, loop [, duration [, rotation]]]])
	// actor의 루트에 붙어 따라다닌다. offset·rotation은 루트 기준 상대값
	Env.set_function("SpawnEffectAttached",
		[Owner](const FString& Name, sol::object Target, sol::optional<FVector> Offset,
			sol::optional<bool> bLoop, sol::optional<float> Duration, sol::optional<FRotator> Rotation) -> int64
		{
			FEffectManager* Effects = FindEffectManagerForObject(Owner);
			if (!Effects)
				return -1;

			AActor* TargetActor = Target.is<FLuaObjectRef>() ? Cast<AActor>(Target.as<FLuaObjectRef>().Weak.Get()) : nullptr;
			if (!TargetActor)
			{
				HTR_LOG(Warning, "[Lua] SpawnEffectAttached({}): target is not a live actor", Name);
				return -1;
			}

			FTransform Relative;
			Relative.Location = Offset.value_or(FVector(0.0f, 0.0f, 0.0f));
			Relative.Rotation = Rotation.value_or(FRotator());
			return Effects->SpawnEffectAttached(FName(Name), TargetActor, Relative,
				bLoop.value_or(false), Duration.value_or(0.0f)).Pack();
		});

	// SpawnTextEffect(text, location [, color [, duration]])
	Env.set_function("SpawnTextEffect",
		[Owner](const FString& Text, const FVector& Location, sol::optional<FLinearColor> Color, sol::optional<float> Duration)
		{
			if (FEffectManager* Effects = FindEffectManagerForObject(Owner))
				Effects->SpawnTextEffect(Text, Location, Color.value_or(DefaultTextColor), Duration.value_or(1.0f));
		});

	// StopEffect(h [, immediate]) : immediate가 아니면 방출만 멈추고 남은 입자가 사라진 뒤 정리된다
	Env.set_function("StopEffect",
		[Owner](int64 Handle, sol::optional<bool> bImmediate)
		{
			if (FEffectManager* Effects = FindEffectManagerForObject(Owner))
				Effects->StopEffect(FEffectHandle::Unpack(Handle), bImmediate.value_or(false));
		});

	// IsEffectActive(h) : 아직 재생 중이거나 남은 입자가 사라지는 중이면 true
	Env.set_function("IsEffectActive",
		[Owner](int64 Handle) -> bool
		{
			FEffectManager* Effects = FindEffectManagerForObject(Owner);
			return Effects && Effects->IsEffectActive(FEffectHandle::Unpack(Handle));
		});

	// RegisterEffect(name, { Base = "HitSplash", ParticleCount = 20, MinVelocity = FVector(...), ... })
	// 키 이름은 FEffectDesc 필드와 같다. Base가 있으면 그 프리셋에서 시작하고, 없으면 기본값에서 시작한다.
	// 같은 이름이 있으면 덮어쓴다. 프리셋은 플레이가 끝나도 남는다.
	Env.set_function("RegisterEffect",
		[Owner](const FString& Name, const sol::table& Table)
		{
			FEffectManager* Effects = FindEffectManagerForObject(Owner);
			if (!Effects)
				return;

			FEffectDesc Desc;
			if (sol::optional<std::string> Base = Table["Base"])
			{
				const FEffectDesc* BaseDesc = Effects->FindEffect(FName(*Base));
				if (!BaseDesc)
					throw sol::error(std::format("RegisterEffect({}): Base '{}' 프리셋이 없습니다", Name, *Base));
				Desc = *BaseDesc;
			}

			ApplyEffectTable(Name, Table, Desc);
			Effects->RegisterEffect(FName(Name), Desc);
		});
}
