#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Script/LuaReflection.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

void RegisterLuaObjectBindings(sol::state& Lua)
{
	// 컴포넌트처럼 따로 usertype이 없는 UObject는 이 타입으로 넘어간다.
	// 명시적 멤버가 없으면 리플렉션(FUNCTION, PROPERTY)에서 찾는다.
	Lua.new_usertype<UObject>("UObject",
		sol::no_constructor,

		// GetUUID·GetName은 UObject::StaticClass에서 FUNCTION으로 등록한다.
		// 클래스 이름은 UClass*를 거쳐야 해서(FUNCTION이 지원하지 않는 타입) 명시적으로 둔다.
		"ClassName", sol::readonly_property([](const UObject& O) { return O.GetClass() ? O.GetClass()->Name : FString(); }),

		sol::meta_function::index,     [](UObject& O, const std::string& Key, sol::this_state S) { return LuaReflection::Index(O, Key, S); },
		sol::meta_function::new_index, [](UObject& O, const std::string& Key, sol::stack_object Value) { LuaReflection::SetProperty(O, Key, Value); }
	);
}
