#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Core/ProjectSettings.h"
#include "Serialization/JsonArchive.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"
#include "Render/Swapchain.h"
#include "Render/Texture2D.h"

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 1280;          // 0이면 모니터 전체 해상도
	Desc.Height = 720;
	Desc.bBorderless = false;   // 제목 표시줄과 테두리가 있는 일반 창
	Desc.SyncInterval = 0;
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = true;  // 창 모드면 닫기 버튼이 있으니 false로 해도 됨
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init()) 
		return false;

	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Game);
	InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Game, true));
	GameWorld = InitialWorldContext.World();

	// 액터는 SpawnActor로 BeginPlay 대기열에 들어가 첫 World Tick에서 BeginPlay가 불린다.
	LoadStartScene();

	// 뷰포트 연결과 입력 설정이 이 카메라에 적용되므로 그 전에 정한다.
	SetupMainCamera();

	// Skybox·Fog·FXAA·Gamma는 Editor와 공용인 파이프라인이 소유한다.
	ScenePipeline.Init(GetEngineLoop().GetRenderer());

	// 게임 화면은 창 전체를 덮는 뷰포트 하나다. 창은 PreInit에서 이미 만들어져 크기가 정해져 있다.
	Viewport.BeginPlay(MakeUnique<FGameViewportClient>(*GameWorld));
	Viewport.SetOutputSize(GetEngineLoop().GetViewportWidth(), GetEngineLoop().GetViewportHeight());
	// BeginPlay는 입력을 끈 채로 시작하므로 게임 카메라가 입력을 받도록 켠다.
	Viewport.GetGameClient()->SetInputEnabled(true);

	return true;
}

// Config/Project.json의 StartScene을 불러온다.
// 설정이나 씬이 없어도 게임은 빈 월드로 뜨게 하고, 원인은 경고 로그로 남긴다.
void UGameEngine::LoadStartScene()
{
	FProjectSettings Settings;
	if (!Settings.Load(FProjectSettings::DefaultPath))
	{
		HTR_LOG(Warning, "[Game] starting with an empty world");
		return;
	}
	if (Settings.StartScene.empty())
	{
		HTR_LOG(Warning, "[Game] StartScene is not set in {}; starting with an empty world", FProjectSettings::DefaultPath);
		return;
	}
	if (!FJsonArchive::LoadWorld(GameWorld, FProjectSettings::ToNativePath(Settings.StartScene)))
	{
		HTR_LOG(Warning, "[Game] failed to load start scene {}; starting with an empty world", Settings.StartScene);
		return;
	}
	HTR_LOG(Info, "[Game] start scene loaded: {}", Settings.StartScene);
}

// 씬에 배치된 첫 ACameraActor를 메인 카메라로 지정한다. PIE 시작과 같은 규칙이다.
// 배치된 카메라가 없으면 UWorld::CreateMainCamera가 만든 기본 카메라를 그대로 쓴다.
void UGameEngine::SetupMainCamera()
{
	for (AActor* Actor : GameWorld->GetPersistentLevel()->GetActors())
	{
		if (ACameraActor* Camera = Cast<ACameraActor>(Actor))
		{
			GameWorld->SetMainCamera(Camera);
			return;
		}
	}
	HTR_LOG(Info, "[Game] no placed camera in scene; using default main camera");
}

// FEngineLoop가 스왑체인을 조정한 뒤 새 창 크기로 호출한다. 0 크기(최소화)에는 호출되지 않는다.
void UGameEngine::OnResize(uint32 Width, uint32 Height)
{
	Viewport.SetOutputSize(Width, Height);
}

// UObject(World) 일괄 삭제 전에 뷰포트의 게임 클라이언트를 놓는다.
// 클라이언트가 World의 카메라를 빌려 쓰므로, World가 먼저 사라지면 뷰포트 소멸자가 해제된 메모리를 건드린다.
void UGameEngine::PreExit()
{
	Viewport.EndPlay();
	SceneRenderer.EndFrame();
	ReleaseWorldShadowResources(GameWorld);
}

void UGameEngine::ReleaseWorldShadowResources(UWorld* World)
{
	if (!World) return;
	FWorldContext* Context = GetWorldContextFromWorld(World);
	if (!Context) return;
	// CPU의 비소유 참조를 먼저 끊고 GPU 자원 슬롯을 반환한다.
	SceneRenderer.ReleaseWorldShadows(Context->ShadowResourceIndex);
	GetEngineLoop().GetRenderer()->ReleaseShadowResources(Context->ShadowResourceIndex);
	Context->ShadowResourceIndex = InvalidShadowResourceIndex;
}

