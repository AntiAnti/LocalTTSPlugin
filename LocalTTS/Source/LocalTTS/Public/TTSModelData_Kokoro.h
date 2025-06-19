// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TTSModelData_Base.h"
#include "TTSModelData_Kokoro.generated.h"

/*
* Nested float array in another array
*/
USTRUCT(BlueprintType)
struct FTtsFloatArrayWrapper
{
	GENERATED_BODY()

	// Float array data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float Array")
	TArray<float> Data;
};

/**
 * Object to tokenize text and prepare Kokoro-TTS model inputs
 * https://huggingface.co/hexgrad/Kokoro-82M
 * https://github.com/hexgrad/kokoro
 * https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX <-- get here model and voices
 */
UCLASS(Blueprintable, meta = (DisplayName = "TSS Model Data (Kokoro)"))
class LOCALTTS_API UTTSModelData_Kokoro : public UTTSModelData_Base
{
	GENERATED_BODY()
	
public:
	UTTSModelData_Kokoro();

	// Every other phoneme id is pad
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bInterspersePad = false;

	// Add beginning of sentence (bos) symbol at start
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bAddBos = true;

	// Add end of sentence (eos) symbol at end
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tokenizer")
	bool bAddEos = true;

	// UTTSModelData_Base implementation
	virtual FString GetEspeakCode(int32 SpeakerId) const override;
	virtual bool PhonemizeText(const FString& InText, FString& OutText, int32 SpeakerId, TArray<TArray<Piper::PhonemeUtf8>>& Phonemes) override;
	virtual bool Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast) override;
	virtual bool SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const override;
	virtual void PostProcessNND(FSynthesisResult& SynthesisData) const override;
	virtual void ImportFromFile(const FString& FileName) override;
	// End UTTSModelData_Base implementation

	// Import voice binary from file
	void ImportVoiceFromFile(const FString& FileName);
	// Delete voice
	void DeleteSpeaker(int32 SpeakerID);
	void DeleteAllSpeakers();

protected:
	UPROPERTY()
	FString TokenizePatternRegex;
	UPROPERTY()
	bool bTokenizePatternTypeReplace = true;
	// Key is speakerId
	UPROPERTY()
	TArray<FTtsFloatArrayWrapper> VoiceCache;
	// eSpeak codes for imported voices, Key is speakerId
	UPROPERTY()
	TArray<FString> VoiceEspeakCodes;

	Piper::PhonemeUtf8 CharPad = U'$';
	Piper::PhonemeUtf8 CharBOS = U'$';
	Piper::PhonemeUtf8 CharEOS = U'$';
};
