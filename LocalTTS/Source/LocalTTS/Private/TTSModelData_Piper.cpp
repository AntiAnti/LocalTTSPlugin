// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "TTSModelData_Piper.h"
#include "NNE.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
//#include "HAL/FileManager.h"
#include "LocalTTSFunctionLibrary.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString UTTSModelData_Piper::GetEspeakCode(int32 SpeakerId) const
{
    return ESpeakVoiceCode;
}

bool UTTSModelData_Piper::Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast)
{
    // Update PhonemeIdMap if needed
    EnsurePhonemesMap();

    // Beginning of sentence symbol (^)
    if (bFirst && bAddBos && PhonemeIdMap.Contains(CharBOS))
    {
        const TArray<Piper::PhonemeId>& bosIds = PhonemeIdMap[CharBOS];
        OutTokens.Append(bosIds);

        if (bInterspersePad && PhonemeIdMap.Contains(CharPad))
        {
            // Pad after bos (_)
            const TArray<Piper::PhonemeId>& padIds = PhonemeIdMap[CharPad];
            OutTokens.Append(padIds);
        }
    }

    if (bInterspersePad && PhonemeIdMap.Contains(CharPad))
    {
        // Add ids for each phoneme *with* padding
        const TArray<Piper::PhonemeId>& padIds = PhonemeIdMap[CharPad];

        for (auto const phoneme : Phonemes)
        {
            if (!PhonemeIdMap.Contains(phoneme))
            {
                // Phoneme is missing from id map
                if (int32* MissedNum = OutMissedPhonemes.Find(phoneme))
                {
                    (*MissedNum)++;
                }
                else
                {
                    OutMissedPhonemes.Add(phoneme, 1);
                }
                continue;
            }

            // add mapped
            const TArray<Piper::PhonemeId>& mappedIds = PhonemeIdMap[phoneme];
            OutTokens.Append(mappedIds);
            // add pad (_)
            OutTokens.Append(padIds);
        }
    }
    else
    {
        // Add ids for each phoneme *without* padding, don't count missed
        for (auto const phoneme : Phonemes)
        {
            if (const TArray<Piper::PhonemeId>* mappedIds = PhonemeIdMap.Find(phoneme))
            {
                OutTokens.Append(*mappedIds);
            }
        }
    }

    // End of sentence symbol ($)
    if (bAddEos && bLast)
    {
        if (const TArray<Piper::PhonemeId>* eosIds = PhonemeIdMap.Find(CharEOS))
        {
            OutTokens.Append(*eosIds);
        }
    }

    return OutTokens.Num() > 0;
}

bool UTTSModelData_Piper::SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const
{
    // check parameters
    if (!NNModel.CheckInParam(0, ENNETensorDataType::Int64)
        || !NNModel.CheckInParam(1, ENNETensorDataType::Int64)
        || !NNModel.CheckInParam(2, ENNETensorDataType::Float))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid input tensor parameters at ONNX TTS model"));
        return false;
    }
    if (!Context.Tokens)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid input tokens"));
        return false;
    }

    int32 TokensNum = Context.Tokens->Num();

    // inputs: tokenized phonemes
    NNModel.PrepareInputInt64(0, *Context.Tokens, { 1, (uint32)TokensNum });
    // input_lengths
    NNModel.PrepareInputInt64(1, { TokensNum }, { 1 });
    // scale
    NNModel.PrepareInputFloat(2, { NoiseScale, Speed, NoiseW }, { 3 });

    // sID
    if (Speakers.Num() > 0)
    {
        int32 MinVal = Speakers.begin()->Value;
        int32 MaxVal = MinVal;
        for (const auto& s : Speakers)
        {
            MinVal = FMath::Min(MinVal, s.Value);
            MaxVal = FMath::Max(MaxVal, s.Value);
        }

        NNModel.PrepareInputInt64(3, { FMath::Clamp(Context.SpeakerId, MinVal, MaxVal) }, { 1 });
    }

    return true;
}

