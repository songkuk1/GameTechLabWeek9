#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Script/LuaReflection.h"
#include "GameFramework/Actor.h"

void RegisterLuaActorBindings(sol::state& Lua)
{
	// ClassName은 UObject에서 물려받는다.
	// 액터는 엔진이 소유하므로 Lua에서 만들 수 없게 하고, 항상 포인터로 넘긴다.
	// GetActorLocation·GetRootComponent·FindComponentByName 등은 Actor.h의 FUNCTION으로 등록되어 있다.
	Lua.new_usertype<AActor>("AActor",
		sol::no_constructor,
		sol::base_classes, sol::bases<UObject>(),

		// TArray를 반환해 FUNCTION으로 등록할 수 없으므로 명시적으로 둔다.
		// Lua 배열(1부터 시작하는 테이블)로 돌려준다. ipairs로 순회한다.
		"GetComponents", [](const AActor& A, sol::this_state S) {
			sol::state_view Lua(S);
			sol::table Result = Lua.create_table();
			for (UActorComponent* Component : A.GetComponents())
			{
				if (Component)
					Result.add(LuaReflection::MakeObject(Lua, Component));
			}
			return Result;
		},

		// 위에 없는 이름은 리플렉션(FUNCTION, PROPERTY)에서 찾는다. 예: obj.ScriptPath
		// sol2는 부모 usertype을 탐색할 때 부모의 fallback을 부르지 않으므로 여기에도 둔다.
		sol::meta_function::index,     [](AActor& A, const std::string& Key, sol::this_state S) { return LuaReflection::Index(A, Key, S); },
		sol::meta_function::new_index, [](AActor& A, const std::string& Key, sol::stack_object Value) { LuaReflection::SetProperty(A, Key, Value); }
	);
}