void UGameEngine::Tick(float DeltaTime)
{
	UpdateGameInput();
	GameWorld->Tick(DeltaTime);
	//TODO: LUA가 들어갈자리?

	GameWorld->GetScene().UpdateAllTransforms();
	RenderViewports();

}

void UGameEngine::UpdateGameInput()
{

}

void UGameEngine::RenderViewports()
{
	SceneRenderer.BeginFrame();
	// TODO: 그림자 컬링 설정을 엔진 멤버로 두고 넘긴다. 지금은 FSceneRenderer 기본값을 쓴다.
	//SceneRenderer.SetShadowCullingSettings()

	//TODO: 지금은 한 개의 Viewport만 가지고 Game이 돌아가게 만들었음
	
	FViewportClient* Client = Viewport.GetActiveClient();
	Viewport.ClearSceneData();
	if (!Viewport.IsVisible() || !Client || !Client->HasCamera())
	{
		SceneRenderer.EndFrame();
		return;
	}


	Viewport.EnsureRenderTargets();
	UWorld* ViewWorld = Client->GetWorld();
	const FSceneView View = Viewport.CreateSceneView();
	// 쉐도우 맵은 World의 첫 View를 그릴 때 준비되므로, 그 전에 그림자를 쓰는 View를 등록한다.
	if (ScenePipeline.ViewSupportsShadows(View, FSceneRenderSettings{}))
		SceneRenderer.AddShadowView(*ViewWorld, View);
	FSceneRenderData& Data = Viewport.GetSceneRenderData();
	SceneRenderer.PrepareView(*ViewWorld, View, Data);
	// 파티클/빌보드 상수가 다음 수집으로 바뀌기 전에 이 화면을 소비한다.
	RenderFrame(Viewport, *ViewWorld, View);

	Data.ReleaseSceneReferences();
	CopyViewportToBackbuffer();


	SceneRenderer.EndFrame();
}

void UGameEngine::RenderFrame(FViewport& InViewport, UWorld& ViewWorld, const FSceneView& View)
{
	const FSceneRenderSettings Settings{};   // TODO: 렌더 설정을 엔진 멤버로

	// 그림자 자원 번호는 World 컨텍스트에 보관하고 처음 쓸 때 할당한다.
	const bool bShadows = ScenePipeline.ViewSupportsShadows(View, Settings);
	uint32 ShadowResourceIndex = InvalidShadowResourceIndex;
	if (bShadows)
	{
		if (FWorldContext* Context = GetWorldContextFromWorld(&ViewWorld))
		{
			if (Context->ShadowResourceIndex == InvalidShadowResourceIndex)
				Context->ShadowResourceIndex = GetEngineLoop().GetRenderer()->AllocateShadowResources(false);
			ShadowResourceIndex = Context->ShadowResourceIndex;
		}
	}

	// 1. 그림자,라이팅,불투명,반투명 
	FShadowFrameData DirectionalFrame;
	ScenePipeline.RenderScene(InViewport, ViewWorld, View, SceneRenderer, ShadowResourceIndex, bShadows, Settings, DirectionalFrame);
	// 2. 후처리: Fog -> Gamma -> FXAA (표시용 sRGB)
	ScenePipeline.RenderPostProcess(InViewport, ViewWorld, View, Settings);
	// TODO: HUD


}

void UGameEngine::CopyViewportToBackbuffer()
{
	const FRenderingInfo& BackbufferInfo = GetEngineLoop().GetSwapchain()->GetRenderingInfo();
	if (BackbufferInfo.ColorRenderTargets.Num() == 0)
		return;

	// 백버퍼는 SRV가 없어 후처리 입력으로 쓸 수 없으므로 오프스크린 결과를 통째로 복사한다.
	// 창 크기 변경 직후 한 프레임은 크기가 어긋날 수 있어 그때는 건너뛴다.
	FTexture2D* Backbuffer = BackbufferInfo.ColorRenderTargets[0].Texture;
	if (!Backbuffer || !Viewport.GetColorTarget() ||
		BackbufferInfo.ViewportSetting.Width != Viewport.GetRenderingInfo().ViewportSetting.Width ||
		BackbufferInfo.ViewportSetting.Height != Viewport.GetRenderingInfo().ViewportSetting.Height)
		return;

	RenderCommand::CopyTexture(Backbuffer, Viewport.GetColorTarget());
}

