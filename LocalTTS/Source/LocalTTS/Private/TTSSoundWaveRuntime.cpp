// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "TTSSoundWaveRuntime.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "Misc/ScopeLock.h"
#include "Engine/Engine.h"
#include "AudioMixerTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/AssetRegistryTagsContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TTSSoundWaveRuntime)

UTTSSoundWaveRuntime::UTTSSoundWaveRuntime(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DataMutex(MakeShared<FCriticalSection>())
{
	bProcedural = true;
	NumBufferUnderrunSamples = 512;
	NumSamplesToGeneratePerCallback = DEFAULT_PROCEDURAL_SOUNDWAVE_BUFFER_SIZE;
	static_assert(DEFAULT_PROCEDURAL_SOUNDWAVE_BUFFER_SIZE >= 512, TEXT("Should generate more samples than this per callback."));

	SampleByteSize = 2;
}

int32 UTTSSoundWaveRuntime::GetPlatformSampleRate() const
{
	return (int32)GetSampleRateForCurrentPlatform();
}

int32 UTTSSoundWaveRuntime::GetChannelsNum() const
{
	return NumChannels;
}

void UTTSSoundWaveRuntime::Parse(FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances)
{
	FScopeLock Lock(&*DataMutex);

	if (ActiveSound.PlaybackTime == 0.f)
	{
		SetPlaybackTime(ParseParams.StartTime);
	}

	// Stopping all other active sounds that are using the same sound wave, so that only one sound wave can be played at a time
	const TArray<FActiveSound*>& ActiveSounds = AudioDevice->GetActiveSounds();
	for (FActiveSound* ActiveSoundPtr : ActiveSounds)
	{
		if (ActiveSoundPtr->GetSound() == this && ActiveSoundPtr->IsPlayingAudio() && &ActiveSound != ActiveSoundPtr)
		{
			AudioDevice->StopActiveSound(ActiveSoundPtr);
		}
	}

	ActiveSound.PlaybackTime = GetPlaybackTime();

	if (IsPlaybackFinished())
	{
		if (!bLooping)
		{
			AudioDevice->StopActiveSound(&ActiveSound);
		}
		else
		{
			ActiveSound.PlaybackTime = 0.f;
			SetPlaybackTime(0.f);
		}
	}

	Super::Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
}

void UTTSSoundWaveRuntime::InitializeAudio(const uint8* AudioData, const int32 BufferSize)
{
	FScopeLock Lock(&*DataMutex);

	Audio::EAudioMixerStreamDataFormat::Type Format = GetGeneratedPCMDataFormat();
	SampleByteSize = (Format == Audio::EAudioMixerStreamDataFormat::Int16) ? 2 : 4;

	int32 UseBufferSize = BufferSize;
	if (BufferSize == 0 || !ensure((BufferSize % SampleByteSize) == 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("UTTSSoundWaveRuntime: invalid audio buffer size"));
		UseBufferSize = BufferSize - (BufferSize % SampleByteSize);
		return;
	}

	NumChannels = 1;
	bLooping = false;
	Duration = (float)UseBufferSize / (float)(SampleByteSize * SampleRate);

	StaticAudioBuffer.SetNumUninitialized(UseBufferSize);
	FMemory::Memcpy(StaticAudioBuffer.GetData(), AudioData, UseBufferSize);
	SetPlaybackTime(0.f);
}

void UTTSSoundWaveRuntime::GetRawPCMData(TArray<uint8>& Buffer) const
{
	Buffer = StaticAudioBuffer;
}

int32 UTTSSoundWaveRuntime::GetAudioBufferSize()
{
	return StaticAudioBuffer.Num();
}

bool UTTSSoundWaveRuntime::SetPlaybackTime(float PlaybackTime)
{
	FScopeLock Lock(&*DataMutex);

	PlaybackTime = FMath::Min(PlaybackTime, Duration);
	PlayedNumOfFrames = PlaybackTime * GetSampleRateForCurrentPlatform() / SampleByteSize;

	return true;
}

float UTTSSoundWaveRuntime::GetPlaybackTime() const
{
	FScopeLock Lock(&*DataMutex);

	return (float)SampleByteSize * (float)PlayedNumOfFrames / GetSampleRateForCurrentPlatform();
}

bool UTTSSoundWaveRuntime::IsPlaybackFinished() const
{
	FScopeLock Lock(&*DataMutex);

	const bool bOutOfFrames = ((int32)PlayedNumOfFrames * SampleByteSize) >= StaticAudioBuffer.Num();
	return !StaticAudioBuffer.IsEmpty() && bOutOfFrames;
}

int32 UTTSSoundWaveRuntime::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	uint32 TotalSamplesAvailable = StaticAudioBuffer.Num() / SampleByteSize;
	uint32 SamplesToGenerate = FMath::Min3((uint32)SamplesNeeded, (uint32)NumSamplesToGeneratePerCallback, TotalSamplesAvailable - PlayedNumOfFrames);

	// Wait until we have enough samples that are requested before starting.
	if (SamplesToGenerate > 0)
	{
		const int32 BytesToCopy = SamplesToGenerate * SampleByteSize;
		FMemory::Memcpy((void*)PCMData, &StaticAudioBuffer[PlayedNumOfFrames * SampleByteSize], BytesToCopy);
		PlayedNumOfFrames += SamplesToGenerate;
		
		return BytesToCopy;
	}

	// There wasn't enough data ready, write out zeros
	const int32 BytesCopied = NumBufferUnderrunSamples * SampleByteSize;
	FMemory::Memzero(PCMData, BytesCopied);
	return BytesCopied;
}

int32 UTTSSoundWaveRuntime::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

bool UTTSSoundWaveRuntime::HasCompressedData(FName Format, ITargetPlatform* TargetPlatform) const
{
	return false;
}

#if ENGINE_MINOR_VERSION > 3
void UTTSSoundWaveRuntime::BeginGetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides, const ITargetPlatform* InTargetPlatform)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
}

FByteBulkData* UTTSSoundWaveRuntime::GetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides, const ITargetPlatform* InTargetPlatform)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
	return nullptr;
}
#endif

void UTTSSoundWaveRuntime::InitAudioResource(FByteBulkData& CompressedData)
{
	// Should never be pushing compressed data to a SoundWaveProcedural
	check(false);
}

bool UTTSSoundWaveRuntime::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a UTTSSoundWaveRuntime
	return true;
}

