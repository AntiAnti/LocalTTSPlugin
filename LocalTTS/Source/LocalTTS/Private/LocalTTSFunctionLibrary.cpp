// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSFunctionLibrary.h"
#include "LocalTTSTypes.h"
#include "Internationalization/Regex.h"
#include "LocalTTSSubsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "TTSModelData_Base.h"
#include "Misc/FileHelper.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"

#include "Modules/ModuleManager.h"
#include "LocalTTSModule.h"
#include <string>

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"

struct WaveHeader
{
	char ChunkID[4];
	unsigned int ChunkSize;
	char Format[4];
	char SubChunk1ID[4];
	unsigned int SubChunk1Size;
	unsigned short int AudioFormat;
	unsigned short int NumChannels;
	unsigned int SampleRate;
	unsigned int ByteRate;
	unsigned short int BlockAlign;
	unsigned short int BitsPerSample;
	char SubChunk2ID[4];
	unsigned int SubChunk2Size; // PCM data size in bytes
};

void ULocalTTSFunctionLibrary::SaveAudioDataToFile(const TArray<uint8>& RawPCMData, int32 NumChannels, int32 SampleRate, FString FileName)
{
	const int32 WaveHeaderSize = sizeof(WaveHeader);
	TArray<uint8> FileData;
	FileData.SetNumUninitialized(WaveHeaderSize + RawPCMData.Num());

	// Saving audio from editor asset, need to decompress it first
	UE_LOG(LogTemp, Log, TEXT("Saving %s file using uncompressed data (size = %d)"), *FPaths::GetCleanFilename(FileName), RawPCMData.Num());

	//float Duration = ((RawFileData.Num() / 2) / (SampleRate * SampleRate));

	WaveHeader h;
	FMemory::Memcpy(h.ChunkID, "RIFF", 4);
	h.ChunkSize = 36 + RawPCMData.Num();
	FMemory::Memcpy(h.Format, "WAVE", 4);
	FMemory::Memcpy(h.SubChunk1ID, "fmt ", 4);
	h.SubChunk1Size = 16;
	h.AudioFormat = 1;
	h.NumChannels = NumChannels;
	h.SampleRate = SampleRate;
	h.BitsPerSample = 16;
	h.BlockAlign = h.NumChannels;
	h.ByteRate = h.SampleRate * h.NumChannels;

	FMemory::Memcpy(h.SubChunk2ID, "data", 4);
	h.SubChunk2Size = RawPCMData.Num();

	FMemory::Memcpy(FileData.GetData(), &h, sizeof(h));
	FMemory::Memcpy(FileData.GetData() + sizeof(h), RawPCMData.GetData(), RawPCMData.Num());

	FFileHelper::SaveArrayToFile(FileData, *FileName);
}

void ULocalTTSFunctionLibrary::SaveAudioData32ToFile(const Audio::FAlignedFloatBuffer& RawPCMData, int32 NumChannels, int32 SampleRate, FString FileName)
{
	const int32 WaveHeaderSize = sizeof(WaveHeader);
	TArray<uint8> FileData;
	FileData.SetNumUninitialized(WaveHeaderSize + RawPCMData.Num() * sizeof(float));

	// Saving audio from editor asset, need to decompress it first
	UE_LOG(LogTemp, Log, TEXT("Saving %s file using uncompressed data (size = %d), sample rate = %d"), *FPaths::GetCleanFilename(FileName), RawPCMData.Num(), SampleRate);
	const int32 NumSamples = RawPCMData.Num();
	const int32 BitsPerSample = 32;

	WaveHeader h;
	FMemory::Memcpy(h.ChunkID, "RIFF", 4);
	h.ChunkSize = 36 + NumSamples * NumChannels * BitsPerSample / 8;
	FMemory::Memcpy(h.Format, "WAVE", 4);
	FMemory::Memcpy(h.SubChunk1ID, "fmt ", 4);
	h.SubChunk1Size = 16;
	h.AudioFormat = 3;
	h.NumChannels = NumChannels;
	h.SampleRate = SampleRate;
	h.ByteRate = SampleRate * NumChannels * BitsPerSample / 8;
	h.BlockAlign = NumChannels * BitsPerSample / 8;
	h.BitsPerSample = BitsPerSample;

	FMemory::Memcpy(h.SubChunk2ID, "data", 4);
	h.SubChunk2Size = NumSamples * NumChannels * BitsPerSample / 8;
	FMemory::Memcpy(FileData.GetData(), &h, sizeof(h));
	FMemory::Memcpy(FileData.GetData() + sizeof(h), RawPCMData.GetData(), RawPCMData.Num() * sizeof(float));

	FFileHelper::SaveArrayToFile(FileData, *FileName);
}

