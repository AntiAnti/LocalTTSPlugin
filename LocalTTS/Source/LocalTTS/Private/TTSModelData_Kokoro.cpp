// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "TTSModelData_Kokoro.h"
#include "NNE.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
//#include "HAL/FileManager.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UTTSModelData_Kokoro::UTTSModelData_Kokoro()
{
    SampleRate = 24000;
    BaseSynthesisSpeedMultiplier = 3.f;
    SentenceSilenceSeconds = 0.f;
}

FString UTTSModelData_Kokoro::GetEspeakCode(int32 SpeakerId) const
{
    if (VoiceEspeakCodes.IsValidIndex(SpeakerId))
    {
        return VoiceEspeakCodes[SpeakerId];
    }
    else
    {
        return ESpeakVoiceCode;
    }
}

bool UTTSModelData_Kokoro::PhonemizeText(const FString& InText, FString& OutText, int32 SpeakerId, TArray<TArray<Piper::PhonemeUtf8>>& Phonemes)
{
    const int32 MaxTokensInBatch = bInterspersePad ? 240 : 480;

    TArray<TArray<Piper::PhonemeUtf8>> TempBatches;
    bool bResult = Super::PhonemizeText(InText, OutText, SpeakerId, TempBatches);

    // Don't separate sentences, but keep tokens num below 510
    if (bResult)
    {
        TArray<Piper::PhonemeUtf8> CombinedBatches;

        for (const auto& Batch : TempBatches)
        {
            // Combine batches
            if (CombinedBatches.Num() + Batch.Num() > MaxTokensInBatch)
            {
                if (CombinedBatches.Num() > 0)
                {
                    Phonemes.Add(CombinedBatches);
                }
                CombinedBatches = Batch;
            }
            else
            {
                CombinedBatches.Append(Batch);
            }

            // Ensure we're within tokens limit
            while (CombinedBatches.Num() > MaxTokensInBatch)
            {
                Phonemes.AddDefaulted();
                Phonemes.Last().SetNumUninitialized(MaxTokensInBatch);
                FMemory::Memcpy(Phonemes.Last().GetData(), CombinedBatches.GetData(), MaxTokensInBatch * (int32)sizeof(Piper::PhonemeUtf8));

                TArray<Piper::PhonemeUtf8> tmp;
                tmp.SetNumUninitialized(CombinedBatches.Num() - MaxTokensInBatch);
                FMemory::Memcpy(tmp.GetData(), &CombinedBatches[MaxTokensInBatch], tmp.Num() * (int32)sizeof(Piper::PhonemeUtf8));
                CombinedBatches.SetNumUninitialized(tmp.Num());
                CombinedBatches = tmp;
            }
        }

        if (!CombinedBatches.IsEmpty())
        {
            Phonemes.Add(CombinedBatches);
            UE_LOG(LogTemp, Log, TEXT("UTTSModelData_Kokoro::PhonemizeText: Added batch of size [%d] to Phonemes"), CombinedBatches.Num());
        }
    }

    return bResult;
}

