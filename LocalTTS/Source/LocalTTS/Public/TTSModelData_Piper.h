// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TTSModelData_Base.h"
#include "TTSModelData_Piper.generated.h"

/**
 * Object to tokenize text and prepare Piper model inputs
 * See https://github.com/rhasspy/piper
 */
UCLASS(Blueprintable, meta = (DisplayName = "TSS Model Data (Piper)"))
class LOCALTTS_API UTTSModelData_Piper : public UTTSModelData_Base
{
	GENERATED_BODY()
	
public:
	// Piper model settings: noise applied
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model|Piper")
	float NoiseScale = 0.667f;

	// Piper model settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model|Piper")
	float NoiseW = 0.8f;

	// Every other phoneme id is pad
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bInterspersePad = true;

	// Add beginning of sentence (bos) symbol at start
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bAddBos = true;

	// Add end of sentence (eos) symbol at end
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bAddEos = true;

	// UTTSModelData_Base implementation
	virtual FString GetEspeakCode(int32 SpeakerId) const override;
	virtual bool Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast) override;
	virtual bool SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const override;
	virtual void PostProcessNND(FSynthesisResult& SynthesisData) const override;
	virtual void ImportFromFile(const FString& FileName) override;
	// End UTTSModelData_Base implementation

protected:
	Piper::PhonemeUtf8 CharPad = U'_';
	Piper::PhonemeUtf8 CharBOS = U'^';
	Piper::PhonemeUtf8 CharEOS = U'$';
};
