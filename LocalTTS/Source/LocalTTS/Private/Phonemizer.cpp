// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "Phonemizer.h"
#include "LocalTTSFunctionLibrary.h"
#include "LocalTTSTypes.h"
#include "Modules/ModuleManager.h"
#include "LocalTTSModule.h"
#include "Engine/AssetManager.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "LocalTTSSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DictionaryArchive.h"
#include "Async/Async.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"
#include <string>

// convert three-dimensional array address to one-dim
#define _ADDR3(a, b, c, shape) a*shape[1]*shape[2] + b*shape[2] + c

namespace LocalTtsUtils
{
	FString PrintShape(const TArray<uint32>& Shape)
	{
		FString s;
		for (const auto& d : Shape)
		{
			if (!s.IsEmpty()) s.Append(TEXT(", "));
			s.Append(FString::FromInt(d));
		}
		return s;
	}
}

void UPhonemizer::BeginDestroy()
{
	Super::BeginDestroy();
}

void UPhonemizer::SyncLoadModel(TSoftObjectPtr<UNNEModelData> EncoderPtr, TSoftObjectPtr<UNNEModelData> DecoderPtr)
{
	if (EncoderPtr.IsNull() || DecoderPtr.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("UPhonemizer. Model Reference is not set, please assign it in the editor"));
		return;
	}

	/*
	AsyncTask(ENamedThreads::AnyThread, [this, EncoderPtr, DecoderPtr]() mutable
	{
	});
	*/
	UNNEModelData* EncoderModelAsset = EncoderPtr.LoadSynchronous();
	UNNEModelData* DecoderModelAsset = DecoderPtr.LoadSynchronous();

	if (IsValid(EncoderModelAsset) && IsValid(DecoderModelAsset))
	{
		// We'll override output buffer size every time before RunSync
		ULocalTTSFunctionLibrary::LoadNNM(Encoder, EncoderModelAsset, 1024, TEXT("G2PEncoder"));
		ULocalTTSFunctionLibrary::LoadNNM(Decoder, DecoderModelAsset, 1024, TEXT("G2PDecoder"));
	}
}

void UPhonemizer::SetLanguageCodeFormatRaw(const FString& InLanguageCode)
{
	LanguageCode = InLanguageCode.TrimStartAndEnd();
}

// Convert eSpeak language code to g2p model language code
bool UPhonemizer::SetLanguageCodeFormatEspeak(const FString& InLanguageCode, bool bUseDicrionary)
{
	if (const FString* RawCode = EspeakToActual.Find(InLanguageCode))
	{
		const FString CodeNNM = *RawCode;
		UE_LOG(LogTemp, Log, TEXT("SetLanguageCodeFormatEspeak code: %s"), *CodeNNM);
		if (bUseDicrionary)
		{
			FName DictKey = *CodeNNM;
			if (Dictionaries.Contains(DictKey) && !Dictionaries[DictKey].IsNull())
			{
				UDictionaryArchive* Dict = Dictionaries[DictKey].IsValid()
					? Dictionaries[DictKey].Get()
					: Dictionaries[DictKey].LoadSynchronous();

				if (!Dict->IsDictionaryReady() && Dict->HasBulkData())
				{
					Dict->Unzip();
				}
			}
		}
		SetLanguageCodeFormatRaw(CodeNNM);
		return true;
	}
	return false;
}


void UPhonemizer::PrepareDictionary(const FString& EspeakLanguageCode)
{
	if (const FString* RawCode = EspeakToActual.Find(EspeakLanguageCode))
	{
		const FName DictKey = **RawCode;
		
		if (Dictionaries.Contains(DictKey)
			&& !Dictionaries[DictKey].IsNull()
			&& !Dictionaries[DictKey].IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Prepare dictionary asset: %s"), *DictKey.ToString());
			Dictionaries[DictKey].ToSoftObjectPath().LoadAsync(OnDictionaryLoaded);
		}
	}
}

void UPhonemizer::LoadDictionaryFromArchive(const FString& FileName, const FString& InLanguageCode)
{
	if (!Dictionaries.Contains(*InLanguageCode))
	{
		TSoftObjectPtr<UDictionaryArchive> ptr(FSoftObjectPath(TEXT("/LocalTTS/G2P/Dict/" + InLanguageCode + TEXT(".") + InLanguageCode)));
		Dictionaries.Add(*InLanguageCode, ptr);
	}
	UDictionaryArchive* arch = Dictionaries[*InLanguageCode].LoadSynchronous();
	if (arch)
	{
		arch->InitializeFromFile(FileName);
	}
}

