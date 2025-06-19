// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FLocalTTSModule"

FString FLocalTTSModule::GetBinariesPath()
{
	FString PluginBinariesDir;
#if WITH_EDITOR
	auto ThisPlugin = IPluginManager::Get().FindPlugin(GetName());
	if (ThisPlugin.IsValid())
	{
		PluginBinariesDir = FPaths::ConvertRelativePathToFull(ThisPlugin->GetBaseDir()) / TEXT("Source/ThirdParty/espeak/Binaries/Win64");
	}
	else
	{
		PluginBinariesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Binaries/ThirdParty/espeak");
	}
#else
	PluginBinariesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Binaries/ThirdParty/espeak");
#endif
	return PluginBinariesDir;
}

FString FLocalTTSModule::GetContentPath()
{
	FString PluginContentDir;
#if WITH_EDITOR
	auto ThisPlugin = IPluginManager::Get().FindPlugin(GetName());
	if (ThisPlugin.IsValid())
	{
		PluginContentDir = FPaths::ConvertRelativePathToFull(ThisPlugin->GetContentDir());
	}
	else
	{
		PluginContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	}
#else
	PluginContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
#endif
	if (!FPaths::DirectoryExists(PluginContentDir / TEXT("NonUFS")))
	{
		if (FPaths::DirectoryExists(FPaths::ProjectContentDir() / TEXT("NonUFS")))
		{
			PluginContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		}
	}

	return PluginContentDir;
}

#if PLATFORM_ANDROID
int FLocalTTSModule::func_espeak_Initialize(espeak_AUDIO_OUTPUT output, int buflength, const char* path, int options)
{
	return espeak_Initialize(output, buflength, path, options);
}

espeak_ERROR FLocalTTSModule::func_espeak_Terminate(void)
{
	return espeak_Terminate();
}

espeak_ERROR FLocalTTSModule::func_espeak_SetVoiceByName(const char* name)
{
	return espeak_SetVoiceByName(name);
}

const char* FLocalTTSModule::func_espeak_TextToPhonemesWithTerminator(const void** textptr, int textmode, int phonememode, int* terminator)
{
	//return espeak_TextToPhonemesWithTerminator(textptr, textmode, phonememode, terminator);
	return espeak_TextToPhonemes(textptr, textmode, phonememode);
}
#endif

void FLocalTTSModule::StartupModule()
{
	FString PluginBinariesDir = GetBinariesPath();
	UE_LOG(LogTemp, Log, TEXT("Espeak_ng Third-party DLLs Directory: %s"), *PluginBinariesDir);
	FPlatformProcess::PushDllDirectory(*PluginBinariesDir);

#if PLATFORM_WINDOWS
	FString FilePath = PluginBinariesDir / TEXT("libespeak-ng.dll");

	if (FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("FLocalTTSModule: Loading torch wrapper from %s"), *FilePath);
		EspeakDllHandle = FPlatformProcess::GetDllHandle(*FilePath);

		if (EspeakDllHandle != NULL)
		{
			func_espeak_Initialize = (ptr_espeak_Initialize)FPlatformProcess::GetDllExport(EspeakDllHandle, TEXT("espeak_Initialize"));
			if (func_espeak_Initialize == NULL)
			{
				UE_LOG(LogTemp, Error, TEXT("FLocalTTSModule: can't find entry point for function espeak_Initialize"));
				return;
			}
			func_espeak_Terminate = (ptr_espeak_Terminate)FPlatformProcess::GetDllExport(EspeakDllHandle, TEXT("espeak_Terminate"));
			if (func_espeak_Terminate == NULL)
			{
				UE_LOG(LogTemp, Error, TEXT("FLocalTTSModule: can't find entry point for function espeak_Terminate"));
				return;
			}
			func_espeak_SetVoiceByName = (ptr_espeak_SetVoiceByName)FPlatformProcess::GetDllExport(EspeakDllHandle, TEXT("espeak_SetVoiceByName"));
			if (func_espeak_SetVoiceByName == NULL)
			{
				UE_LOG(LogTemp, Error, TEXT("FLocalTTSModule: can't find entry point for function espeak_SetVoiceByName"));
				return;
			}
			func_espeak_TextToPhonemesWithTerminator = (ptr_espeak_TextToPhonemesWithTerminator)FPlatformProcess::GetDllExport(EspeakDllHandle, TEXT("espeak_TextToPhonemesWithTerminator"));
			if (func_espeak_TextToPhonemesWithTerminator == NULL)
			{
				UE_LOG(LogTemp, Error, TEXT("FLocalTTSModule: can't find entry point for function espeak_TextToPhonemesWithTerminator"));
				return;
			}
			bLoaded = true;
		}
	}
#elif PLATFORM_ANDROID
	bLoaded = true;
#endif
}

void FLocalTTSModule::ShutdownModule()
{
	if (EspeakDllHandle)
	{
		FPlatformProcess::FreeDllHandle(EspeakDllHandle);
		bLoaded = false;
	}
}

bool FLocalTTSModule::IsLoaded() const
{
	return bLoaded;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLocalTTSModule, LocalTTS)