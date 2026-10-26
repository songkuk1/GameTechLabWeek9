#include "EnginePCH.h"
#include "Script/LuaBindings.h"
#include "Engine/HUD.h"
#include "Math/Color.h"
#include "Asset/AssetManager.h"
#include "Text/Font.h"
#include "Text/TextRenderer.h"
#include "Core/ResourcePaths.h"
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

	// local W = MeasureHUDText(text [, size])
	// DrawHUDText로 그렸을 때의 가로 폭 (HUD 기준 단위). 가운데·오른쪽 정렬할 때 쓴다.
	Env.set_function("MeasureHUDText",
		[](const FString& Text, sol::optional<float> Size) -> float
		{
			const UFont* Font = FHUD::GetFont();
			if (!Font)
				return 0.0f;

			// FHUDRenderer::AppendText와 같이 글리프 Advance를 더한다. 아틀라스에 없는 글자는 건너뛴다
			float Width = 0.0f;
			for (uint32 CodePoint : DecodeUTF8(Text))
			{
				if (const FGlyphInfo* Glyph = Font->GlyphMap.Find(CodePoint))
					Width += Glyph->Advance;
			}
			return Width * Size.value_or(DefaultHUDFontSize);
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

	// DrawHUDImage(path, x, y, w, h [, tint])
	Env.set_function("DrawHUDImage",
		[Owner](const FString& Path, float X, float Y, float W, float H, sol::optional<FLinearColor> Tint)
		{
			FHUD* HUD = FindHUDForObject(Owner);
			if (!HUD)
				return;

			UTexture2D* Texture = UAssetManager::Get().LoadTexture(Path);
			HUD->AddImage(Texture, FVector2(X, Y), FVector2(W, H), Tint.value_or(DefaultHUDColor));
		});

    // HUDButton(label, x, y, w, h [, size])
	Env.set_function("HUDButton",
		[Owner](const FString& Label, float X, float Y, float W, float H, sol::optional<float> Size) -> bool
		{
			if (FHUD* HUD = FindHUDForObject(Owner))
				return HUD->Button(Label, FVector2(X, Y), FVector2(W, H), Size.value_or(DefaultHUDFontSize));
			return false;
		});

	// HUDImageButton(path, x, y, w, h [, label [, size]])
	// 이미지 하나로 만든 버튼. Hover면 밝게, 누르는 중이면 어둡게 그린다.
	Env.set_function("HUDImageButton",
		[Owner](const FString& Path, float X, float Y, float W, float H,
			sol::optional<FString> Label, sol::optional<float> Size) -> bool
		{
			FHUD* HUD = FindHUDForObject(Owner);
			if (!HUD)
				return false;

			UTexture2D* Texture = UAssetManager::Get().LoadTexture(Path);
			return HUD->ImageButton(Texture, FVector2(X, Y), FVector2(W, H),
				Label.value_or(FString()), Size.value_or(DefaultHUDFontSize));
		});
}