FString UPhonemizer::GetLanguage() const
{
	return LanguageCode;
}

// At first, try to get phonemes from dictionary if it exists and unzipped
// For all non-phonemized words run NNE G2P model
void UPhonemizer::SyncPhonemizeText(const FString& Text, FString& PhonemizedText, TArray<FString>& OutWords, bool bCharactersAsWords)
{
	const int32 MaxLen = 50;
	const int32 TokenEOS = 1;
	const int32 TokenPad = 0;
	const int32 CharCodeOffset = 3;

	// Wrap language code as tag
	FString PreparedLangCode = LanguageCode;
	if (PreparedLangCode.Left(1) != TEXT("<"))
	{
		PreparedLangCode = TEXT("<") + PreparedLangCode;
	}
	if (PreparedLangCode.Right(1) != TEXT(":"))
	{
		if (PreparedLangCode.Right(1) != TEXT(">"))
		{
			PreparedLangCode.Append(TEXT(">:"));
		}
		else
		{
			PreparedLangCode.Append(TEXT(":"));
		}
	}

	TMap<int32, FString> WordTerminators;

	// Cleanup words
	FString PreparedText = Text;
	TSet<FString> Terminators = { TEXT("."), TEXT(","), TEXT("?"), TEXT("!") };
	TSet<FString> TxtRepl = { TEXT("/"), TEXT("\\"), TEXT("("), TEXT(")"), TEXT(":"), TEXT(";"), TEXT("\""), TEXT("_"), TEXT("\r"), TEXT("\n"), TEXT("？"), TEXT("。"), TEXT("«"), TEXT("»"), TEXT("\t"), TEXT("¿") };
	for (const auto& v : TxtRepl) PreparedText.ReplaceInline(*v, TEXT(" "));
	PreparedText.ReplaceInline(TEXT("  "), TEXT(" "));
	PreparedText.ReplaceInline(TEXT("  "), TEXT(" "));

	if (bCharactersAsWords)
	{
		FString TempText;
		for (int32 i = 0; i < PreparedText.Len(); i++)
		{
			const auto chr = PreparedText.Mid(i, 1);
			TempText.Append(chr);
			if (i < PreparedText.Len() - 1 && !TxtRepl.Contains(chr)) TempText.Append(TEXT(" "));
		}
		TempText.ReplaceInline(TEXT("  "), TEXT(" "));
		PreparedText = TempText;
	}

	// Split words into array
	TArray<FString> Words;
	PreparedText.ParseIntoArray(Words, TEXT(" "));
	for (int32 i = 0; i < Words.Num(); i++)
	{
		auto& w = Words[i];
		for (const auto& t : Terminators)
		{
			if (w.Contains(t))
			{
				WordTerminators.Add(i, t);
				w.ReplaceInline(*t, TEXT(""));
			}
		}
	}

	// Try to find words in the dictionary
	TArray<FString> WordsPhonemized;
	WordsPhonemized.SetNum(Words.Num());

	// Can use phonemization dictionary?
	if (const auto DictPtr = Dictionaries.Find(*LanguageCode))
	{
		if (DictPtr && !DictPtr->IsNull() && DictPtr->IsValid())
		{
			UDictionaryArchive* dict = DictPtr->Get();
			if (dict->IsDictionaryReady())
			{
				int32 WordsPhonemizedCounter = 0;
				for (int32 i = 0; i < Words.Num(); i++)
				{
					const FString& Word = Words[i];
					if (const FString* ph = dict->Find(Word.ToLower()))
					{
						WordsPhonemized[i] = *ph;
						WordsPhonemizedCounter++;
						Words[i] = TEXT("");
					}
					else WordsPhonemized[i] = TEXT("");
				}

				// only generate unphonemized words
				Words.Remove(TEXT(""));

				if (Words.IsEmpty())
				{
					UE_LOG(LogTemp, Log, TEXT("SyncPhonemizeText: done using dictionary."));
					for (int32 i = 0; i < WordsPhonemized.Num(); i++)
					{
						auto& w = WordsPhonemized[i];
						if (const FString* term = WordTerminators.Find(i)) w.Append(*term);
						PhonemizedText.Append(w + TEXT(" "));
					}
					PhonemizedText.TrimEndInline();
					OutWords = WordsPhonemized;
					return;
				}
				UE_LOG(LogTemp, Log, TEXT("SyncPhonemizeText: %d of %d words were phonemized using dictionary. Using NNM for %d."), WordsPhonemizedCounter, WordsPhonemized.Num(), Words.Num());
			}
		}
	}

	// Pad input_ids and fill attention_mask
	const int32 BatchNum = Words.Num();
	//FFileHelper::BufferToString()
	int32 MaxWordLength = 0;
	for (auto& W : Words)
	{
		W = PreparedLangCode + TEXT(" ") + W;

		std::string s = TCHAR_TO_UTF8(*W);
		MaxWordLength = FMath::Max(MaxWordLength, (int32)s.size());
	}
	TArray<int64> TokenizedWords, AttentionMask;
	TokenizedWords.SetNumZeroed(MaxWordLength * BatchNum);
	AttentionMask.Init(1, MaxWordLength * BatchNum);
	for (int32 i = 0; i < BatchNum; i++)
	{
		std::string s = TCHAR_TO_UTF8(*Words[i]);
		for (int32 n = 0; n < MaxWordLength; n++)
		{
			if (n < (int32)s.size())
			{
				TokenizedWords[i * MaxWordLength + n] = (uint64)(uint8)s[n] + CharCodeOffset;
				AttentionMask[i * MaxWordLength + n] = 1;
			}
			else
			{
				AttentionMask[i * MaxWordLength + n] = 0;
			}
		}

		FString s1;
		for (int32 n = 0; n < MaxWordLength; n++)
		{
			if (n > 0) s1.Append(TEXT(", "));
			s1.Append(FString::FromInt(TokenizedWords[i * MaxWordLength + n]));
		}
	}

	// Encoder Inputs
	Encoder.PrepareInputInt64(0, TokenizedWords, { (uint32)BatchNum, (uint32)MaxWordLength });
	Encoder.PrepareInputInt64(1, AttentionMask,  { (uint32)BatchNum, (uint32)MaxWordLength });
	// Encoder Outputs
	Encoder.PrepareOutputBuffer(BatchNum * MaxWordLength * 1024);
	// Run
	TArray<float> EncoderOutputs;
	TArray<uint32> EncoderOutputsShape;
	if (!Encoder.RunNNE(EncoderOutputs, EncoderOutputsShape))
	{
		UE_LOG(LogTemp, Log, TEXT("G2P Encoder failed"));
		return;
	}

	FString enshapetmp = LocalTtsUtils::PrintShape(EncoderOutputsShape);

	TArray<TArray<int64>> DecoderInputIds; // Shape = (words_num, decoder_seq_len = 1..2..3...)	
	DecoderInputIds.SetNum(BatchNum); // init with pad tokens; will remove later
	for (auto& W : DecoderInputIds) W.Init(TokenPad, 1);

	TArray<int64> DecoderInputIds_Data;
	DecoderInputIds_Data.SetNumZeroed(BatchNum); // We know Pad == 0

	TArray<bool> FinishedStatus;
	FinishedStatus.SetNumZeroed(BatchNum);

	for (int32 Step = 0; Step < MaxLen; Step++)
	{
		// Decoder Inputs		
		Decoder.PrepareInputInt64(0, AttentionMask,			{ (uint32)BatchNum, (uint32)MaxWordLength });	// encoder_attention_mask	(10, 15)
		Decoder.PrepareInputInt64(1, DecoderInputIds_Data,	{ (uint32)BatchNum, (uint32)Step + 1 });		// input_ids				(10, 1...)
		Decoder.PrepareInputFloat(2, EncoderOutputs,		EncoderOutputsShape);							// encoder_hidden_states	(10, 15, 256)
		// Decoder Outputs
		Decoder.PrepareOutputBuffer(BatchNum * 1024 * (Step + 1));
		// Run
		TArray<float> Logits;
		TArray<uint32> LogitsShape; // (0: batch_size, 1: decoder_seq_len, 2: vocab_size)
		if (!Decoder.RunNNE(Logits, LogitsShape))
		{
			UE_LOG(LogTemp, Log, TEXT("G2P Decoder failed"));
			return;
		}

		// Read generated tokens in the current step: extract logits for the latest data in sequence and do ArgMax
		TArray<int32> NextTokens;
		NextTokens.SetNumZeroed(LogitsShape[0]); // Batch size
		int32 LastInSeqIndex = (int32)LogitsShape[1] - 1;
		int32 FinishedCounter = 0;
		
		for (int32 WordId = 0; WordId < NextTokens.Num(); WordId++)
		{
			// Perform ArgMax for the latest in seq_len
			float MaxVal = Logits[_ADDR3(WordId, LastInSeqIndex, 0, LogitsShape)];
			for (int32 TokenId = 1; TokenId < (int32)LogitsShape[2]; TokenId++)
			{
				const float& TestVal = Logits[_ADDR3(WordId, LastInSeqIndex, TokenId, LogitsShape)];
				if (TestVal > MaxVal)
				{
					MaxVal = TestVal;
					NextTokens[WordId] = TokenId;
				}
			}

			// Check status for each word
			if (FinishedStatus[WordId])
			{
				NextTokens[WordId] = TokenPad;
			}
			else
			{
				if (NextTokens[WordId] == TokenEOS)
				{
					FinishedStatus[WordId] = true;
				}
			}

			if (FinishedStatus[WordId]) FinishedCounter++;
		}

		// Everything is generated
		if (FinishedCounter == BatchNum)
		{
			break;
		}
		
		// Update DecoderInputIds
		for (int32 WordId = 0; WordId < NextTokens.Num(); WordId++)
		{
			DecoderInputIds[WordId].Add(NextTokens[WordId]);
		}

		// Update data
		const int32 SequenceLength = (int32)LogitsShape[1] + 1;
		DecoderInputIds_Data.SetNumUninitialized(BatchNum * SequenceLength);
		for (int32 WordId = 0; WordId < NextTokens.Num(); WordId++)
		{
			FMemory::Memcpy(&DecoderInputIds_Data[WordId * SequenceLength], DecoderInputIds[WordId].GetData(), DecoderInputIds[WordId].Num() * sizeof(int64));
		}
	}

	// Set to next word which wasn't phonemized
	int32 InsertIndexNext = INDEX_NONE;
	while (++InsertIndexNext < WordsPhonemized.Num() && !WordsPhonemized[InsertIndexNext].IsEmpty());

	for (const auto& WordTokens : DecoderInputIds)
	{
		// 0, 206, 139
		int32 Index = 0;
		char buffer[128];
		FString Desc;

		for (int32 CharIndex = 0; CharIndex < WordTokens.Num(); CharIndex++)
		{
			const auto& byte = WordTokens[CharIndex];

			if (!Desc.IsEmpty()) Desc.Append(TEXT(", "));
			Desc.Append(FString::FromInt(byte));

			if (byte >= CharCodeOffset)
			{
				buffer[Index] = byte - CharCodeOffset;
				Index++;
			}
		}
		buffer[Index] = 0;
		WordsPhonemized[InsertIndexNext] = UTF8_TO_TCHAR(buffer);
		// next InsertIndexNext
		while (++InsertIndexNext < WordsPhonemized.Num() && !WordsPhonemized[InsertIndexNext].IsEmpty());		
	}

	for (const auto& wt : WordTerminators)
	{
		WordsPhonemized[wt.Key].Append(wt.Value);
	}

	OutWords = WordsPhonemized;
	
	// set resulted text
	PhonemizedText.Empty();
	for (const auto& w : WordsPhonemized)
	{
		PhonemizedText.Append(w + TEXT(" "));
	}
	PhonemizedText.TrimEndInline();
}

void UPhonemizer::ArgMax(const float* In, int32 Shape0, int32 Shape1, TArray<int64>& Out) const
{
	Out.SetNumUninitialized(Shape0);

	for (int32 lineIndex = 0; lineIndex < Shape0; lineIndex++)
	{
		int32 usefulIndex = INDEX_NONE;
		float MaxVal = 0.f;
		for (int32 argIndex = 0; argIndex < Shape1; argIndex++)
		{
			const float val = In[lineIndex * argIndex + argIndex];
			if (argIndex == 0 || MaxVal < val)
			{
				MaxVal = val; usefulIndex = argIndex;
			}
		}

		Out[lineIndex] = (int64)usefulIndex;
	}
}
