// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "Modules/ModuleManager.h"

// BEGIN ESPEAK_LIB HEADER
#define espeakCHARS_AUTO 0

typedef enum {
	EE_OK = 0,
	EE_INTERNAL_ERROR = -1,
	EE_BUFFER_FULL = 1,
	EE_NOT_FOUND = 2
} espeak_ERROR;

typedef enum {
	/* PLAYBACK mode: plays the audio data, supplies events to the calling program*/
	AUDIO_OUTPUT_PLAYBACK,
	/* RETRIEVAL mode: supplies audio data and events to the calling program */
	AUDIO_OUTPUT_RETRIEVAL,
	/* SYNCHRONOUS mode: as RETRIEVAL but doesn't return until synthesis is completed */
	AUDIO_OUTPUT_SYNCHRONOUS,
	/* Synchronous playback */
	AUDIO_OUTPUT_SYNCH_PLAYBACK
} espeak_AUDIO_OUTPUT;
// END HEADER

#if ESPEAK_NG
typedef int(*ptr_espeak_Initialize)(espeak_AUDIO_OUTPUT output, int buflength, const char* path, int options);
typedef espeak_ERROR(*ptr_espeak_Terminate)(void);
typedef espeak_ERROR(*ptr_espeak_SetVoiceByName)(const char* name);
typedef char*(*ptr_espeak_TextToPhonemesWithTerminator)(const void** textptr, int textmode, int phonememode, int* terminator);
#endif

class FLocalTTSModule : public IModuleInterface
{
public:
	static FString GetBinariesPath();
	static FString GetContentPath();

#if ESPEAK_NG
	ptr_espeak_Initialize func_espeak_Initialize;
	ptr_espeak_Terminate func_espeak_Terminate;
	ptr_espeak_SetVoiceByName func_espeak_SetVoiceByName;
	ptr_espeak_TextToPhonemesWithTerminator func_espeak_TextToPhonemesWithTerminator;
#else
	int func_espeak_Initialize(espeak_AUDIO_OUTPUT output, int buflength, const char* path, int options);
	espeak_ERROR func_espeak_Terminate(void);
	espeak_ERROR func_espeak_SetVoiceByName(const char* name);
	const char* func_espeak_TextToPhonemesWithTerminator(const void** textptr, int textmode, int phonememode, int* terminator);
#endif

	static FString GetName() { return TEXT("LocalTTS"); }

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsLoaded() const;

protected:
	/** DLL Handle */
	void* EspeakDllHandle = nullptr;
	bool bLoaded = false;
};
