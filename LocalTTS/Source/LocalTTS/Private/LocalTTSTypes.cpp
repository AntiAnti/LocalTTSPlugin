// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSTypes.h"
#include "TTSModelData_Base.h"
#include "LocalTTSSubsystem.h"
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

void FNNEModelTTS::PrepareOutputBuffer(int32 Size)
{
	OutputData.SetNumUninitialized(Size);
	OutputBindings.SetNumZeroed(1);
	OutputBindings[0].Data = OutputData.GetData();
	OutputBindings[0].SizeInBytes = Size * sizeof(float);
}

bool FNNEModelTTS::PrepareInputFloat(int32 Index, const TArray<float>& Data, const TArrayView<const uint32>& Shape)
{
	if (!CheckInParam(Index, ENNETensorDataType::Float))
	{
		return false;
	}

	// input
	TArray<float>& Inputs = GetInParamFloatUnsafe(Index);

	Inputs.SetNumUninitialized(Data.Num());
	FMemory::Memcpy(Inputs.GetData(), Data.GetData(), Data.Num() * (int32)sizeof(float));
	InputBindings[Index].SizeInBytes = (uint64)Inputs.Num() * sizeof(float);
	InputBindings[Index].Data = Inputs.GetData();

	InputTensorShapes[Index] = UE::NNE::FTensorShape::Make(Shape);

	return true;
}

bool FNNEModelTTS::PrepareInputInt64(int32 Index, const TArray<int64>& Data, const TArrayView<const uint32>& Shape)
{
	if (!CheckInParam(Index, ENNETensorDataType::Int64))
	{
		return false;
	}

	// input
	TArray<int64>& Inputs = GetInParamIntUnsafe(Index);

	Inputs.SetNumUninitialized(Data.Num());
	FMemory::Memcpy(Inputs.GetData(), Data.GetData(), Data.Num() * (int32)sizeof(int64));
	InputBindings[Index].SizeInBytes = (uint64)Inputs.Num() * sizeof(int64);
	InputBindings[Index].Data = Inputs.GetData();

	InputTensorShapes[Index] = UE::NNE::FTensorShape::Make(Shape);

	return true;
}

bool FNNEModelTTS::RunNNE(TArray<float>& OutData, TArray<uint32>& OutDataShape, bool bReturnData)
{
	// Ensure we applied input tensor shapes
	ModelInstance->SetInputTensorShapes(InputTensorShapes);

	if (InputBindings.Num() != InputTensorShapes.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid NNE input setup: input tensors num (%d) doesn't match input tensor shapes (%d)"), InputBindings.Num(), InputTensorShapes.Num());
		return false;
	}

	// run
	UE::NNE::IModelInstanceRunSync::ERunSyncStatus RunStatus = ModelInstance->RunSync(InputBindings, OutputBindings);
	if (RunStatus == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Fail)
	{
		return false;
	}

	// get output data
	const auto EncoderOutputsShape = ModelInstance->GetOutputTensorShapes().GetData();
	int32 GeneratedSamplesNum = EncoderOutputsShape->Volume();
	if (GeneratedSamplesNum > OutputData.Num())
	{
#if WITH_EDITOR
		FString OutShape = LocalTtsUtils::PrintArray(EncoderOutputsShape->GetData());
		UE_LOG(LogTemp, Error, TEXT("RunNNE: output buffer size (%d) is smaller then NNM output volume %d, shape (%s). Data is corrupted."), OutputData.Num(), GeneratedSamplesNum, *OutShape);
#endif
		return false;
	}

	if (bReturnData)
	{
		OutData.SetNumUninitialized(GeneratedSamplesNum);
		FMemory::Memcpy(OutData.GetData(), OutputData.GetData(), GeneratedSamplesNum * (int32)sizeof(float));
	}
	OutDataShape = EncoderOutputsShape->GetData();

	return true;
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
	return std::filesystem::exists(TCHAR_TO_ANSI(*d));
#else
	return FPaths::DirectoryExists(Dir);
#endif
}