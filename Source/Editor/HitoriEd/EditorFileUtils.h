#pragma once

#include "Engine/World.h"

class FEditorFileUtils
{
public:
	static bool NewScene(UWorld* World);
	static bool SaveScene(UWorld* World);
	static bool SaveSceneAs(UWorld* World);
	static bool LoadScene(UWorld* World);

	// .lua 파일을 고르게 한다. 저장소 루트 기준 상대 경로를 돌려주고, 취소하면 빈 문자열.
	static FString OpenScriptFileDialog();

private:
	static FString OpenSaveSceneDialog();
	static FString OpenLoadSceneDialog();

	static FString CurrentScenePath;
};

