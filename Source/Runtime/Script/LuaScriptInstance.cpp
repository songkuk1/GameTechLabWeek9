#include "EnginePCH.h"
#include "Script/LuaScriptInstance.h"
#include "Script/LuaScriptManager.h"
#include "GameFramework/Actor.h"   // sol이 AActor*를 Lua 값으로 바꾸려면 완전한 타입이 필요하다

FLuaScriptInstance::FLuaScriptInstance(AActor* InOwner, const FString& InPath)
	: Owner(InOwner), Path(InPath)
{
}

// Env는 Lua VM 안의 테이블을 참조로 붙잡고 있다. VM(FLuaScriptManager)보다 먼저 파괴되어야 한다.
FLuaScriptInstance::~FLuaScriptInstance() = default;

bool FLuaScriptInstance::Load()
{
	bLoaded = false;

	sol::state& Lua = FLuaScriptManager::Get().GetState();

	// 못 찾은 이름은 전역(print, math, 바인딩된 타입)에서 찾도록 globals를 fallback으로 둔다.
	Env = sol::environment(Lua, sol::create, Lua.globals());
	Env["obj"] = Owner;

	sol::protected_function_result Result = Lua.safe_script_file(Path, Env, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Lua] {} 로드 실패 : {}", Path, Err.what());
		Env = sol::environment();
		return false;
	}

	bLoaded = true;
	return true;
}