// Normalize and convert to 16bit
void UTTSModelData_Piper::PostProcessNND(FSynthesisResult& SynthesisData) const
{
    const float MAX_WAV_VALUE = 32767.0f;

    // Get max audio value for scaling
    float MaxAudioValue = 0.01f;
    for (const auto& sample : SynthesisData.PCMData32)
    {
        float AudioValue = abs(sample);
        if (AudioValue > MaxAudioValue)
        {
            MaxAudioValue = AudioValue;
        }
    }

    SynthesisData.PCMData16.SetNumUninitialized(SynthesisData.PCMData32.Num() * 2);
    int16* pcm16 = (int16*)SynthesisData.PCMData16.GetData();

    float AudioScale = (MAX_WAV_VALUE / FMath::Max(0.01f, MaxAudioValue));
    for (int32 i = 0; i < SynthesisData.PCMData32.Num(); i++)
    {
        pcm16[i] = (int16)FMath::TruncToInt(SynthesisData.PCMData32[i] * AudioScale);
    }
}

void UTTSModelData_Piper::ImportFromFile(const FString& FileName)
{
    FString JsonData;
    FFileHelper::LoadFileToString(JsonData, *FileName);

    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    TSharedPtr<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);

    if (!FJsonSerializer::Deserialize(Reader, JsonRootObject))
    {
        UE_LOG(LogTemp, Warning, TEXT("ImportFromFile: invalid json file %s"), *FileName);
        return;
    }

    if (!JsonRootObject->HasTypedField<EJson::Object>(TEXT("audio")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("espeak")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("inference")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("phoneme_id_map")) ||
        !JsonRootObject->HasTypedField<EJson::Number>(TEXT("num_speakers")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("speaker_id_map")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("language")))
    {
        UE_LOG(LogTemp, Warning, TEXT("ImportFromFile: invalid json file %s"), *FileName);
        return;
    }

    TSharedPtr<FJsonObject> AudioRoot = JsonRootObject->GetObjectField(TEXT("audio"));
    AudioRoot->TryGetNumberField(TEXT("sample_rate"), SampleRate);

    TSharedPtr<FJsonObject> EspeakRoot = JsonRootObject->GetObjectField(TEXT("espeak"));
    EspeakRoot->TryGetStringField(TEXT("voice"), ESpeakVoiceCode);

    TSharedPtr<FJsonObject> InferenceRoot = JsonRootObject->GetObjectField(TEXT("inference"));
    InferenceRoot->TryGetNumberField(TEXT("noise_scale"), NoiseScale);
    InferenceRoot->TryGetNumberField(TEXT("length_scale"), Speed);
    InferenceRoot->TryGetNumberField(TEXT("noise_w"), NoiseW);

    TSharedPtr<FJsonObject> LanguageRoot = JsonRootObject->GetObjectField(TEXT("language"));
    LanguageRoot->TryGetStringField(TEXT("code"), LanguageCode);
    LanguageRoot->TryGetStringField(TEXT("family"), LanguageFamily);

    int32 SpeakersNum;
    JsonRootObject->TryGetNumberField(TEXT("num_speakers"), SpeakersNum);
    Speakers.Empty();
    if (SpeakersNum > 1)
    {
        TSharedPtr<FJsonObject> SpeakersRoot = JsonRootObject->GetObjectField(TEXT("speaker_id_map"));
        for (const auto& SpeakerBinding : SpeakersRoot->Values)
        {
            if (SpeakerBinding.Value->Type == EJson::Number)
            {
                FString SpeakerName = SpeakerBinding.Key;
                Speakers.Add(SpeakerName, SpeakerBinding.Value->AsNumber());
            }
        }
    }

    TokenToId.Empty();
    TSharedPtr<FJsonObject> PhonemeMapRoot = JsonRootObject->GetObjectField(TEXT("phoneme_id_map"));
    for (const auto& PhonemeBinding : PhonemeMapRoot->Values)
    {
        if (PhonemeBinding.Value->Type == EJson::Array)
        {
            FString Phoneme = PhonemeBinding.Key;
            const auto TokensJson = PhonemeBinding.Value->AsArray();
            TArray<int32> Tokens;
            for (const auto& jToken : TokensJson)
            {
                int32 NewToken = jToken->AsNumber();
                Tokens.Add(NewToken);
            }

            TokenToId.Add(Phoneme, Tokens);
        }
    }
}