int32 ULocalTTSFunctionLibrary::GetModelSpeakerByName(const FNNMInstanceId& ModelID, const FString& SpeakerName)
{
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	if (LocalTTS->IsVoiceModelValid(ModelID))
	{
		const auto& Speakers = LocalTTS->GetVoiceModel(ModelID)->VoiceDesc->Speakers;
		if (const int32* Id = Speakers.Find(SpeakerName))
		{
			return *Id;
		}
	}

	return INDEX_NONE;
}

bool ULocalTTSFunctionLibrary::IsTtsModelLoaded(const FNNMInstanceId& ModelID)
{
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	return LocalTTS->IsVoiceModelValid(ModelID);
}

UTTSModelData_Base* ULocalTTSFunctionLibrary::GetTtsModelDataAsset(const FNNMInstanceId& ModelID)
{
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	return LocalTTS->GetModelDataAsset(ModelID);
}

bool ULocalTTSFunctionLibrary::ReleaseTtsModel(const FNNMInstanceId& ModelID)
{
	ULocalTTSSubsystem* LocalTTS = GEngine->GetEngineSubsystem<ULocalTTSSubsystem>();
	return LocalTTS->ReleaseModel(ModelID);
}

void ULocalTTSFunctionLibrary::Util_PhonemizeDictionaries()
{
	TMap<FString, FString> FileToLanguage = {
		{ TEXT("ar.csv"), TEXT("ar_JO") },
		{ TEXT("de.csv"), TEXT("de") },
		{ TEXT("en_UK.csv"), TEXT("en") },
		{ TEXT("en_US.csv"), TEXT("en-us") },
		{ TEXT("es_ES.csv"), TEXT("es") },
		{ TEXT("es_MX.csv"), TEXT("es-la") },		
		//{ TEXT("fa.csv"), TEXT("fa_IR") },
		{ TEXT("fi.csv"), TEXT("fi") },
		{ TEXT("fr_FR.csv"), TEXT("fr") },
		{ TEXT("is.csv"), TEXT("is") },
		//{ TEXT("ma.csv"), TEXT("ml_IN") },
		{ TEXT("nl.dic"), TEXT("nl") },
		{ TEXT("vi_N.csv"), TEXT("vi") },
		{ TEXT("zh_hant.csv"), TEXT("zh") },
		{ TEXT("hungarian.dic"), TEXT("hu") },
		{ TEXT("italian.dic"), TEXT("it") },
		{ TEXT("polish.dic"), TEXT("pl") },
		{ TEXT("pt_br.dic"), TEXT("pt") },
		{ TEXT("pt_pt.dic"), TEXT("pt-pt") },
		{ TEXT("turkish.dic"), TEXT("tr") },
		{ TEXT("russian.txt"), TEXT("ru") },		
	};

	auto ModuleTts = FModuleManager::GetModulePtr<FLocalTTSModule>(TEXT("LocalTTS"));
	int Terminator = 0;
	const FRegexPattern Pattern(TEXT("\\([^()]*\\)"));

	const FString DictPath = TEXT("C:/Users/Yura/source/repos/G2PTrain/G2PTrain/csv");
	const FString EndChar = TEXT("/");
	for (const auto& FileInfo : FileToLanguage)
	{
		FString VoiceCode = FileInfo.Value;
		bool bPreExisting = false;

		UE_LOG(LogTemp, Log, TEXT("Set Language: %s"), *VoiceCode);
		int32 Result = ModuleTts->func_espeak_SetVoiceByName(TCHAR_TO_ANSI(*VoiceCode));
		if (Result != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Error. Unsupported Voice"));
			bPreExisting = true;
		}

		TArray<FString> CsvData, ResultData;
		FFileHelper::LoadFileToStringArray(CsvData, *(DictPath / FileInfo.Key));

		bool bFormatIPA = FileInfo.Key.EndsWith(TEXT(".csv"));
		bool bFormatRu = FileInfo.Key.EndsWith(TEXT(".txt"));
		bool bFormatDict = FileInfo.Key.EndsWith(TEXT(".dic"));

		int32 Counter = 0;
		for (auto& Line : CsvData)
		{
			Line.ToLowerInline();
			FString Word;
			if (bPreExisting)
			{
				int32 SepIndex = Line.Find(TEXT(","));
				if (SepIndex <= 0) continue;
				Word = Line.Left(SepIndex);
				FString Phonemes = Line.Right(Line.Len() - SepIndex - 1);
				Phonemes.TrimStartAndEndInline();
				if (Phonemes.StartsWith(TEXT("\"")) || Phonemes.Contains(TEXT(";")) || Phonemes.Contains(TEXT(","))) //"/aːðana/, /aːðanna/, /aːðin/"
				{
					Phonemes.MidInline(1);
					if (Phonemes.StartsWith(TEXT("/")))
					{
						Phonemes.MidInline(1);
					}
					SepIndex = Line.Find(TEXT("/"));
					Phonemes = Phonemes.Left(SepIndex);
				}
				else
				{
					Phonemes.ReplaceInline(TEXT("/"), TEXT(""));
				}

				Line = Word + TEXT(";") + Phonemes;

				if (Counter == 0)
				{
					UE_LOG(LogTemp, Log, TEXT("espeak_TextToPhonemesWithTerminator %s"), *Line);
				}
				Counter = (Counter + 1) % 100;

				continue;
			}
			else if (bFormatIPA)
			{
				int32 SepIndex = Line.Find(TEXT(","));
				if (SepIndex <= 0) continue;
				Word = Line.Left(SepIndex);
			}
			else if (bFormatRu)
			{
				Word = Line;
			}
			else if (bFormatDict)
			{
				int32 SepIndex = Line.Find(EndChar);
				if (SepIndex <= 0) continue;
				Word = Line.Left(SepIndex);
			}

			// phonemize
			std::string stword = TCHAR_TO_UTF8(*Word);
			const char* InputTextPointer = stword.c_str();
			std::string clausePhonemes(ModuleTts->func_espeak_TextToPhonemesWithTerminator((const void**)&InputTextPointer, /*textmode*/ espeakCHARS_AUTO, /*phonememode = IPA*/ 0x02, &Terminator));

			// save
			FString Phonemes = UTF8_TO_TCHAR(clausePhonemes.c_str());
			// cleanup
			FRegexMatcher Matcher(Pattern, Phonemes);
			while (Matcher.FindNext())
			{
				int32 Start = Matcher.GetMatchBeginning();
				int32 End = Matcher.GetMatchEnding();
				Phonemes.RemoveAt(Start, End - Start);
				Matcher = FRegexMatcher(Pattern, Phonemes);
			}

			Line = Word + TEXT(";") + Phonemes;


		}

		FString FinalFileName = DictPath / TEXT("espoken") / FileInfo.Key.Left(FileInfo.Key.Len() - 3) + TEXT("csv");
		FFileHelper::SaveStringArrayToFile(CsvData, *FinalFileName);
	}
}

