#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Engine/HUD.h"
#include "Math/Color.h"
#include <sol/sol.hpp>

namespace
{
	const FLinearColor DefaultHUDColor(1.0f, 1.0f, 1.0f, 1.0f);
	constexpr float DefaultHUDFontSize = 24.0f;
}

void RegisterLuaHUDAPI(sol::environment& Env, UObject* Owner)
{
	// DrawHUDRect(x, y, w, h [, color])
	Env.set_function("DrawHUDRect",
		[Owner](float X, float Y, float W, float H, sol::optional<FLinearColor> Color)
		{
			if (FHUD* HUD = FindHUDForObject(Owner))
				HUD->AddRect(FVector2(X, Y), FVector2(W, H), Color.value_or(DefaultHUDColor));
		});

	// DrawHUDText(text, x, y [, size [, color]])
	Env.set_function("DrawHUDText",
		[Owner](const FString& Text, float X, float Y, sol::optional<float> Size, sol::optional<FLinearColor> Color)
		{
			if (FHUD* HUD = FindHUDForObject(Owner))
				HUD->AddText(Text, FVector2(X, Y), Size.value_or(DefaultHUDFontSize), Color.value_or(DefaultHUDColor));
		});

	// local W, H = GetHUDSize()
	Env.set_function("GetHUDSize",
		[Owner]() -> std::tuple<float, float>
		{
			if (const FHUD* HUD = FindHUDForObject(Owner))
			{
				const FVector2 Size = HUD->GetVirtualSize();
				return { Size.X, Size.Y };
			}
			return { FHUD::ReferenceWidth, FHUD::ReferenceHeight };
		});
}