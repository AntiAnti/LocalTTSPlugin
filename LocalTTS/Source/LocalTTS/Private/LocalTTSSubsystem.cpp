// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSSubsystem.h"
#include "LocalTTSModule.h"
#include "HAL/CriticalSection.h"
#include "Containers/StringConv.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "LocalTTSTypes.h"
#include "TTSModelData_Base.h"
#include "LocalTTSFunctionLibrary.h"
#include "DSP/AlignedBuffer.h"
#include "AudioResampler.h"
#include "TTSSoundWaveRuntime.h"
#include "LocalTTSSettings.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "LocalTTSModule.h"
#include "Phonemizer.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"

//#include <espeak-ng/speak_lib.h>

FCriticalSection ULocalTTSSubsystem::OnnxLoadMutex;

/*
#if WITH_EDITOR
template<typename ElemType>
FString LocalTtsUtils::PrintArray(const TArray<ElemType>& InData)
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
FString LocalTtsUtils::PrintArray(const TArrayView<ElemType>& InData)
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
#endif
*/

void ULocalTTSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	const UTtsSettings* Settings = UTtsSettings::Get();
	if (Settings->bAutoInitializeOnStartup)
	{
		//TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULocalTTSSubsystem::StartupDelayedInitialize_Internal), 0.5f);
		StartupDelayedInitialize_Internal(0.f);
	}
}

void ULocalTTSSubsystem::Deinitialize()
{
    Super::Deinitialize();
	Cleanup();
}

void ULocalTTSSubsystem::InitializePhonemizer()
{
	StartupDelayedInitialize_Internal(0.f);
}

void ULocalTTSSubsystem::LoadModelTTS(TSoftObjectPtr<UNNEModelData> TTSModelReferene, TSoftObjectPtr<UTTSModelData_Base> TokenizerReferene, const FLocalTTSStatusResponse& OnLoadingComplete)
{
	if (bIsLoading)
	{
		OnLoadingComplete.ExecuteIfBound(INDEX_NONE, false);
		return;
	}

	if (TTSModelReferene.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("Model Reference is not set, please assign it in the editor"));
		OnLoadingComplete.ExecuteIfBound(INDEX_NONE, false);
		return;
	}
	else
	{
		if (TokenizerReferene.IsNull())
		{
			UE_LOG(LogTemp, Error, TEXT("Tokenizer/Model data is invalid"));

			OnLoadingComplete.ExecuteIfBound(INDEX_NONE, false);
			return;
		}
		
		FString ModelAssetName = TTSModelReferene.GetAssetName();
		for (const auto& ExistingModel : VoiceModels)
		{
			if (ExistingModel.Value.ModelAssetName == ModelAssetName && ExistingModel.Value.bLoaded)
			{
				// already loaded
				UE_LOG(LogTemp, Log, TEXT("Model is already loaded"));

				OnLoadingComplete.ExecuteIfBound(ExistingModel.Key, true);
				return;
			}
		}

		bIsLoading = true;
		OnLastLoadCallback = OnLoadingComplete;
		LastAddedModelTag = INDEX_NONE;

		FNNMInstanceId NewId = VoiceModels.Num();
		VoiceModels.Add(NewId.Id);
		VoiceModels[NewId.Id].ModelAssetName = ModelAssetName;
		VoiceModels[NewId.Id].VoiceDesc = TokenizerReferene.LoadSynchronous();

		const auto Delegate = FStreamableDelegate::CreateLambda([ModelReferene = TTSModelReferene, ModelId = NewId.Id, this]()
		{
			if (!ModelReferene.IsValid())
			{
				LastAddedModelTag = ModelId;
				UE_LOG(LogTemp, Log, TEXT("Couldn't load TTS model from soft pointer %s"), *ModelReferene.GetLongPackageName());
				OnModelLoadingComplete_Internal(false);
				return;
			}
			const FName ModelName = *ModelReferene.GetAssetName();

			AsyncTask(ENamedThreads::AnyThread, [this, ModelReferene, ModelId]() mutable
			{
				OnnxLoadMutex.TryLock();
				LastAddedModelTag = ModelId;

				auto& ModelData = VoiceModels[ModelId];
				UNNEModelData* ModelAsset = ModelReferene.Get();
				bool bResult = ULocalTTSFunctionLibrary::LoadNNM(ModelData, ModelAsset, OutputDataBufferSize, TEXT("TTSModel"));

				AsyncTask(ENamedThreads::GameThread, [this, bResult]()
				{
					OnnxLoadMutex.Unlock();
					OnModelLoadingComplete_Internal(bResult);
				});
			});
		});

		if (UAssetManager::IsInitialized())
		{
			UAssetManager::GetStreamableManager().RequestAsyncLoad(TTSModelReferene.ToSoftObjectPath(), Delegate);
		}
	}
}

