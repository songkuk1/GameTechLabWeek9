#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Script/LuaReflection.h"
#include "GameFramework/Actor.h"

void RegisterLuaActorBindings(sol::state& Lua)
{
	// UUID·Name·ClassName은 UObject에서 물려받는다.
	// 액터는 엔진이 소유하므로 Lua에서 만들 수 없게 하고, 항상 포인터로 넘긴다.
	Lua.new_usertype<AActor>("AActor",
		sol::no_constructor,
		sol::base_classes, sol::bases<UObject>(),

		// AActor가 이미 제공하는 getter만 읽기 전용으로 노출한다.
		// 반환값은 복사본이다. obj.Location.X = 1 처럼 고쳐도 액터는 바뀌지 않는다.
		// GetActorRotation은 선언만 있고 구현이 없어 아직 노출하지 않는다.
		"Location", sol::readonly_property([](const AActor& A) { return A.GetActorLocation(); }),
		"Scale",    sol::readonly_property([](const AActor& A) { return A.GetActorScale3D(); }),

		// 컴포넌트는 따로 usertype이 없어 UObject로 넘어가고, 프로퍼티는 리플렉션으로 읽고 쓴다.
		"GetRootComponent", [](const AActor& A, sol::this_state S) {
			return LuaReflection::MakeObject(sol::state_view(S), A.GetRootComponent());
		},
		// 없으면 nil
		"FindComponentByName", [](const AActor& A, const FString& Name, sol::this_state S) {
			return LuaReflection::MakeObject(sol::state_view(S), A.FindComponentByName(FName(Name)));
		},
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
