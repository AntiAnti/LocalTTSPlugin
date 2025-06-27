// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerTypes.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundWaveProcedural.h"
#include "Runtime/Launch/Resources/Version.h"
#include "TTSSoundWaveRuntime.generated.h"

/**
 * SoundWave asset created in runtime with access to PCM data from blueprint
 */
UCLASS()
class LOCALTTS_API UTTSSoundWaveRuntime : public USoundWaveProcedural
{
	GENERATED_BODY()

protected:

	// The actual audio buffer that can be consumed. QueuedAudio is fed to this buffer. Accessed only audio thread.
	TArray<uint8> StaticAudioBuffer;

public:
	UTTSSoundWaveRuntime(const FObjectInitializer& ObjectInitializer);

	// Get audio sample rate for current platform
	UFUNCTION(BlueprintPure, Category = "Sound Wave")
	int32 GetPlatformSampleRate() const;

	// Get channesl number
	UFUNCTION(BlueprintPure, Category = "Sound Wave")
	int32 GetChannelsNum() const;

	// Get raw 16-bit audio buffer
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get PCM Data"), Category = "Sound Wave")
	void GetRawPCMData(TArray<uint8>& Buffer) const;

	//~ Begin USoundBase Interface.
	virtual void Parse(class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances) override;
	//~ End USoundBase Interface.

	//~ Begin USoundWave Interface.
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	virtual bool HasCompressedData(FName Format, ITargetPlatform* TargetPlatform) const override;
	virtual void BeginGetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides, const ITargetPlatform* InTargetPlatform) override;
	virtual FByteBulkData* GetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides, const ITargetPlatform* InTargetPlatform) override;
	virtual void InitAudioResource(FByteBulkData& CompressedData) override;
	virtual bool InitAudioResource(FName Format) override;
	virtual int32 GetResourceSizeForFormat(FName Format) override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override { return Audio::EAudioMixerStreamDataFormat::Int16; }
	//~ End USoundWave Interface.

	/** Add data to the FIFO that feeds the audio device. */
	void InitializeAudio(const uint8* AudioData, const int32 BufferSize);

	/** Query bytes queued for playback */
	int32 GetAudioBufferSize();

private:

	mutable TSharedPtr<FCriticalSection> DataMutex;

	bool SetPlaybackTime(float PlaybackTime);
	float GetPlaybackTime() const;
	bool IsPlaybackFinished() const;

	uint32 PlayedNumOfFrames = 0;
};