void ULocalTTSSubsystem::DoTextToSpeech(const FNNMInstanceId& VoiceModelId, const FString& Text, const FTTSGenerateSettings& Settings, const FLocalTTSSynthesisResponse& OnResult)
{
	if (RequestsQueue.IsEmpty() && !bIsWorking)
	{
		ActiveRequest.VoiceModelId = VoiceModelId;
		ActiveRequest.Text = Text;
		ActiveRequest.Settings = Settings;
		ActiveRequest.Callback = OnResult;
		Inference();
	}
	else
	{
		FSynthesisQueue r;
		r.VoiceModelId = VoiceModelId;
		r.Text = Text;
		r.Settings = Settings;
		r.Callback = OnResult;
		RequestsQueue.Enqueue(r);
	}
}

int32 ULocalTTSSubsystem::PredictOutputBufferSize(int32 TokensNum, const FNNEModelTTS& Model) const
{
	int32 val = 
		(int32)(
			(float)(TokensNum * (int32)900 * (Model.VoiceDesc->SampleRate / (int32)22050)) *
			(Model.VoiceDesc->BaseSynthesisSpeedMultiplier * Model.VoiceDesc->Speed)
			);

	const int32 minval = OutputDataBufferSize * 2;
	return val > minval ? val : minval;
}

