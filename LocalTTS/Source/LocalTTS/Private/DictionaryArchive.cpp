// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "DictionaryArchive.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "LocalTTSTypes.h"

#if !defined(__ORDER_LITTLE_ENDIAN__)
#define __ORDER_LITTLE_ENDIAN__ PLATFORM_LITTLE_ENDIAN
#endif

#pragma warning( push )
#pragma warning( disable : 4334)

#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#endif

#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#define malloc(Count)				FMemory::Malloc(Count)
#define free(Original)				FMemory::Free(Original)
#define realloc(Original, Count)	FMemory::Realloc(Original, Count)
#define memset(Dest, Char, Count)	FMemory::Memset(Dest, Char, Count)
#define memcpy(Dest, Source, Count)	FMemory::Memcpy(Dest, Source, Count)

THIRD_PARTY_INCLUDES_START
#if PLATFORM_PS4 || PLATFORM_PS5
#define MINIZ_NO_TIME
#endif
#include "miniz.h"
#include "miniz.c"
THIRD_PARTY_INCLUDES_END

#undef malloc
#undef free
#undef realloc
#undef memset
#undef memcpy

#pragma warning( pop )

//--------------------------------------------------------------------------------------------------------

void UDictionaryArchive::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    ArchivedData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload | BULKDATA_Size64Bit);
    ArchivedData.Serialize(Ar, this, INDEX_NONE, false);
}

void UDictionaryArchive::BeginDestroy()
{
    Super::BeginDestroy();
	
    if (G2PData.IsValid())
    {
        G2PData->Empty();
        G2PData.Reset();
    }

    // It shouldn't happen unless the app was closed in a process of unzipping
    if (ZipPtr)
    {
        FMemory::Free(ZipPtr);
        ZipPtr = nullptr;
    }
}

bool UDictionaryArchive::InitializeFromFile(const FString& FileName /* lang_file.zip { lang_file.csv } */)
{
    TArray64<uint8> TempBuffer;

    if (FPaths::FileExists(FileName))
    {
        if (FFileHelper::LoadFileToArray(TempBuffer, *FileName))
        {
            ImportFileName = FPaths::GetBaseFilename(FileName) + TEXT(".csv");
            Size = TempBuffer.Num();

            // Fill BulkData
            ArchivedData.Lock(LOCK_READ_WRITE);
            void* DataPtr = ArchivedData.Realloc(TempBuffer.Num());
            FMemory::Memcpy(DataPtr, TempBuffer.GetData(), TempBuffer.Num());
            ArchivedData.Unlock();

            Unzip();

            return true;
        }
    }
    return false;
}

void UDictionaryArchive::Unzip()
{
    mz_zip_archive* pZip = (mz_zip_archive*)(FMemory::Memzero(FMemory::Malloc(sizeof(mz_zip_archive)), sizeof(mz_zip_archive)));
    ZipPtr = pZip;

    // Read data
    char* DataPtr = nullptr;
    int32 DataSize = ArchivedData.GetBulkDataSize();
    ArchivedData.GetCopy(reinterpret_cast<void**>(&DataPtr));    

    if (!mz_zip_reader_init_mem(pZip, DataPtr, DataSize, MZ_ZIP_FLAG_WRITE_ZIP64))
    {
        UE_LOG(LogTemp, Warning, TEXT("Couldn't load zip archive from the binary data"));
        pZip = nullptr;
        return;
    }

    int32 EntriesNum = mz_zip_reader_get_num_files(pZip);
    if (EntriesNum <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Archive is empty :("));
        FMemory::Free(ZipPtr);
        pZip = nullptr;
        return;

    }

    bool bDirectoriesThreeError = false;

    ZipEntriesNum = EntriesNum;
    FullArchiveSize = 0;
    int32 Index = 0;
    {
        // Get file information
        mz_zip_archive_file_stat EntryDesc;
        const bool bResult = static_cast<bool>(mz_zip_reader_file_stat(pZip, (mz_uint)Index, &EntryDesc));
        if (!bResult)
        {
            FMemory::Free(ZipPtr);
            ZipPtr = nullptr;
            return;
        }

        int32 IsDirectory = (int32)EntryDesc.m_is_directory;
        FullArchiveSize += (int64)EntryDesc.m_comp_size;

        FString FileName = ANSI_TO_TCHAR(EntryDesc.m_filename);

        // Create directory
        if (!IsDirectory)
        {
            UnzippedSize += (int64)EntryDesc.m_comp_size;

            // Extracting to memory
            size_t EntrySize;
            uint8* EntryDataPtr = (uint8*)mz_zip_reader_extract_to_heap(pZip, (mz_uint)Index, &EntrySize, 0);

            if (!EntryDataPtr)
            {
                FString EntryName = ANSI_TO_TCHAR(EntryDesc.m_filename);
                UE_LOG(LogTemp, Warning, TEXT("Unable to extract zip entry '%s' into memory"), *EntryName);
                FMemory::Free(ZipPtr);
                ZipPtr = nullptr;
                return;
            }

            FillMap((char*)EntryDataPtr, EntrySize);

            FMemory::Free(EntryDataPtr);
        }
    }

    FMemory::Free(ZipPtr);
    FMemory::Free(DataPtr);
    pZip = nullptr;
}

void UDictionaryArchive::FillMap(const char* Data, int64 DataSize)
{
    FString DataRaw = UTF8_TO_TCHAR(Data);

    TArray<FString> CsvLines;
    DataRaw.ParseIntoArrayLines(CsvLines);

    if (!G2PData)
    {
        G2PData = MakeUnique<TMap<FString, FString>>();
    }

    G2PData->Empty();
    int32 Counter = 0;
    for (const auto& p : CsvLines)
    {
        if (p == TEXT("word;phoneme")) continue;
        int32 ind = p.Find(TEXT(";"));
        if (ind > 0)
        {
            FString Key = p.Left(ind);
            FString Val = p.Right(p.Len() - ind - 1);
            G2PData->Add(Key, Val);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("Skipping line #%d (%s)"), Counter, *p);;
        }
        Counter++;
    }
    UE_LOG(LogTemp, Log, TEXT("Phonemization dictionary: %d lines deserialized"), G2PData->Num());
}

bool UDictionaryArchive::HasBulkData() const
{
    return ArchivedData.GetBulkDataSize() > 0;
}

bool UDictionaryArchive::IsDictionaryReady() const
{
    return G2PData.IsValid() && !G2PData->IsEmpty();
}