bool UTTSModelData_Kokoro::Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast)
{
    // Update PhonemeIdMap if needed
    EnsurePhonemesMap();

    // Beginning of sentence symbol (^)
    if (bAddBos && bFirst && PhonemeIdMap.Contains(CharBOS))
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

/*
* shape (1, -1)   type Int64			phonemized tokens
* shape (1, 256)  type Float			voice
* shape (1)       type Float			speed 1.0
*/
bool UTTSModelData_Kokoro::SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const
{
    // check parameters
    if (!NNModel.CheckInParam(0, ENNETensorDataType::Int64)
        || !NNModel.CheckInParam(1, ENNETensorDataType::Float)
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
    TArray<UE::NNE::FTensorShape> InputShapes;

    // inputs
    // shape (1, -1)   type Int64			phonemized tokens
    TArray<int64>& Inputs = NNModel.GetInParamIntUnsafe(0);
    Inputs.SetNumUninitialized(TokensNum);
    FMemory::Memcpy(Inputs.GetData(), Context.Tokens->GetData(), TokensNum * (int32)sizeof(int64));
    InputShapes.Add(UE::NNE::FTensorShape::Make({ 1, (uint32)Inputs.Num() }));
    NNModel.InputBindings[0].SizeInBytes = (uint64)Inputs.Num() * sizeof(int64);
    NNModel.InputBindings[0].Data = Inputs.GetData();

    // voice
    // shape (1, 256)  type Float			voice
    TArray<float>& Voice = NNModel.GetInParamFloatUnsafe(1);
    Voice.SetNumUninitialized(256);
    
    if (VoiceCache.IsValidIndex(Context.SpeakerId))
    {
        int32 BuffOffset = (TokensNum * 256) % VoiceCache[Context.SpeakerId].Data.Num();
        FMemory::Memcpy(Voice.GetData(), &VoiceCache[Context.SpeakerId].Data[BuffOffset], (uint64)256 * (uint64)sizeof(float));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid SpeakerId: %d"), Context.SpeakerId);
        return false;
    }

    InputShapes.Add(UE::NNE::FTensorShape::Make({ 1, 256 }));
    NNModel.InputBindings[1].SizeInBytes = (uint64)256 * (uint64)sizeof(float);
    NNModel.InputBindings[1].Data = Voice.GetData();

    // speed
    // shape(1)       type Float			speed 1.0
    TArray<float>& ScaleInp = NNModel.GetInParamFloatUnsafe(2);
    ScaleInp = { Speed };
    InputShapes.Add(UE::NNE::FTensorShape::Make({ 1 }));
    NNModel.InputBindings[2].SizeInBytes = sizeof(float);
    NNModel.InputBindings[2].Data = ScaleInp.GetData();

    NNModel.ModelInstance->SetInputTensorShapes(InputShapes);
    return true;
}

void UTTSModelData_Kokoro::PostProcessNND(FSynthesisResult& SynthesisData) const
{
    // Normalize and convert to 16bit

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

/*
* Expected: tokenizer.json
* from https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX
*/
void UTTSModelData_Kokoro::ImportFromFile(const FString& FileName)
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

    if (!JsonRootObject->HasTypedField<EJson::Object>(TEXT("normalizer")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("post_processor")) ||
        !JsonRootObject->HasTypedField<EJson::Object>(TEXT("model")))
    {
        UE_LOG(LogTemp, Warning, TEXT("ImportFromFile: invalid json file %s"), *FileName);
        return;
    }

    TSharedPtr<FJsonObject> NormalizerRoot = JsonRootObject->GetObjectField(TEXT("normalizer"));
    if (NormalizerRoot->HasTypedField<EJson::Object>(TEXT("pattern")))
    {
        TSharedPtr<FJsonObject> RegexPatternObj = NormalizerRoot->GetObjectField(TEXT("pattern"));
        RegexPatternObj->TryGetStringField(TEXT("Regex"), TokenizePatternRegex);
    }
    FString NormType;
    if (NormalizerRoot->TryGetStringField(TEXT("type"), NormType))
    {
        bTokenizePatternTypeReplace = NormType.Equals(TEXT("Replace"));
    }

    TokenToId.Empty();
    TSharedPtr<FJsonObject> ModelRoot = JsonRootObject->GetObjectField(TEXT("model"));
    if (!ModelRoot->HasTypedField<EJson::Object>(TEXT("vocab")))
    {
        UE_LOG(LogTemp, Warning, TEXT("ImportFromFile: invalid json file %s"), *FileName);
        return;
    }
    TSharedPtr<FJsonObject> VocabRoot = ModelRoot->GetObjectField(TEXT("vocab"));
    for (const auto& CharToToken : VocabRoot->Values)
    {
        FString VocabChar = CharToToken.Key;
        if (CharToToken.Value->Type == EJson::Number)
        {
            int32 Token = CharToToken.Value->AsNumber();
            TokenToId.Add(VocabChar, FTokensArrayWrapper({ Token }));
        }
    }

    ESpeakVoiceCode.Empty();
    LanguageCode.Empty();
    LanguageFamily.Empty();
    SampleRate = 24000;
    Speed = 1.f;
    SentenceSilenceSeconds = 0.f;
}

void UTTSModelData_Kokoro::ImportVoiceFromFile(const FString& FileName)
{
    if (!FPaths::FileExists(FileName))
    {
        return;
    }
    FString VoiceName = FPaths::GetBaseFilename(FileName);
    FString VoiceCode = VoiceName.Left(2);
    int32 VoiceId = Speakers.Num();

    if (Speakers.Contains(VoiceName))
    {
        VoiceId = Speakers[VoiceName];
    }
    else
    {
        Speakers.Add(VoiceName, VoiceId);
        VoiceEspeakCodes.Add(TEXT(""));
        VoiceCache.AddDefaulted();
    }

    // Add new speaker
    if (VoiceCode == TEXT("af") || VoiceCode == TEXT("am"))
    {
        VoiceCode = TEXT("en-us");
    }
    else if (VoiceCode == TEXT("bf") || VoiceCode == TEXT("bm"))
    {
        VoiceCode = TEXT("en-gb");
    }
    else if (VoiceCode == TEXT("jf") || VoiceCode == TEXT("jm"))
    {
        VoiceCode = TEXT("ja");
    }
    else if (VoiceCode == TEXT("zf") || VoiceCode == TEXT("zm"))
    {
        VoiceCode = TEXT("cmn");
    }
    else if (VoiceCode == TEXT("ef") || VoiceCode == TEXT("em"))
    {
        VoiceCode = TEXT("es");
    }
    else if (VoiceCode == TEXT("ff"))
    {
        VoiceCode = TEXT("fr-fr");
    }
    else if (VoiceCode == TEXT("hf") || VoiceCode == TEXT("hm"))
    {
        VoiceCode = TEXT("hi");
    }
    else if (VoiceCode == TEXT("if") || VoiceCode == TEXT("im"))
    {
        VoiceCode = TEXT("it");
    }
    else if (VoiceCode == TEXT("pf") || VoiceCode == TEXT("pm"))
    {
        VoiceCode = TEXT("pt-br");
    }
    // Add espeak code
    VoiceEspeakCodes[VoiceId] = VoiceCode;

    // Add cached voice data, actual shape is [-1, 1, 256]
    TArray<uint8> RawBinaryData;
    FFileHelper::LoadFileToArray(RawBinaryData, *FileName);
    auto& TargetFloatArray = VoiceCache[VoiceId].Data;
    TargetFloatArray.SetNumUninitialized(RawBinaryData.Num() / sizeof(float));

    UE_LOG(LogTemp, Log, TEXT("From BIN file loaded %d bytes, casting them as %d floats"), RawBinaryData.Num(), TargetFloatArray.Num());

    FMemory::Memcpy(TargetFloatArray.GetData(), RawBinaryData.GetData(), RawBinaryData.Num());
}

void UTTSModelData_Kokoro::DeleteSpeaker(int32 SpeakerID)
{
    FString key;
    for (auto& sp : Speakers)
    {
        if (sp.Value == SpeakerID)
        {
            key = sp.Key;
        }
        else if (sp.Value > SpeakerID)
        {
            sp.Value--;
        }
    }

    if (VoiceCache.IsValidIndex(SpeakerID))
    {
        Speakers.Remove(key);
        VoiceCache.RemoveAt(SpeakerID);
        VoiceEspeakCodes.RemoveAt(SpeakerID);
    }
}

void UTTSModelData_Kokoro::DeleteAllSpeakers()
{
    Speakers.Empty();
    VoiceCache.Empty();
    VoiceEspeakCodes.Empty();
}
