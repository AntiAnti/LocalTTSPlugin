// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LocalTTSTypes.h"
#include "DictionaryArchive.h"
#include "Phonemizer.generated.h"

/**
 * Asset to phonemize text input using dictionaries and NNE G2P model
 * The model was fine-tuned for all voices supported by Piper.
 * Dataset built using open IPA dictionaries https://github.com/open-dict-data/ipa-dict
 * re-phonemized with espeak-ng https://github.com/espeak-ng/espeak-ng/
 * G2P model architecture: https://github.com/lingjzhu/CharsiuG2P/
 * 13 epoches, loss: 0.0107.
 */
UCLASS(BlueprintType)
class LOCALTTS_API UPhonemizer : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	// Phonemization dictionaries (still better the model, and of course faster)
	// To add a new dictionary use LoadDictionaryFromArchive
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS Phonemizer")
	TMap<FName, TSoftObjectPtr<class UDictionaryArchive>> Dictionaries;

	// Load NNE models and prepare to use phonemizer
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	void SyncLoadModel(TSoftObjectPtr<UNNEModelData> EncoderPtr, TSoftObjectPtr<UNNEModelData> DecoderPtr);

	// Phonemize text. Automatically uses dictionary for known words, if th dictionary was loaded and unzipped for the active language
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	void SyncPhonemizeText(const FString& Text, FString& PhonemizedText, TArray<FString>& OutWords, bool bCharactersAsWords);

	// Load dictionary asset by espeak language code ("en-us", "ru")
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	void PrepareDictionary(const FString& EspeakLanguageCode);

	// Set current language. Language code like: "eng-us" or "rus"
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	void SetLanguageCodeFormatRaw(const FString& InLanguageCode);

	// Set current language. Language code like: "en-us", "ru"...
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	bool SetLanguageCodeFormatEspeak(const FString& InLanguageCode, bool bUseDicrionary);

	// Add new phonemization dictionary
	UFUNCTION(BlueprintCallable, Category = "Phonemizer")
	void LoadDictionaryFromArchive(const FString& FileName, const FString& InLanguageCode);

	// Get active language code ("eng-us" or "rus")
	UFUNCTION(BlueprintPure, Category = "Phonemizer")
	FString GetLanguage() const;

protected:
	// G2P model: encoder
	FNNEModelTTS Encoder;
	// G2P model: decoder
	FNNEModelTTS Decoder;

	// Active language code
	FString LanguageCode;

	// Logits to tokens, not used anymore
	void ArgMax(const float* In, int32 Shape0, int32 Shape1, TArray<int64>& Out) const;

	TMap<FString, FString> EspeakToActual = {
		{TEXT("ar"), TEXT("ara")}, {TEXT("ca"), TEXT("cat")}, {TEXT("cs"), TEXT("cze")}, {TEXT("cy"), TEXT("wel-nw")}, {TEXT("da"), TEXT("dan")}, {TEXT("de"), TEXT("ger")}, {TEXT("el"), TEXT("gre")}, {TEXT("en-gb-x-rp"), TEXT("eng-uk")}, {TEXT("en-us"), TEXT("eng-us")}, {TEXT("es"), TEXT("spa")}, {TEXT("es-419"), TEXT("spa-me")}, {TEXT("fa"), TEXT("fas")}, {TEXT("fi"), TEXT("fin")}, {TEXT("fr"), TEXT("fra")}, {TEXT("fr-fr"), TEXT("fra")}, {TEXT("hu"), TEXT("hun")}, {TEXT("is"), TEXT("ice")}, {TEXT("it"), TEXT("ita")}, {TEXT("ka"), TEXT("geo")}, {TEXT("kk"), TEXT("kaz")}, {TEXT("lb"), TEXT("ltz")}, {TEXT("nl"), TEXT("dut")}, {TEXT("nb"), TEXT("nob")}, {TEXT("pl"), TEXT("pol")}, {TEXT("pt-br"), TEXT("por-bz")}, {TEXT("pt"), TEXT("por-po")}, {TEXT("ro"), TEXT("ron")}, {TEXT("ru"), TEXT("rus")}, {TEXT("sk"), TEXT("slo")}, {TEXT("sl"), TEXT("slv")}, {TEXT("sr"), TEXT("srp")}, {TEXT("sv"), TEXT("swe")}, {TEXT("sw"), TEXT("swa")}, {TEXT("tr"), TEXT("tur")}, {TEXT("uk"), TEXT("ukr")}, {TEXT("vi"), TEXT("vie-n")}, {TEXT("cmn"), TEXT("zho-s")}, {TEXT("zh"), TEXT("zho-s")}, {TEXT("j"), TEXT("jpn")}, {TEXT("ja"), TEXT("jpn")}, {TEXT("hi"), TEXT("hin")}
	};

	FLoadSoftObjectPathAsyncDelegate OnDictionaryLoaded;
};