void ULocalTTSSubsystem::Inference()
{
	if (!VoiceModels.Contains(ActiveRequest.VoiceModelId.Id))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid ModelTag. Model not found."));
		OnGenerationComplete_Internal(false);
		return;
	}
	if (ActiveRequest.Text.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Text is empty. Skipping."));
		OnGenerationComplete_Internal(false);
		return;
	}

	bIsWorking = true;

	// Current voice vodel
	auto& VModel = VoiceModels[ActiveRequest.VoiceModelId.Id];
	SynthResult.Reset(ActiveRequest.VoiceModelId);
	SynthResult.SampleRate = VModel.VoiceDesc->SampleRate;

	AsyncTask(ENamedThreads::AnyThread, [this]() mutable
	{
		// Current voice vodel
		auto& VModel = VoiceModels[SynthResult.ModelTag.Id];
			
		// Convert text to arrays of phonemes separated by sentences
		FString PhonemizedText;
		if (!VModel.VoiceDesc->PhonemizeText(ActiveRequest.Text, PhonemizedText, ActiveRequest.Settings.SpeakerId, SynthResult.PhonemePhrases))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to phonemize text: %s"), *ActiveRequest.Text);
			OnGenerationComplete_Internal(false);
			return;
		}

		// Prepare memory for 32bit PCM buffer
		int32 TotalPhonemeCount = 0;
		for (const auto& PhonemesInPhrase : SynthResult.PhonemePhrases)
		{
			TotalPhonemeCount += PhonemesInPhrase.Num();
		}
		UE_LOG(LogTemp, Log, TEXT("Phonemized Text: [%s] (%d symbols in total)"), *PhonemizedText, TotalPhonemeCount);
		
		int32 SentenceSilenceSamples = (int32)(VModel.VoiceDesc->SentenceSilenceSeconds * (float)VModel.VoiceDesc->SampleRate /* * channel num */);
		SynthResult.PCMData32.Reserve(PredictOutputBufferSize(TotalPhonemeCount, VModel));

		int32 SentenceIndex = -1;
		for (const auto& PhonemesInPhrase : SynthResult.PhonemePhrases)
		{
			TArray<Piper::PhonemeId> Tokens;
			TMap<Piper::PhonemeUtf8, int32> MissedPhonemes;
			SentenceIndex++;

			// 1. Tokenize sentence
			if (!VModel.VoiceDesc->Tokenize(PhonemesInPhrase, Tokens, MissedPhonemes,
				/* bFirst */ SentenceIndex == 0,
				/* bLast */  SentenceIndex == SynthResult.PhonemePhrases.Num() - 1)
				)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to tokenize phonemes."));
				OnGenerationComplete_Internal(false);
				return;
			}
			if (Tokens.IsEmpty())
			{
				UE_LOG(LogTemp, Log, TEXT("Failed to tokenize"));
				continue;
			}
			if (MissedPhonemes.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Couldn't tokenize %d phonemes! Result will be inaccurate."), MissedPhonemes.Num());
			}

#if WITH_EDITOR
			FString TokensStr = LocalTtsUtils::PrintArray(Tokens);
			UE_LOG(LogTemp, Log, TEXT("Tokenized data: %s (%d total)"), *TokensStr, Tokens.Num());
#endif

			// 2. Set NN inputs
			FTTSGenerateRequestContext PrepareContext;
			PrepareContext.SpeakerId = ActiveRequest.Settings.SpeakerId;
			PrepareContext.Tokens = &Tokens;
			if (!VModel.VoiceDesc->SetNNEInputParams(VModel, PrepareContext))
			{
				UE_LOG(LogTemp, Warning, TEXT("Unable to prepare NNM inputs."));
				OnGenerationComplete_Internal(false);
				return;
			}

			// 3. Prepare NN output buffer
			// usually we get about 600 samples per token for 22,050 Hz, but need some reserve for safety
			int32 ExpectedOutputSize = PredictOutputBufferSize(Tokens.Num(), VModel);
			if (VModel.OutputData.Num() < ExpectedOutputSize)
			{
				VModel.OutputData.SetNumUninitialized(ExpectedOutputSize);
				UE_LOG(LogTemp, Log, TEXT("Expanding output buffer to %d float samples"), ExpectedOutputSize);
			}
			VModel.OutputBindings[0].Data = VModel.OutputData.GetData();
			VModel.OutputBindings[0].SizeInBytes = VModel.OutputData.Num() * sizeof(float);

			// 4. Interfere current phrase (sentence)
			TArray<float> TTSOutputs;
			TArray<uint32> TTSOutputsShape;
			if (!VModel.RunNNE(TTSOutputs, TTSOutputsShape, false)) // no need to copy to TTSOutputs
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to run NNE."));
				OnGenerationComplete_Internal(false);
				return;
			}

			// 5. Read output
			int32 GeneratedSamplesNum = VModel.ModelInstance->GetOutputTensorShapes().GetData()->Volume();

			// Push output into PCMData32 buffer
			if (GeneratedSamplesNum == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Nothing was generated for sentence %d of %d"), SentenceIndex + 1, SynthResult.PhonemePhrases.Num());
			}
			else if (GeneratedSamplesNum > VModel.OutputData.Num())
			{
				UE_LOG(LogTemp, Error, TEXT("NNE output buffer was too small (%d vs %d). Data is corrupted."), VModel.OutputData.Num(), GeneratedSamplesNum);
				OnGenerationComplete_Internal(false);
				return;
			}
			else if (GeneratedSamplesNum > 0)
			{
				SynthResult.AudioSeconds += (float)GeneratedSamplesNum / (float)VModel.VoiceDesc->SampleRate;
				UE_LOG(LogTemp, Log, TEXT("Total generated audio size: %f seconds"), SynthResult.AudioSeconds);
				int32 StartOffset = SynthResult.PCMData32.Num() * (int32)sizeof(float);
				SynthResult.PCMData32.AddUninitialized(GeneratedSamplesNum);
				FMemory::Memcpy((uint8*)SynthResult.PCMData32.GetData() + StartOffset, (const uint8*)VModel.OutputData.GetData(), GeneratedSamplesNum * (int32)sizeof(float));

				// Add pause at the end of each sentence
				if (SentenceSilenceSamples > 0 && SentenceIndex < SynthResult.PhonemePhrases.Num() - 1)
				{
					UE_LOG(LogTemp, Log, TEXT("Addign silence samples (%d) for %f seconds"), SentenceSilenceSamples, VModel.VoiceDesc->SentenceSilenceSeconds);
					SynthResult.AudioSeconds += VModel.VoiceDesc->SentenceSilenceSeconds;
					SynthResult.PCMData32.AddZeroed(SentenceSilenceSamples);
				}
			}
		}

		// Set to target sample rate
		const UTtsSettings* Settings = UTtsSettings::Get();
		if (Settings->bResampleSynthesizedAudio && Settings->TargetSampleRate != VModel.VoiceDesc->SampleRate)
		{
			Audio::FAlignedFloatBuffer ResampledPCMData;

			const Audio::FResamplingParameters ResampleParameters =
			{
				Audio::EResamplingMethod::Linear,
				1,
				static_cast<float>(VModel.VoiceDesc->SampleRate),
				static_cast<float>(Settings->TargetSampleRate),
				SynthResult.PCMData32
			};

			ResampledPCMData.AddUninitialized(Audio::GetOutputBufferSize(ResampleParameters));
			Audio::FResamplerResults ResampleResults;
			ResampleResults.OutBuffer = &ResampledPCMData;

			if (Audio::Resample(ResampleParameters, ResampleResults))
			{
				SynthResult.PCMData32 = MoveTemp(ResampledPCMData);
				SynthResult.SampleRate = Settings->TargetSampleRate;
			}	
		}

		// Custom postprocessing if needed (for piper: normalize volume)
		VModel.VoiceDesc->PostProcessNND(SynthResult);

		// Resample 32bit to 16bit if it didn't happen during post-processing
		if (SynthResult.PCMData16.IsEmpty())
		{
			int32 SamplesNum = SynthResult.PCMData32.Num();
			SynthResult.PCMData16.SetNumUninitialized(SamplesNum * 2);

			int16* pcm16 = (int16*)SynthResult.PCMData16.GetData();
			for (int32 i = 0; i < SamplesNum; i++)
			{
				pcm16[i] = (int16)FMath::TruncToInt(SynthResult.PCMData32[i] * 32768.0f);
			}
		}

		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			OnGenerationComplete_Internal(true);
		});
	});
}

