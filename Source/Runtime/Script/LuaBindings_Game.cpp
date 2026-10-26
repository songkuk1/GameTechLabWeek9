#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Engine/Engine.h"
#include <sol/sol.hpp>

void RegisterLuaGameBindings(sol::state& Lua)
{
	// OpenScene("Assets/Scenes/GameJamMap0.scene")
	// 요청만 해 두고 실제 전환은 다음 프레임 시작에 엔진이 한다. 호출한 스크립트는 이번 프레임을 끝까지 돈다.
	// 게임은 게임 월드를, 에디터는 PIE 세션을 유지한 채 PlayWorld만 바꾼다.
	Lua.set_function("OpenScene", [](const FString& ScenePath)
		{
			if (GEngine)
				GEngine->TravelToScene(ScenePath);
		});

	// QuitGame(): 게임은 앱을 끝내고, 에디터 PIE에서는 PIE 세션만 끝낸다.
	Lua.set_function("QuitGame", []()
		{
			if (GEngine)
				GEngine->QuitGame();
		});
}
