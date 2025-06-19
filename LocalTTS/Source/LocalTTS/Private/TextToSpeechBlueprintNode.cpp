// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "TextToSpeechBlueprintNode.h"
#include "TTSSoundWaveRuntime.h"
#include "LocalTTSSubsystem.h"
#include "Engine/Engine.h"

ULocalTTSBlueprintNode* ULocalTTSBlueprintNode::TTS(const FNNMInstanceId& ModelID, const FString& Text, const FTTSGenerateSettings& Settings)
{
	ULocalTTSBlueprintNode* BlueprintNode = NewObject<ULocalTTSBlueprintNode>();
	BlueprintNode->SynthesisRequest.VoiceModelId = ModelID;
	BlueprintNode->SynthesisRequest.Text = Text;
	BlueprintNode->SynthesisRequest.Settings = Settings;
	return BlueprintNode;
}

void ULocalTTSBlueprintNode::OnTTSResult(USoundWave* SoundWaveAsset)
{
	SynthesisRequest.Callback.Clear();
	if (IsValid(SoundWaveAsset))
	{
		Succeed.Broadcast(Cast<UTTSSoundWaveRuntime>(SoundWaveAsset));
	}
	else
	{
		Failed.Broadcast(nullptr);
	}
}

void ULocalTTSBlueprintNode::Activate()
{
	SynthesisRequest.Callback.BindUFunction(this, TEXT("OnTTSResult"));
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	{
		LocalTTS->DoTextToSpeech(SynthesisRequest.VoiceModelId, SynthesisRequest.Text, SynthesisRequest.Settings, SynthesisRequest.Callback);
	}
}