const FNNEModelTTS* ULocalTTSSubsystem::GetVoiceModel(const FNNMInstanceId& ModelID) const
{
	return VoiceModels.Find(ModelID.Id);
}

bool ULocalTTSSubsystem::IsVoiceModelValid(const FNNMInstanceId& ModelID) const
{
	return VoiceModels.Contains(ModelID.Id) && VoiceModels[ModelID.Id].bLoaded;
}

UTTSModelData_Base* ULocalTTSSubsystem::GetModelDataAsset(const FNNMInstanceId& ModelID) const
{
	const FNNEModelTTS* Model = VoiceModels.Find(ModelID.Id);
	return Model ? Model->VoiceDesc : nullptr;
}

bool ULocalTTSSubsystem::ReleaseModel(const FNNMInstanceId& ModelTag)
{
	if (IsVoiceModelValid(ModelTag))
	{
		VoiceModels[ModelTag.Id].ModelInstance.Reset();
		VoiceModels.Remove(ModelTag.Id);
		return true;
	}
	return false;
}

bool ULocalTTSSubsystem::StartupDelayedInitialize_Internal(float DeltaTime)
{
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}

#if ESPEAK_NG
	FString ContentPath = FLocalTTSModule::GetContentPath() / TEXT("NonUFS/espeak-ng-data");
	UE_LOG(LogTemp, Log, TEXT("eSpeak data path: %s"), *ContentPath);
	if (PlatformFileUtils::DirectoryExists(ContentPath))
	{
		auto ModuleTts = FModuleManager::GetModulePtr<FLocalTTSModule>(TEXT("LocalTTS"));
		if (!ModuleTts->IsLoaded())
		{
			UE_LOG(LogTemp, Log, TEXT("eSpeak status: unexpected error."));
			return false;
		}

#if PLATFORM_ANDROID
		ContentPath = PlatformFileUtils::GetPlatformPath(ContentPath);
#endif

		int32 EspeakResult = ModuleTts->func_espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, TCHAR_TO_ANSI(*ContentPath), 0);
		UE_LOG(LogTemp, Log, TEXT("eSpeak initialization status: %d"), EspeakResult);
		bEspeakStatus = EspeakResult > 0;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("eSpeak status: can't find content"));
		bEspeakStatus = false;
	}

