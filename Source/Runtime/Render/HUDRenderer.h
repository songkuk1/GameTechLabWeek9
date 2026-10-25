#pragma once
#include "Render/PipelineState.h"
#include "Render/Buffer.h"
#include "Render/Vertex.h"

class FHUD;

// FHUD Command를 화면 픽셀 좌표로 그림.
class FHUDRenderer
{
public:
	FHUDRenderer() = default;
	~FHUDRenderer() = default;
	FHUDRenderer(const FHUDRenderer&) = delete;
	FHUDRenderer& operator=(const FHUDRenderer&) = delete;

	bool Init();
	// 바인딩된 렌더 타깃( ScreenWidth x ScreenHeight )에 HUD를 그림.
	void OnRender(const FHUD& HUD, float ScreenWidth, float ScreenHeight);

private:
	void AddQuad(const FVector2& Position, const FVector2& Size, const FVector4& Color);

	static constexpr uint32 MaxQuads = 4096;

	FPipelineState PipelineState;
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
	TArray<FVertex> Vertices;
};