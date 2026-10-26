#include "EnginePCH.h"
#include "Script/LuaScriptInstance.h"
#include "Script/LuaScriptManager.h"
#include "Script/LuaReflection.h"
#include "Script/LuaBindings.h"
#include "GameFramework/Actor.h"
#include "Component/ActorComponent.h"

FLuaScriptInstance::FLuaScriptInstance(UObject* InOwner, const FString& InPath)
	: Owner(InOwner), Path(InPath)
{
}

// Env는 Lua VM 안의 테이블을 참조로 붙잡고 있다. VM(FLuaScriptManager)보다 먼저 파괴되어야 한다.
FLuaScriptInstance::~FLuaScriptInstance() = default;

std::unique_ptr<FLuaScriptInstance> FLuaScriptInstance::CreateAndLoad(UObject* InOwner, const FString& InPath)
{
	if (InPath.empty())
		return nullptr;

	auto Instance = std::make_unique<FLuaScriptInstance>(InOwner, InPath);
	if (!Instance->Load())
		return nullptr;

	return Instance;
}

bool FLuaScriptInstance::Load()
{
	bLoaded = false;

	sol::state& Lua = FLuaScriptManager::Get().GetState();

	// 못 찾은 이름은 전역(print, math, 바인딩된 타입)에서 찾도록 globals를 fallback으로 둔다.
	Env = sol::environment(Lua, sol::create, Lua.globals());

	// 실제 클래스에 맞는 usertype으로 넘긴다. 액터는 AActor, 컴포넌트는 UObject로 넘어간다.
	Env["obj"] = LuaReflection::MakeObject(Lua, Owner);
	if (UActorComponent* Component = Cast<UActorComponent>(Owner))
		Env["owner"] = LuaReflection::MakeObject(Lua, Component->GetOwner());

	// start, stop, wait Script 을 Env에 등록
	Scheduler.RegisterLuaAPI(Env);
	RegisterLuaHUDAPI(Env, Owner);
	RegisterLuaInputAPI(Env, Owner);

	sol::protected_function_result Result = Lua.safe_script_file(Path, Env, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Lua] {} 로드 실패 : {}", Path, Err.what());

		// 실행 도 중 시작된 Coroutine이 있으면 Env 비우기 전에 정리
		Scheduler.StopAll();
		Env = sol::environment();
		return false;
	}

	bLoaded = true;
	return true;
}
