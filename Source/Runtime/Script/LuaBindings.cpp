#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include <sol/sol.hpp>

void RegisterLuaBindings(sol::state& Lua)
{
	// 리플렉션이 FVector·FRotator 등을 돌려주므로 수학 타입을 먼저 등록한다.
	// 액터·컴포넌트는 모두 FLuaObjectRef 핸들 하나로 넘어가므로 액터 전용 usertype은 없다.
	RegisterLuaMathBindings(Lua);
	RegisterLuaObjectBindings(Lua);
	RegisterLuaInputBindings(Lua);
	RegisterLuaGameBindings(Lua);
	RegisterLuaAudioBindings(Lua);
}
