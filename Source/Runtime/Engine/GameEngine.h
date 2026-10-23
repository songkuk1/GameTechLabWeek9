#pragma once

#include "Engine.h"
#include "Engine/World.h"
#include "Render/SceneRenderer.h"
#include "Engine/Viewport.h"
#include "SceneRenderPipeline.h"

class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)
public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnResize(uint32 Width, uint32 Height) override;
	void PreExit() override;

private:
	UWorld* GameWorld = nullptr;
	FSceneRenderer SceneRenderer;
	//FXAA나 Fog Renderer를 보관중
	FSceneRenderPipeline ScenePipeline;
	FViewport Viewport;
	

	void LoadStartScene();
	void SetupMainCamera();
	void UpdateGameInput();
	void RenderViewports();

	void RenderFrame(FViewport& InViewport, UWorld& ViewWorld, const FSceneView& View);
	// 뷰포트의 오프스크린 결과를 메인 스왑체인 백버퍼로 복사한다. Present는 FEngineLoop가 한다.
	void CopyViewportToBackbuffer();
	void ReleaseWorldShadowResources(UWorld* World);
	
};