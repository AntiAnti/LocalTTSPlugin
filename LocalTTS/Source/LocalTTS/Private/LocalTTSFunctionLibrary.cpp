// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSFunctionLibrary.h"
#include "LocalTTSTypes.h"
#include "Internationalization/Regex.h"
#include "LocalTTSSubsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "TTSModelData_Base.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"

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
