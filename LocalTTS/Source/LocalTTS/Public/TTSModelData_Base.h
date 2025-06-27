// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LocalTTSTypes.h"
#include "TTSModelData_Base.generated.h"

// Nested array of IDs to token-to-IDs map
USTRUCT(BlueprintType)
struct FTokensArrayWrapper
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokens Array")
	TArray<int32> Tokens;

	FTokensArrayWrapper() {}
	FTokensArrayWrapper(const TArray<int32>& InTokens)
	{
		Tokens = InTokens;
	}
};

/**
 * Practically virtual asset to describe ONNX model settings and provide tokenization.
 * Should create child class for each type of TTS
 */
UCLASS()
class LOCALTTS_API UTTSModelData_Base : public UDataAsset
{
	GENERATED_BODY()

public:
	UTTSModelData_Base();

	// Speakers (voices) supported by this model, if it supports more than one, with corresponding IDs (speaker tokens)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Model")
	TMap<FString, int32> Speakers;

	// Language code used in eSpeak phonemizer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="eSpeak Language Code"), Category = "Model")
	FString ESpeakVoiceCode;

	// General language code
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	FString LanguageCode;

	// General language family
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	FString LanguageFamily;

	// Sample rate used by the model this asset describes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	int32 SampleRate = 22050;

	// Num channels (it's always mono)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Model")
	int32 Channels = 1;

	// Extra silence to add between sentences
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	float SentenceSilenceSeconds = 0.f;

	// Speaking speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	float Speed = 1.f;

	// It's internal, used to predict output audio buffer size by phonemes number
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Model")
	float BaseSynthesisSpeedMultiplier = 1.f;

	// Neural net vocabulary
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	TMap<FString, FTokensArrayWrapper> TokenToId;

	// eSpeak phonemization is supported only in non-commercial version (due to GPL-3.0 limitations), and NN/dicrionaries is supported only in the Fab version
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	ETTSPhonemeType PhonemizationType;

	// Universal function to generate phonemes splitted by sentences
	virtual bool PhonemizeText(const FString& InText, FString& OutText, int32 SpeakerId, TArray<TArray<Piper::PhonemeUtf8>>& Phonemes, bool bCastCharactersAsWords = false);

	// Get phonemization code (usually eSpeakVoiceCode, but can be overriden for multilangual models)
	virtual FString GetEspeakCode(int32 SpeakerId) const;

	// Convert array of phonemes to tokens
	virtual bool Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast);

	// Called before RunSync to initialize model's input parameters
	virtual bool SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const;

	// Called after RunSync for audio normalization, if needed
	virtual void PostProcessNND(FSynthesisResult& SynthesisData) const {};

	// Import setting of this asset from file
	virtual void ImportFromFile(const FString& FileName) {};

	// Fill PhonemeIdMap from TokenToId
	void EnsurePhonemesMap();

protected:
	Piper::PhonemeUtf8 P_Period = U'.';      // CLAUSE_PERIOD
	Piper::PhonemeUtf8 P_Comma = U',';       // CLAUSE_COMMA
	Piper::PhonemeUtf8 P_Question = U'?';    // CLAUSE_QUESTION
	Piper::PhonemeUtf8 P_Exclamation = U'!'; // CLAUSE_EXCLAMATION
	Piper::PhonemeUtf8 P_Colon = U':';       // CLAUSE_COLON
	Piper::PhonemeUtf8 P_Semicolon = U';';   // CLAUSE_SEMICOLON
	Piper::PhonemeUtf8 P_Space = U' ';

	// Toknization map converted from Unreal to NN-readable format
	TMap<Piper::PhonemeUtf8, TArray<Piper::PhonemeId>> PhonemeIdMap;
};
