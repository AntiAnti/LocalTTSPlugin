// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LocalTTSTypes.h"
#include "LocalTTSFunctionLibrary.generated.h"

/**
 * Helper functions for LocalTTS subsystem
 */
UCLASS()
class LOCALTTS_API ULocalTTSFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// Save 16-bit audio buffer to .wav file
	UFUNCTION(BlueprintCallable, Category = "Local TTS")
	static void SaveAudioDataToFile(const TArray<uint8>& RawPCMData, int32 NumChannels, int32 SampleRate, FString FileName);

	// Save 32-bit audio buffer to .wav file
	static void SaveAudioData32ToFile(const Audio::FAlignedFloatBuffer& RawPCMData, int32 NumChannels, int32 SampleRate, FString FileName);

	// Get for the loaded model speaker ID corresponding to the speaker's name
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Speaker ID by Name"), Category = "Local TTS")
	static int32 GetModelSpeakerByName(const FNNMInstanceId& ModelID, const FString& SpeakerName);

	// Check if the model is loaded
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is TTS Model Loaded"), Category = "Local TTS")
	static bool IsTtsModelLoaded(const FNNMInstanceId& ModelID);

	// Get TTSModelData asset related to the loaded model by tag
	UFUNCTION(BlueprintPure, meta=(DisplayName="Get TTS Model Data Asset"), Category = "Local TTS")
	static class UTTSModelData_Base* GetTtsModelDataAsset(const FNNMInstanceId& ModelID);

	// Release from memory already loaded NNE model
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Release TTS Model"), Category = "Local TTS")
	static bool ReleaseTtsModel(const FNNMInstanceId& ModelID);
};

