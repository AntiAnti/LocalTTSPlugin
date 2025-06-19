// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LoadTTSModelBlueprintNode.h"
#include "LocalTTSSubsystem.h"
#include "NNEModelData.h"
#include "TTSModelData_Base.h"
#include "Engine/Engine.h"

ULoadTTSModelBlueprintNode* ULoadTTSModelBlueprintNode::LoadTTSModel(TSoftObjectPtr<UNNEModelData> TTSModelReferene, TSoftObjectPtr<UTTSModelData_Base> ModelDataReferene)
{
	ULoadTTSModelBlueprintNode* BlueprintNode = NewObject<ULoadTTSModelBlueprintNode>();
	BlueprintNode->TTSModelPointer = TTSModelReferene;
	BlueprintNode->TokenizerPointer = ModelDataReferene;
	return BlueprintNode;
}

void ULoadTTSModelBlueprintNode::OnModelLoadResult(const FNNMInstanceId& ModelID, bool bSucceed)
{
	Callback.Clear();
	if (bSucceed)
	{
		Succeed.Broadcast(ModelID);
	}
	else
	{
		Failed.Broadcast(INDEX_NONE);
	}
}

void ULoadTTSModelBlueprintNode::Activate()
{
	Callback.BindUFunction(this, TEXT("OnModelLoadResult"));
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	{
		LocalTTS->LoadModelTTS(TTSModelPointer, TokenizerPointer, Callback);
	}
}

