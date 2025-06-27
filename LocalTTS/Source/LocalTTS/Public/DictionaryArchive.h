// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "Engine/DataAsset.h"
#include "DictionaryArchive.generated.h"

/**
 * Word-to-phonemes map serialized as ZIP archive
 */
UCLASS(BlueprintType)
class LOCALTTS_API UDictionaryArchive : public UDataAsset
{
	GENERATED_BODY()
	
public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface
	
	// Binary archive to save in uasset
	// Main data storage
	FByteBulkData ArchivedData;

	// Size of data (just for visual representation, because ArchivedData is hidden in blueprints)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data Info")
	int64 Size = 0;

	// Name of the imported file
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data Info")
	FString ImportFileName;

	// Function to create the object from zip archive with CSV file
	bool InitializeFromFile(const FString& FileName);

	// Extract ZIP archive on disk in the specified directory
	void Unzip();

	// Fill G2PData from csv file data
	void FillMap(const char* Data, int64 DataSize);

	// Does the asset contain zip data?
	bool HasBulkData() const;

	// Was G2PData unzipped?
	bool IsDictionaryReady() const;

	// Find phonemes for the input word
	const FString* Find(const FString& Word) const
	{
		return G2PData->Find(Word);
	}

private:

	// Phonemization dictionary unzipped in runtime and shouldn't be serialized
	TUniquePtr<TMap<FString, FString>> G2PData = nullptr;

	// Archive header
	void* ZipPtr = nullptr;
	// Path to unzip
	int32 ZipEntriesNum = 0;
	int64 UnzippedSize = 0;
	int64 FullArchiveSize = 0;
};
