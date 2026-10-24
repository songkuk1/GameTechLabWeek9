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

		"UUID",      sol::readonly_property([](const UObject& O) { return O.GetUUID(); }),
		"Name",      sol::readonly_property([](const UObject& O) { return O.GetName(); }),
		"ClassName", sol::readonly_property([](const UObject& O) { return O.GetClass() ? O.GetClass()->Name : FString(); }),

		sol::meta_function::index,     [](UObject& O, const std::string& Key, sol::this_state S) { return LuaReflection::Index(O, Key, S); },
		sol::meta_function::new_index, [](UObject& O, const std::string& Key, sol::stack_object Value) { LuaReflection::SetProperty(O, Key, Value); }
	);
}
