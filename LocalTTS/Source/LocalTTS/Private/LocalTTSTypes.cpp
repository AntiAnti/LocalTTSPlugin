// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSTypes.h"
#include "TTSModelData_Base.h"
#include "NNERuntimeCPU.h"
#include "Misc/Paths.h"

#if PLATFORM_ANDROID
#include <filesystem>
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProcess.h"
#include "Android/AndroidPlatformFile.h"
#include "Misc/App.h"
#endif


TArray<int64>& FNNEModelTTS::GetInParamIntUnsafe(const int32 Index)
{
    return InputDataInt64[InputMap[Index].ArrayIndex];
}

TArray<float>& FNNEModelTTS::GetInParamFloatUnsafe(const int32 Index)
{
    return InputDataFloat[InputMap[Index].ArrayIndex];
}

bool FNNEModelTTS::CheckInParam(const int32 Index, ENNETensorDataType Type) const
{
    return InputMap.IsValidIndex(Index) && InputMap[Index].Format == Type;
}

void PlatformFileUtils::NormalizePath(FString& Path)
{
	Path.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	Path.ReplaceInline(TEXT("//"), TEXT("/"), ESearchCase::CaseSensitive);
	Path.ReplaceInline(TEXT("/./"), TEXT("/"), ESearchCase::CaseSensitive);
}

FString PlatformFileUtils::GetPlatformPath(FString Path)
{
	NormalizePath(Path);

#if PLATFORM_ANDROID
	auto& PlatformFile = IAndroidPlatformFile::GetPlatformPhysical();

	while (Path.StartsWith(TEXT("../"), ESearchCase::CaseSensitive))
	{
		Path.RightChopInline(3, EAllowShrinking::No);
	}
	Path.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
	if (Path.Equals(TEXT(".."), ESearchCase::CaseSensitive))
	{
		Path = TEXT("");
	}

	// BasePath = GFilePathBase/UnrealGame/FApp::GetProjectName()/
	FString BasePath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(TEXT("../"));
	// Avoid duplication
	if (!Path.StartsWith(BasePath))
	{
		Path = BasePath / Path;
	}

	NormalizePath(Path);
	return Path;
#else
	return FPaths::ConvertRelativePathToFull(Path);
#endif
}

bool PlatformFileUtils::DirectoryExists(const FString& Dir)
{
#if PLATFORM_ANDROID
	FString d = PlatformFileUtils::GetPlatformPath(Dir);
	if (d.EndsWith(TEXT("/")))
	{
		d.LeftChopInline(1);
	}

	UE_LOG(LogTemp, Log, TEXT("PlatformFileUtils::DirectoryExists(%s)"), *d);

	return std::filesystem::exists(TCHAR_TO_ANSI(*d));
#else
	return FPaths::DirectoryExists(Dir);
#endif
}