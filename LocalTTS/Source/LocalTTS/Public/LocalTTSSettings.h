// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "LocalTTSSettings.generated.h"

/**
* Global audio synthesis settings
*/
UCLASS(config = Engine, defaultconfig)
class LOCALTTS_API UTtsSettings : public UObject
{
	GENERATED_BODY()

public:
	UTtsSettings();

	// For any loaded model, should resample generated audio to TargetSampleRate?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	bool bResampleSynthesizedAudio = true;

	// Any generated audio will be resampled to this value, if ResampleSynthesizedAudio is checked
	UPROPERTY(GlobalConfig, EditAnywhere, meta=(EditCondition=bResampleSynthesizedAudio), Category = "Synthesis")
	int32 TargetSampleRate = 44100;

	// Save generated audio to [project dir]/Saved/CacheTTS
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	bool bSaveCachedWav = false;

	// Init espeak tokenizer when starting UE
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	bool bAutoInitializeOnStartup = true;

	// ONNX encoder of the G2P NNE model
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	TSoftObjectPtr<class UNNEModelData> PhonemizerEncoder;

	// ONNX decoder of the G2P NNE model
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	TSoftObjectPtr<class UNNEModelData> PhonemizerDecoder;

	// Phonemizer asset
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Synthesis")
	TSoftObjectPtr<class UPhonemizer> PhonemizerInfo;

	static const UTtsSettings* Get();
};