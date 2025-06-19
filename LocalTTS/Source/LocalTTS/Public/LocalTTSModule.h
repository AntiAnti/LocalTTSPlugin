/*
Copyright (c) 2025 Yuri NK, LocalTSS UE5 Plugin

This plugin is licensed for non-commercial use only.

You are allowed to:

    Use, modify, and distribute this plugin in non-commercial projects.
    Include it in personal, educational, and open-source projects.

You are NOT allowed to:

    Use this plugin in any commercial product or service.
    Sell, sublicense, or include this plugin in paid software, games, or content.
    Use this plugin in projects that generate revenue, directly or indirectly.

Commercial use requires a separate commercial license.
To obtain one, go to Fab.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
*/

#pragma once

#include "Modules/ModuleManager.h"

#define espeakCHARS_AUTO   0

#if PLATFORM_WINDOWS
// BEGIN C HEADER
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

typedef int(*ptr_espeak_Initialize)(espeak_AUDIO_OUTPUT output, int buflength, const char* path, int options);
typedef espeak_ERROR(*ptr_espeak_Terminate)(void);
typedef espeak_ERROR(*ptr_espeak_SetVoiceByName)(const char* name);
typedef char*(*ptr_espeak_TextToPhonemesWithTerminator)(const void** textptr, int textmode, int phonememode, int* terminator);
// END HEADER
#elif PLATFORM_ANDROID
#include "espeak-ng/speak_lib.h"
#endif

class FLocalTTSModule : public IModuleInterface
{
public:
	static FString GetBinariesPath();
	static FString GetContentPath();

#if PLATFORM_WINDOWS
	ptr_espeak_Initialize func_espeak_Initialize;
	ptr_espeak_Terminate func_espeak_Terminate;
	ptr_espeak_SetVoiceByName func_espeak_SetVoiceByName;
	ptr_espeak_TextToPhonemesWithTerminator func_espeak_TextToPhonemesWithTerminator;
#elif PLATFORM_ANDROID
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
