#pragma once
#include <string>
#include <sol/sol.hpp>

class UObject;
class UClass;

// 리플렉션(PROPERTY, FUNCTION)에 등록된 멤버를 Lua에서 이름으로 읽고 쓰고 호출한다.
// usertype의 __index/__newindex fallback으로 쓰므로, 명시적으로 바인딩된 멤버가 항상 먼저 찾아진다.
namespace LuaReflection
{
	// __index fallback. 클래스별 함수(부모까지) → 프로퍼티 순서로 찾는다.
	sol::object Index(UObject& Object, const std::string& Key, sol::this_state S);

	// 등록된 프로퍼티가 없으면 nil을 돌려준다(Lua 관례). 지원하지 않는 타입이면 Lua 에러.
	sol::object GetProperty(UObject& Object, const std::string& Key, sol::this_state S);

	// 없는 프로퍼티, 읽기 전용(CPF_EditConst), 타입이 맞지 않는 값이면 Lua 에러.
	// 디테일 패널과 같이 쓰고 나서 PostEditChangeProperty를 호출한다.
	void SetProperty(UObject& Object, const std::string& Key, sol::stack_object Value);

	// FUNCTION()으로 등록된 함수를 Lua 함수로 감싼 테이블. 클래스당 한 번 만들어 레지스트리에 캐시한다.
	// 이 클래스에 직접 선언된 함수만 담고, 부모 클래스의 함수는 Index가 따라 올라가며 찾는다.
	sol::table GetClassFunctionTable(sol::state_view Lua, UClass* Class);

	// 실제 클래스에 맞는 usertype으로 넘긴다. 액터는 AActor로, 나머지는 UObject로.
	sol::object MakeObject(sol::state_view Lua, UObject* Object);
}
