// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LocalTTSSubsystem.h"
#include "LocalTTSTypes.h"
#include "LoadTTSModelBlueprintNode.generated.h"

class UNNEModelData;
class UTTSModelData_Base;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncTTSModelLoadResult, const FNNMInstanceId&, ModelID);

/**
 * Blueprint async node to recognize audio data and create lip-sync in runtime
 */
UCLASS()
class LOCALTTS_API ULoadTTSModelBlueprintNode : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	virtual void Activate() override;

	/* Output Pin: request complete */
	UPROPERTY(BlueprintAssignable, Category = "Ynnk Recognize")
	FAsyncTTSModelLoadResult Succeed;

	UPROPERTY(BlueprintAssignable, Category = "Ynnk Recognize")
	FAsyncTTSModelLoadResult Failed;

	/** Convert 16-bit audio data to 32-bit audio data
	* @param AudioData					Uncompressed PCM data (bit rate: 16 bit) as byte array with
	*/
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName="Load TTS Model"), Category = "Local TTS")
	static ULoadTTSModelBlueprintNode* LoadTTSModel(TSoftObjectPtr<UNNEModelData> TTSModelReferene, TSoftObjectPtr<UTTSModelData_Base> ModelDataReferene);

protected:

	TSoftObjectPtr<UNNEModelData> TTSModelPointer;
	TSoftObjectPtr<UTTSModelData_Base> TokenizerPointer;
	FLocalTTSStatusResponse Callback;

	UFUNCTION()
	void OnModelLoadResult(const FNNMInstanceId& ModelID, bool bSucceed);
};