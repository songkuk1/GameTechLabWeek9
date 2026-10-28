#pragma once

#include "Camera/CameraShakePattern.h"

class UWaveOscillatorCameraShakePattern : public UCameraShakePattern
{
	DECLARE_CLASS(UWaveOscillatorCameraShakePattern, UCameraShakePattern)

public:
	void Configure(float InStrength, float InDuration, float InFrequency);
	void StartShakePattern() override;
	void UpdateShakePattern(float DeltaTime, FCameraShakePatternUpdateResult& OutResult) override;
	void StopShakePattern() override { bStopped = true; }
	bool IsFinished() const override { return bStopped || ElapsedTime >= Duration; }

private:
	float Strength = 0.1f;
	float Duration = 0.5f;
	float Frequency = 20.0f;
	float ElapsedTime = 0.0f;
	bool bStopped = true;
};
