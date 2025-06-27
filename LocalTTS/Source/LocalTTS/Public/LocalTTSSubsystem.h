// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "HAL/CriticalSection.h"
#include "Containers/Queue.h"
#include "LocalTTSTypes.h"
#include "Containers/Ticker.h"
#include "LocalTTSSubsystem.generated.h"

class UNNEModelData;
class UTTSModelData_Base;

DECLARE_DYNAMIC_DELEGATE_TwoParams(FLocalTTSStatusResponse, const FNNMInstanceId&, ModelID, bool, bSucceed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLocalTTSSynthesized, const FNNMInstanceId&, ModelID, USoundWave*, SoundWaveAsset);
DECLARE_DYNAMIC_DELEGATE_OneParam(FLocalTTSSynthesisResponse, USoundWave*, SoundWaveAsset);

#if WITH_EDITOR
#ifndef LOCALTTSUTILS
#define LOCALTTSUTILS
namespace LocalTtsUtils
{
	template<typename ElemType>
	LOCALTTS_API FString PrintArray(const TArray<ElemType>& InData)
	{
		FString s;
		for (const auto& d : InData)
		{
			if (!s.IsEmpty())
			{
				s.Append(TEXT(", "));
			}

			if (typeid(d) == typeid(float))
			{
				s.Append(FString::SanitizeFloat(d));
			}
			else
			{
				s.Append(FString::FromInt(d));
			}
		}

		return s;
	}

	template<typename ElemType>
	LOCALTTS_API FString PrintArray(const TArrayView<ElemType>& InData)
	{
		FString s;
		for (const auto& d : InData)
		{
			if (!s.IsEmpty())
			{
				s.Append(TEXT(", "));
			}

			if (typeid(d) == typeid(float))
			{
				s.Append(FString::SanitizeFloat(d));
			}
			else
			{
				s.Append(FString::FromInt(d));
			}
		}

		return s;
	}
}
#endif
#endif

// Queue
USTRUCT()
struct FSynthesisQueue
{
	GENERATED_BODY()

	// Model Tag
	UPROPERTY()
	FNNMInstanceId VoiceModelId;
	// Text to generate
	UPROPERTY()
	FString Text;
	// Non-static generation settings
	UPROPERTY()
	FTTSGenerateSettings Settings;
	// Callback function with result
	UPROPERTY()
	FLocalTTSSynthesisResponse Callback;
};

/**
* Core TTS subsystem to store loaded NN models and process audio generation
*/
UCLASS()
class LOCALTTS_API ULocalTTSSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Notifies new audio was generated
	UPROPERTY(BlueprintAssignable, Category = "Local TTS")
	FLocalTTSSynthesized OnGenerationResult;

	// Init phonemizer backend
	UFUNCTION(BlueprintCallable, Category = "Local TTS")
	void InitializePhonemizer();

	// Load TTS model from ONNX asset and corresponding TTSModelData asset
	UFUNCTION()
	void LoadModelTTS(TSoftObjectPtr<UNNEModelData> TTSModelReferene, TSoftObjectPtr<UTTSModelData_Base> TokenizerReferene, const FLocalTTSStatusResponse& OnLoadingComplete);

	// Generate new audio from text for the specified model or add request to the queue
	UFUNCTION()
	void DoTextToSpeech(const FNNMInstanceId& VoiceModelId, const FString& Text, const FTTSGenerateSettings& Settings, const FLocalTTSSynthesisResponse& OnResult);

	// Actualy does TTS generation
	UFUNCTION()
	void Inference();

	// Get internal struct describing loaded NNE model
	const FNNEModelTTS* GetVoiceModel(const FNNMInstanceId& ModelID) const;

	// Check if the model is loaded
	UFUNCTION()
	bool IsVoiceModelValid(const FNNMInstanceId& ModelTag) const;

	// Get TTSModelData asset related to the loaded model by tag
	UFUNCTION()
	UTTSModelData_Base* GetModelDataAsset(const FNNMInstanceId& ModelTag) const;

	UFUNCTION()
	bool ReleaseModel(const FNNMInstanceId& ModelTag);

	inline class UPhonemizer* GetPhonemizer() const { return Phonemizer; }

protected:
	TMap<int32, FNNEModelTTS> VoiceModels;

	TObjectPtr<class UPhonemizer> Phonemizer;

	static FCriticalSection OnnxLoadMutex;
	int32 OutputDataBufferSize = 32768*2;
	bool bEspeakStatus = false;

	FTSTicker::FDelegateHandle TickDelegateHandle;

	// Loading
	bool bIsLoading = false;
	FNNMInstanceId LastAddedModelTag;
	FLocalTTSStatusResponse OnLastLoadCallback;
	// Generation
	bool bIsWorking = false;
	TQueue<FSynthesisQueue> RequestsQueue;
	FSynthesisQueue ActiveRequest;
	// Synthesis result
	FSynthesisResult SynthResult;

	UFUNCTION()
	bool StartupDelayedInitialize_Internal(float DeltaTime);

	void OnModelLoadingComplete_Internal(bool bResult);
	void OnGenerationComplete_Internal(bool bResult);
	int32 PredictOutputBufferSize(int32 TokensNum, const FNNEModelTTS& Model) const;

	void Cleanup();
};

