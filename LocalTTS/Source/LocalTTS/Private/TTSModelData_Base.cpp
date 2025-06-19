// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "TTSModelData_Base.h"
#include "Modules/ModuleManager.h"
#include "LocalTTSModule.h"
//#include "espeak-ng/speak_lib.h"
#include "uni_algo.h"

#include <string>
#include <vector>

#define CLAUSE_INTONATION_FULL_STOP 0x00000000
#define CLAUSE_INTONATION_COMMA 0x00001000
#define CLAUSE_INTONATION_QUESTION 0x00002000
#define CLAUSE_INTONATION_EXCLAMATION 0x00003000

#define CLAUSE_TYPE_CLAUSE 0x00040000
#define CLAUSE_TYPE_SENTENCE 0x00080000

#define CLAUSE_PERIOD (40 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COMMA (20 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_QUESTION (40 | CLAUSE_INTONATION_QUESTION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_EXCLAMATION                                                     \
  (45 | CLAUSE_INTONATION_EXCLAMATION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COLON (30 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_SEMICOLON (30 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)

/*
* The idea is to use differet tokenizer assets inherited from UTTSTokenizerBase for different TTS systems,
* But right now the only supported TTS is piper (https://github.com/rhasspy/piper).
* Another possible local TTS which can be added in a future is KokoroTTS (https://huggingface.co/hexgrad/Kokoro-82M).
* While it provides better quality of the audio, currenly it support 8 languages, and piper supports 36 languages.
*/

bool UTTSModelData_Base::PhonemizeText(const FString& InText, FString& OutText, int32 SpeakerId, TArray<TArray<Piper::PhonemeUtf8>>& Phonemes)
{
    auto ModuleTts = FModuleManager::GetModulePtr<FLocalTTSModule>(TEXT("LocalTTS"));
    if (!ModuleTts->IsLoaded()) return false;

    FString VoiceCode = GetEspeakCode(SpeakerId);
    int32 Result = ModuleTts->func_espeak_SetVoiceByName(TCHAR_TO_ANSI(*VoiceCode));
    if (Result != 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to set eSpeak-ng voice: \"%s\" (default is %s)"), *VoiceCode, *ESpeakVoiceCode);
        return false;
    }

    // Modified by eSpeak
    std::string textCopy(TCHAR_TO_UTF8(*InText));

    TArray<Piper::PhonemeUtf8>* SentencePhonemes = nullptr;
    const char* InputTextPointer = textCopy.c_str();
    int Terminator = 0;

    while (InputTextPointer != NULL)
    {
        // Modified espeak-ng API to get access to clause terminator
        std::string clausePhonemes(ModuleTts->func_espeak_TextToPhonemesWithTerminator((const void**)&InputTextPointer, /*textmode*/ espeakCHARS_AUTO, /*phonememode = IPA*/ 0x02, &Terminator));
#if PLATFORM_ANDROID
        char chrTerminator[2] = { '0', '0' };
        clausePhonemes.append(chrTerminator);
#endif
        OutText.Append(UTF8_TO_TCHAR(clausePhonemes.c_str()));

        auto PhonemesNorm = una::norm::to_nfd_utf8(clausePhonemes);
        auto PhonemesRange = una::ranges::utf8_view{ PhonemesNorm };

        if (!SentencePhonemes)
        {
            // Start new sentence
            Phonemes.AddDefaulted();
            SentencePhonemes = &Phonemes[Phonemes.Num() - 1];
        }

        std::vector<Piper::PhonemeUtf8> MappedSentPhonemes;
        MappedSentPhonemes.insert(MappedSentPhonemes.end(), PhonemesRange.begin(), PhonemesRange.end());

        auto phonemeIter = MappedSentPhonemes.begin();
        auto phonemeEnd = MappedSentPhonemes.end();

        // Filter out (lang) switch (flags).
        // These surround words from languages other than the current voice.
        bool inLanguageFlag = false;

        while (phonemeIter != phonemeEnd)
        {
            if (inLanguageFlag)
            {
                if (*phonemeIter == U')')
                {
                    // End of (lang) switch
                    inLanguageFlag = false;
                }
            }
            else if (*phonemeIter == U'(')
            {
                // Start of (lang) switch
                inLanguageFlag = true;
            }
            else
            {
                SentencePhonemes->Add(*phonemeIter);
            }

            phonemeIter++;
        }

        // Add appropriate punctuation depending on terminator type
        int punctuation = Terminator & 0x000FFFFF;
        if (punctuation == CLAUSE_PERIOD)
        {
            SentencePhonemes->Add(P_Period);
        }
        else if (punctuation == CLAUSE_QUESTION)
        {
            SentencePhonemes->Add(P_Question);
        }
        else if (punctuation == CLAUSE_EXCLAMATION)
        {
            SentencePhonemes->Add(P_Exclamation);
        }
        else if (punctuation == CLAUSE_COMMA)
        {
            SentencePhonemes->Add(P_Comma);
            SentencePhonemes->Add(P_Space);
        }
        else if (punctuation == CLAUSE_COLON)
        {
            SentencePhonemes->Add(P_Colon);
            SentencePhonemes->Add(P_Space);
        }
        else if (punctuation == CLAUSE_SEMICOLON)
        {
            SentencePhonemes->Add(P_Semicolon);
            SentencePhonemes->Add(P_Space);
        }

        if ((Terminator & CLAUSE_TYPE_SENTENCE) == CLAUSE_TYPE_SENTENCE)
        {
            // End of sentence
            SentencePhonemes = nullptr;
        }
    } // while inputTextPointer != NULL

    return true;
}

FString UTTSModelData_Base::GetEspeakCode(int32 SpeakerId) const
{
    return ESpeakVoiceCode;
}

bool UTTSModelData_Base::Tokenize(const TArray<Piper::PhonemeUtf8>& Phonemes, TArray<Piper::PhonemeId>& OutTokens, TMap<Piper::PhonemeUtf8, int32>& OutMissedPhonemes, bool bFirst, bool bLast)
{
    return false;
}

bool UTTSModelData_Base::SetNNEInputParams(FNNEModelTTS& NNModel, const FTTSGenerateRequestContext& Context) const
{
    return false;
}

void UTTSModelData_Base::EnsurePhonemesMap()
{
    if (PhonemeIdMap.Num() < TokenToId.Num())
    {
        PhonemeIdMap.Empty();
        for (const auto& Binding : TokenToId)
        {
            FString strchar = Binding.Key;
            if (strchar.Len() == 0) continue;

            std::string s = TCHAR_TO_UTF8(*strchar);

            auto PhonemesNorm = una::norm::to_nfd_utf8(s);
            auto PhonemesRange = una::ranges::utf8_view{ PhonemesNorm };

            Piper::PhonemeUtf8 Phoneme = *PhonemesRange.begin();
            TArray<Piper::PhonemeId> Ids;
            Ids.SetNum(Binding.Value.Tokens.Num());
            for (int32 i = 0; i < Ids.Num(); i++)
            {
                Ids[i] = (Piper::PhonemeId)Binding.Value.Tokens[i];
            }

            PhonemeIdMap.Add(Phoneme, Ids);
        }
    }
}
