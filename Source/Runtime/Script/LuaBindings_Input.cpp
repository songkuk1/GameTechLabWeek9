#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Input/InputSystem.h"

#include <cctype>
#include <sol/sol.hpp>

namespace
{
	// Lua에서는 키를 "W", "Space"처럼 이름으로 넘긴다. 한 글자는 영문자·숫자, 그 밖에는 아래 이름만 받는다.
	EKeyCode ToKeyCode(const std::string& Name)
	{
		if (Name.size() == 1)
		{
			const int Ch = std::toupper(static_cast<unsigned char>(Name[0]));
			if ((Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9'))
				return static_cast<EKeyCode>(Ch);
		}

		static const TMap<std::string, EKeyCode> Named =
		{
			{ "Space", EKeyCode::Space },     { "Enter", EKeyCode::Enter },
			{ "Escape", EKeyCode::Escape },   { "Tab", EKeyCode::Tab },
			{ "Shift", EKeyCode::Shift },     { "Control", EKeyCode::Control },
			{ "Alt", EKeyCode::Alt },
			{ "Left", EKeyCode::Left },       { "Right", EKeyCode::Right },
			{ "Up", EKeyCode::Up },           { "Down", EKeyCode::Down },
		};
		if (const EKeyCode* Found = Named.Find(Name))
			return *Found;

		throw sol::error(std::format("Input : 알 수 없는 키 이름 '{}'", Name));
	}
}

void RegisterLuaInputBindings(sol::state& Lua)
{
	// 스크립트에서 Input.IsKeyDown("W")처럼 쓴다.
	// Down은 누르고 있는 동안 매 프레임, Pressed·Released는 눌린 프레임·뗀 프레임에만 true다.
	sol::table Input = Lua.create_named_table("Input");
	Input["IsKeyDown"]     = [](const std::string& Key) { return FInputSystem::IsKeyDown(ToKeyCode(Key)); };
	Input["IsKeyPressed"]  = [](const std::string& Key) { return FInputSystem::IsKeyPressed(ToKeyCode(Key)); };
	Input["IsKeyReleased"] = [](const std::string& Key) { return FInputSystem::IsKeyReleased(ToKeyCode(Key)); };
}
