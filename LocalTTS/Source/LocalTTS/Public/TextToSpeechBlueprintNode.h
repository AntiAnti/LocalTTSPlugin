// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LocalTTSSubsystem.h"
#include "TextToSpeechBlueprintNode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncTTSResult, UTTSSoundWaveRuntime*, TTSSoundWave);

/**
 * Blueprint async node to recognize audio data and create lip-sync in runtime
 */
UCLASS()
class LOCALTTS_API ULocalTTSBlueprintNode : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	virtual void Activate() override;

	/* Output Pin: request complete */
	UPROPERTY(BlueprintAssignable, Category = "Ynnk Recognize")
	FAsyncTTSResult Succeed;

	UPROPERTY(BlueprintAssignable, Category = "Ynnk Recognize")
	FAsyncTTSResult Failed;

	/** Convert 16-bit audio data to 32-bit audio data
	* @param AudioData					Uncompressed PCM data (bit rate: 16 bit) as byte array with
	*/
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName="Text to Speech (LocalTTS)"), Category = "Local TTS")
	static ULocalTTSBlueprintNode* TTS(const FNNMInstanceId& ModelID, const FString& Text, const FTTSGenerateSettings& Settings);

protected:

	UPROPERTY()
	FSynthesisQueue SynthesisRequest;

	UFUNCTION()
	void OnTTSResult(USoundWave* SoundWaveAsset);
};