void ULocalTTSFunctionLibrary::Util_PhonemizeDictionariesToTrainG2P()
{
	TMap<FString, FString> EspeakToActual = {
		{TEXT("ar"), TEXT("ara")}, {TEXT("ca"), TEXT("cat")}, {TEXT("cs"), TEXT("cze")}, {TEXT("cy"), TEXT("wel-nw")}, {TEXT("da"), TEXT("dan")}, {TEXT("de"), TEXT("ger")}, {TEXT("el"), TEXT("gre")}, {TEXT("en-gb-x-rp"), TEXT("eng-uk")}, {TEXT("en-us"), TEXT("eng-us")}, {TEXT("es"), TEXT("spa")}, {TEXT("es-419"), TEXT("spa-me")}, {TEXT("fa"), TEXT("fas")}, {TEXT("fi"), TEXT("fin")}, {TEXT("fr"), TEXT("fra")}, {TEXT("fr-fr"), TEXT("fra")}, {TEXT("hu"), TEXT("hun")}, {TEXT("is"), TEXT("ice")}, {TEXT("it"), TEXT("ita")}, {TEXT("ka"), TEXT("geo")}, {TEXT("kk"), TEXT("kaz")}, {TEXT("lb"), TEXT("ltz")}, {TEXT("nl"), TEXT("dut")}, {TEXT("nb"), TEXT("nob")}, {TEXT("pl"), TEXT("pol")}, {TEXT("pt-br"), TEXT("por-bz")}, {TEXT("pt"), TEXT("por-po")}, {TEXT("ro"), TEXT("ron")}, {TEXT("ru"), TEXT("rus")}, {TEXT("sk"), TEXT("slo")}, {TEXT("sl"), TEXT("slv")}, {TEXT("sr"), TEXT("srp")}, {TEXT("sv"), TEXT("swe")}, {TEXT("sw"), TEXT("swa")}, {TEXT("tr"), TEXT("tur")}, {TEXT("uk"), TEXT("ukr")}, {TEXT("vi"), TEXT("vie-n")}, {TEXT("cmn"), TEXT("zho-s")}, {TEXT("zh"), TEXT("zho-s")}, {TEXT("j"), TEXT("jpn")}, {TEXT("ja"), TEXT("jpn")}, {TEXT("hi"), TEXT("hin")}
	};

	auto ModuleTts = FModuleManager::GetModulePtr<FLocalTTSModule>(TEXT("LocalTTS"));
	int Terminator = 0;

	// remove language codes
	const FRegexPattern Pattern(TEXT("\\([^()]*\\)"));

	// train dataset
	//const FString SrcPath = TEXT("C:/Users/Yura/source/repos/CharsiuG2P/data/train");
	// dev dataset
	//const FString SrcPath = TEXT("C:/Users/Yura/source/repos/CharsiuG2P/data/dev");
	// test dataset
	const FString SrcPath = TEXT("C:/Users/Yura/source/repos/CharsiuG2P/data/test");
	for (const auto& Lang : EspeakToActual)
	{
		int32 Result = ModuleTts->func_espeak_SetVoiceByName(TCHAR_TO_ANSI(*Lang.Key));
		if (Result != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Error. Unsupported Voice"));
			continue;
		}

		// LOAD
		TArray<FString> FileData;
		FString FileName = SrcPath / Lang.Value + TEXT(".tsv");
		FString DestFileName = SrcPath / TEXT("espoken") / Lang.Value + TEXT(".tsv");
		FFileHelper::LoadFileToStringArray(FileData, *FileName);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* FileHandle = PlatformFile.OpenWrite(*DestFileName);

		// GENERATE NEW PHONEMES USING ESPEAK-NG
		for (auto& Line : FileData)
		{
			int32 SepIndex = Line.Find(TEXT("\t"));
			if (SepIndex <= 0) continue;
			Line.LeftInline(SepIndex);
			Line.ToLowerInline();

			// phonemize using espeak-ng
			std::string WordUtf8 = TCHAR_TO_UTF8(*Line);
			const char* InputTextPointer = WordUtf8.c_str();
			std::string WordPhonemes(ModuleTts->func_espeak_TextToPhonemesWithTerminator((const void**)&InputTextPointer, /*textmode*/ espeakCHARS_AUTO, /*phonememode = IPA*/ 0x02, &Terminator));
			FString Phonemes = UTF8_TO_TCHAR(WordPhonemes.c_str());
			// cleanup
			FString tmp = Phonemes;
			FRegexMatcher Matcher(Pattern, Phonemes);
			while (Matcher.FindNext())
			{
				int32 Start = Matcher.GetMatchBeginning();
				int32 End = Matcher.GetMatchEnding();
				Phonemes.RemoveAt(Start, End - Start);
				Matcher = FRegexMatcher(Pattern, Phonemes);
			}

			// save
			Line.Append(TEXT("\t") + Phonemes);

			FString linetemp = Line + TEXT("\r\n");
			std::string strutf8(TCHAR_TO_UTF8(*linetemp));
			FileHandle->Write((const uint8*)strutf8.c_str(), strutf8.size());
		}

		// SAVE TO NEW FILE
		delete FileHandle;
		//FFileHelper::SaveStringArrayToFile(FileData, *DestFileName);
	}
}

