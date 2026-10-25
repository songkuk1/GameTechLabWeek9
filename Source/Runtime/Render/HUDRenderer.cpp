#include "EnginePCH.h"
#include "Render/HUDRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Engine/HUD.h"

bool FHUDRenderer::Init()
{
	// 위치, UV, 색 정점을 행렬 하나로 변환하고 색만 출력하는 셰이더
	PipelineState.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/BoundingBoxShader.hlsl");
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;        // Culling 끔
	PipelineState.BlendState = EBlendState::AlphaBlend;                 // 반투명 패널
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;     // 항상 맨 위

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FVertex) * MaxQuads * 6, sizeof(FVertex));
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return PipelineState.Shader != nullptr;
}

void FHUDRenderer::AddQuad(const FVector2& Position, const FVector2& Size, const FVector4& Color)
{
	if (static_cast<uint32>(Vertices.Num()) + 6 > MaxQuads * 6)
		return;   // 넘치면 버린다

	const float L = Position.X;
	const float T = Position.Y;
	const float R = Position.X + Size.X;
	const float B = Position.Y + Size.Y;
	const FVector2 UV;

	// 왼쪽 위 -> 오른쪽 위 ->  오른쪽 아래 / 왼쪽 위 ->  오른쪽 아래 ->  왼쪽 아래
	Vertices.Add({ FVector(L, T, 0.0f), UV, Color });
	Vertices.Add({ FVector(R, T, 0.0f), UV, Color });
	Vertices.Add({ FVector(R, B, 0.0f), UV, Color });
	Vertices.Add({ FVector(L, T, 0.0f), UV, Color });
	Vertices.Add({ FVector(R, B, 0.0f), UV, Color });
	Vertices.Add({ FVector(L, B, 0.0f), UV, Color });
}

void FHUDRenderer::OnRender(const FHUD& HUD, float ScreenWidth, float ScreenHeight)
{
	if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
		return;

	Vertices.Reset();
	for (const FHUDDrawCommand& Command : HUD.GetCommands())
	{
		if (Command.Type == EHUDCommandType::Rect)
			AddQuad(Command.Position, Command.Size, FVector4(Command.Color.R, Command.Color.G, Command.Color.B, Command.Color.A));
		// Text는 3단계에서 처리한다
	}
	if (Vertices.Num() == 0)
		return;

	// 픽셀(왼쪽 위 0,0 / 아래로 +y) -> NDC(-1~1 / 위로 +y)
	const FMatrix PixelToNDC(
		2.0f / ScreenWidth, 0.0f,                 0.0f, 0.0f,
		0.0f,               -2.0f / ScreenHeight, 0.0f, 0.0f,
		0.0f,               0.0f,                 1.0f, 0.0f,
		-1.0f,              1.0f,                 0.0f, 1.0f);

	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FVertex) * Vertices.Num());
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &PixelToNDC, sizeof(FMatrix));

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::Draw(Vertices.Num(), 0);
}