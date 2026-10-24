#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include <sol/sol.hpp>

void RegisterLuaBindings(sol::state& Lua)
{
	// 액터 바인딩이 FVector·FRotator를 돌려주고 UObject를 부모로 두므로 그 순서대로 등록한다.
	RegisterLuaMathBindings(Lua);
	RegisterLuaObjectBindings(Lua);
	RegisterLuaActorBindings(Lua);
}