bool ULocalTTSFunctionLibrary::LoadNNM(FNNEModelTTS& ModelData, class UNNEModelData* ModelAsset, int32 OutputDataSize, FString Header)
{
	bool bResult = false;

	FString NneRuntimeName = TEXT("NNERuntimeORTCpu");
	TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(NneRuntimeName);
	if (!Runtime.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Can't find NNE runtime (NNERuntimeORTCpu)"), *Header);
		return false;
	}

	// Load model
	TSharedPtr<UE::NNE::IModelCPU> Model = Runtime->CreateModelCPU(ModelAsset);
	if (!Model.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Couldn't load runtime TTS model"), *Header);
		return false;
	}

	// Create instance
	ModelData.ModelInstance = Model->CreateModelInstanceCPU();
	if (!ModelData.ModelInstance.IsValid())
	{
		return false;
	}

	// Initialize model instance
	TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = ModelData.ModelInstance->GetInputTensorDescs();

	int32 Index = 0;
	for (const auto& InShape : InputTensorDescs)
	{
		ModelData.InputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(InShape.GetShape()));
		const int32 Volume = FMath::Max(1, (int32)ModelData.InputTensorShapes.Last().Volume());

		ModelData.InputMap.AddDefaulted();
		ModelData.InputMap[Index].Format = InShape.GetDataType();
		FString DataTypeStr = StaticEnum<ENNETensorDataType>()->GetNameByValue((int32)InShape.GetDataType()).ToString();

#if WITH_EDITOR
		FString ShapeDesc = LocalTtsUtils::PrintArray(InShape.GetShape().GetData());
		UE_LOG(LogTemp, Log, TEXT("%s: InputTensorShapes[%d] has shape (%s) and type %s"), *Header, Index, *ShapeDesc, *DataTypeStr);
#endif

		if (InShape.GetDataType() == ENNETensorDataType::Float)
		{
			ModelData.InputDataFloat.AddDefaulted();
			ModelData.InputDataFloat.Last().SetNumUninitialized(Volume);
			ModelData.InputMap[Index].ArrayIndex = ModelData.InputDataFloat.Num() - 1;
		}
		else //if (InShape.GetDataType() == ENNETensorDataType::Int64)
		{
			ModelData.InputDataInt64.AddDefaulted();
			ModelData.InputDataInt64.Last().SetNumUninitialized(Volume);
			ModelData.InputMap[Index].ArrayIndex = ModelData.InputDataInt64.Num() - 1;
		}

		Index++;
	}

	TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs = ModelData.ModelInstance->GetOutputTensorDescs();
	TArray<UE::NNE::FTensorShape> OutputTensorShapes;
	Index = 0;
	int32 OutputSize = 1;
	for (const auto& OutShape : OutputTensorDescs)
	{
		OutputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(OutShape.GetShape()));
		FString DataTypeStr = StaticEnum<ENNETensorDataType>()->GetNameByValue((int32)OutShape.GetDataType()).ToString();

		for (const auto& Dim : OutputTensorShapes.Last().GetData())
		{
			if (Dim > 0) OutputSize *= Dim;
		}
