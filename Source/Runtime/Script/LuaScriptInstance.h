#pragma once
#include "Core/EngineString.h"
#include "Core/EngineLog.h"

// Call이 가변 인자 템플릿이라 헤더에 구현이 있어야 하므로 sol을 여기서 include한다.
// Actor.h처럼 널리 쓰이는 헤더는 이 파일 대신 전방 선언만 둔다.
#include <sol/sol.hpp>

class AActor;

// 액터 하나에 붙는 Lua 스크립트. 스크립트 전용 environment(전역 공간)를 소유한다.
class FLuaScriptInstance
{
public:
	FLuaScriptInstance(AActor* InOwner, const FString& InPath);
	~FLuaScriptInstance();

	// environment를 만들고 파일을 실행해 함수들을 등록한다. 실패하면 로그를 남기고 false.
	bool Load();
	bool IsLoaded() const { return bLoaded; }
	const FString& GetPath() const { return Path; }

	// 스크립트에 FuncName 함수가 있으면 호출한다. 없으면 아무것도 하지 않는다.
	// Lua 쪽 에러는 로그만 남기고 엔진은 계속 돈다.
	template <typename... Args>
	void Call(const char* FuncName, Args&&... InArgs)
	{
		if (!bLoaded)
			return;

		sol::protected_function Func = Env[FuncName];
		if (!Func.valid())
			return;

		sol::protected_function_result Result = Func(std::forward<Args>(InArgs)...);
		if (!Result.valid())
		{
			sol::error Err = Result;
			HTR_LOG(Error, "[Lua] {} : {}() 실패 : {}", Path, FuncName, Err.what());
		}
	}

private:
	AActor* Owner = nullptr;
	FString Path;
	sol::environment Env;
	bool bLoaded = false;
};