#else
	const UTtsSettings* Settings = UTtsSettings::Get();
	if (!Settings->PhonemizerInfo.IsNull())
	{
		Phonemizer = Settings->PhonemizerInfo.LoadSynchronous();
		if (IsValid(Phonemizer))
		{
			Phonemizer->SyncLoadModel(Settings->PhonemizerEncoder, Settings->PhonemizerDecoder);
			bEspeakStatus = true;
		}
	}
#endif

	return bEspeakStatus;
}

void ULocalTTSSubsystem::OnModelLoadingComplete_Internal(bool bResult)
{
	if (IsInGameThread())
	{
		bIsLoading = false;
		OnnxLoadMutex.Unlock();
		OnLastLoadCallback.ExecuteIfBound(LastAddedModelTag, bResult);
		
		// Try to load dictionary beforehand
		if (IsValid(Phonemizer))
		{
			if (VoiceModels.Contains(LastAddedModelTag.Id))
			{
				UTTSModelData_Base* ModelData = VoiceModels[LastAddedModelTag.Id].VoiceDesc;
				if (IsValid(ModelData) && ModelData->PhonemizationType == ETTSPhonemeType::PT_Dictionary)
				{
					Phonemizer->PrepareDictionary(ModelData->GetEspeakCode(0));
				}
			}
		}
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, bResult]()
		{
			bIsLoading = false;
			OnnxLoadMutex.Unlock();
			OnLastLoadCallback.ExecuteIfBound(LastAddedModelTag, bResult);
		});
	}
}

void ULocalTTSSubsystem::OnGenerationComplete_Internal(bool bResult)
{
	if (bResult)
	{
		auto& VModel = VoiceModels[SynthResult.ModelTag.Id];

		const UTtsSettings* Settings = UTtsSettings::Get();
		if (Settings->bSaveCachedWav)
		{
			FString Path = FPaths::ProjectDir() / TEXT("Saved") / TEXT("CacheTTS");
			if (!FPaths::DirectoryExists(Path))
			{
				IFileManager::Get().MakeDirectory(*Path);
			}
			FString StrDate = FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d-%H-%M-%S"));
			FString FileName = Path / TEXT("tts-") + StrDate + TEXT(".wav");
			ULocalTTSFunctionLibrary::SaveAudioDataToFile(SynthResult.PCMData16, 1, SynthResult.SampleRate, FileName);
		}

		UTTSSoundWaveRuntime* VoiceSoundWave = NewObject<UTTSSoundWaveRuntime>();
		if (IsValid(VoiceSoundWave))
		{
			VoiceSoundWave->bProcedural = true;
			VoiceSoundWave->bLooping = false;
			VoiceSoundWave->SetSampleRate(SynthResult.SampleRate);
			VoiceSoundWave->NumChannels = 1;
			VoiceSoundWave->InitializeAudio(SynthResult.PCMData16.GetData(), SynthResult.PCMData16.Num());
			ActiveRequest.Callback.ExecuteIfBound(VoiceSoundWave);
		}
		OnGenerationResult.Broadcast(SynthResult.ModelTag, VoiceSoundWave);

		VModel.OutputData.Empty();
		SynthResult.PCMData16.Empty();
		SynthResult.PCMData32.Empty();
	}
	else
	{
		ActiveRequest.Callback.ExecuteIfBound(nullptr);
	}

	bIsWorking = false;
	if (!RequestsQueue.IsEmpty())
	{
		RequestsQueue.Dequeue(ActiveRequest);
		Inference();
	}
}

void ULocalTTSSubsystem::Cleanup()
{
	if (bEspeakStatus)
	{
		auto ModuleTts = FModuleManager::GetModulePtr<FLocalTTSModule>(TEXT("LocalTTS"));
		if (ModuleTts->IsLoaded())
		{
			ModuleTts->func_espeak_Terminate();
		}
	}

	for (auto& Model : VoiceModels)
	{
		Model.Value.ModelInstance.Reset();
	}
	VoiceModels.Empty();
}