#if WITH_EDITOR
		FString ShapeDesc = LocalTtsUtils::PrintArray(OutShape.GetShape().GetData());
		UE_LOG(LogTemp, Log, TEXT("%s: OutputTensorDescs[%d] has shape (%s and total size %d) and type %s"), *Header, Index, *ShapeDesc, OutputSize, *DataTypeStr);
#endif
		Index++;
	}
	if (OutputTensorShapes.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect NNE Model format!"));
		return false;
	}
	ModelData.OutputData.SetNumZeroed(OutputDataSize * OutputSize);

	// Initialize input-output tensors and arrays

	ModelData.InputBindings.SetNumZeroed(ModelData.InputMap.Num());
	Index = 0;
	for (const auto& Input : ModelData.InputMap)
	{
		if (Input.Format == ENNETensorDataType::Float)
		{
			ModelData.InputBindings[Index].Data = ModelData.InputDataFloat[Input.ArrayIndex].GetData();
			ModelData.InputBindings[Index].SizeInBytes = ModelData.InputDataFloat[Input.ArrayIndex].Num() * sizeof(float);
		}
		else
		{
			ModelData.InputBindings[Index].Data = ModelData.InputDataInt64[Input.ArrayIndex].GetData();
			ModelData.InputBindings[Index].SizeInBytes = ModelData.InputDataInt64[Input.ArrayIndex].Num() * sizeof(int64);
		}
		Index++;
	}

	ModelData.OutputBindings.SetNumZeroed(1);
	ModelData.OutputBindings[0].Data = ModelData.OutputData.GetData();
	ModelData.OutputBindings[0].SizeInBytes = ModelData.OutputData.Num() * sizeof(float);

	bResult = ModelData.InputMap.Num() > 0 && ModelData.OutputData.Num() > 0;
	ModelData.bLoaded = bResult;
	UE_LOG(LogTemp, Log, TEXT("%s: model loading complete. Input num = %d | Output num = %d"), *Header, ModelData.InputMap.Num(), ModelData.OutputData.Num());
	
	return bResult;
}