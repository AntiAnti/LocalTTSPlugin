// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "AssetTypeActions_TTSModelData_Base.h"
#include "Framework/Application/SlateApplication.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "TTSModelData_Base.h"
#include "TTSModelData_Piper.h"
#include "TTSModelData_Kokoro.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "EditorDirectories.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


UClass* FAssetTypeActions_TTSModelData_Base::GetSupportedClass() const
{
	return UTTSModelData_Base::StaticClass();
}

void FAssetTypeActions_TTSModelData_Base::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
}

FColor FAssetTypeActions_TTSModelData_Base::GetTypeColor() const
{
	return FColor(97, 97, 85);
}

uint32 FAssetTypeActions_TTSModelData_Base::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FAssetTypeActions_TTSModelData_Base::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	TSharedRef<FSimpleAssetEditor> Editor = FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	// Imnport UTTSModelData_Piper or UTTSModelData_Kokoro
	Extender->AddMenuExtension(
		TEXT("ViewAssetAudit"),
		EExtensionHook::After,
		MakeShareable(new FUICommandList()),
		FMenuExtensionDelegate::CreateLambda([this, InObjects](FMenuBuilder& MenuBuilder)
		{
			auto Assets = GetTypedWeakObjectPtrs<UTTSModelData_Base>(InObjects);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ImportFromJson", "Import Settings from JSON..."),
				LOCTEXT("ImportFromJsonToolTip", "Import model settings from the description file: [modelname.onnx].json or tokenizer.json"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::ImportFromJson, Assets),
					FCanExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::CanImportFromJson, Assets)
				));
		}));


	for (auto& Obj : InObjects)
	{
		if (UTTSModelData_Kokoro* TTSModelData_Kokoro = Cast<UTTSModelData_Kokoro>(Obj))
		{
			// Import UTTSModelData_Kokoro voice
			Extender->AddMenuExtension(
				TEXT("ViewAssetAudit"),
				EExtensionHook::After,
				MakeShareable(new FUICommandList()),
				FMenuExtensionDelegate::CreateLambda([this, InObjects](FMenuBuilder& MenuBuilder)
				{
					auto Assets = GetTypedWeakObjectPtrs<UTTSModelData_Base>(InObjects);

					MenuBuilder.AddMenuEntry(
						LOCTEXT("ImportVoiceFromBin", "Import Voice from BIN..."),
						LOCTEXT("ImportVoiceFromBinToolTip", "Import voice data from binary (.bin) file"),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::ImportVoiceFromBin, Assets),
							FCanExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::CanImportVoiceFromBin, Assets)
						));
				}));

			if (TTSModelData_Kokoro->Speakers.Num() > 0)
			{
				for (const auto& speaker : TTSModelData_Kokoro->Speakers)
				{
					// Delete UTTSModelData_Kokoro voices
					Extender->AddMenuExtension(
						TEXT("ViewAssetAudit"),
						EExtensionHook::After,
						MakeShareable(new FUICommandList()),
						FMenuExtensionDelegate::CreateLambda([this, TTSModelData_Kokoro, sName = speaker.Key, sID = speaker.Value](FMenuBuilder& MenuBuilder)
						{
							FString MenuLabel = TEXT("Delete Voice \"") + sName + TEXT("\"");
							TWeakObjectPtr<UTTSModelData_Base> obj = TTSModelData_Kokoro;

							MenuBuilder.AddMenuEntry(
								FText::FromString(MenuLabel),
								LOCTEXT("DeleteAllVoicesToolTip", "Delete all voices/speakers imported to this asset"),
								FSlateIcon(),
								FUIAction(
									FExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::DeleteVoice, obj, sID)
								));
						}));
				}

				if (TTSModelData_Kokoro->Speakers.Num() > 1)
				{
					// Delete UTTSModelData_Kokoro voices
					Extender->AddMenuExtension(
						TEXT("ViewAssetAudit"),
						EExtensionHook::After,
						MakeShareable(new FUICommandList()),
						FMenuExtensionDelegate::CreateLambda([this, TTSModelData_Kokoro](FMenuBuilder& MenuBuilder)
						{
							TWeakObjectPtr<UTTSModelData_Base> obj = TTSModelData_Kokoro;

							MenuBuilder.AddMenuEntry(
								LOCTEXT("DeleteAllVoices", "Delete All Voices"),
								LOCTEXT("DeleteAllVoicesToolTip", "Delete all voices/speakers imported to this asset"),
								FSlateIcon(),
								FUIAction(
									FExecuteAction::CreateSP(this, &FAssetTypeActions_TTSModelData_Base::DeleteAllVoices, obj)
								));
						}));
				}
			}
		}
	}

	Editor->AddMenuExtender(Extender);
	Editor->RegenerateMenusAndToolbars();
	Editor->PostRegenerateMenusAndToolbars();
}

void FAssetTypeActions_TTSModelData_Base::ImportFromJson(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects)
{
	IDesktopPlatform* const DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (const auto& Asset : Objects)
	{
		if (Asset.Get()->GetClass()->IsChildOf(UTTSModelData_Base::StaticClass()))
		{
			const FString DefaultPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);

			TArray<FString> OutFiles;
			if (DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				LOCTEXT("SelectJsonFile", "Select JSON file with Piper model settings").ToString(),
				DefaultPath,
				TEXT(""),//TEXT("Curve Table JSON (*.json)|*.json");
				TEXT("JSON Formatted Text (*.json)|*.json"),
				EFileDialogFlags::None,
				OutFiles
			))
			{
				UTTSModelData_Base* ModelData = Cast<UTTSModelData_Base>(Asset.Get());
				if (IsValid(ModelData) && OutFiles.Num() > 0)
				{
					const FScopedTransaction Transaction(LOCTEXT("ImportModelDataJson", "Import TTS Model Data from File"));
					ModelData->Modify();
					ModelData->ImportFromFile(OutFiles[0]);
				}
			}
		}
	}
}

bool FAssetTypeActions_TTSModelData_Base::CanImportFromJson(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects) const
{
	return true;
}

void FAssetTypeActions_TTSModelData_Base::ImportVoiceFromBin(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects)
{
	IDesktopPlatform* const DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (const auto& Asset : Objects)
	{
		if (Asset.Get()->GetClass()->IsChildOf(UTTSModelData_Base::StaticClass()))
		{
			const FString DefaultPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);

			TArray<FString> OutFiles;
			if (DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				LOCTEXT("SelectJsonFile", "Select BIN file cached voice").ToString(),
				DefaultPath,
				TEXT(""),//TEXT("Curve Table JSON (*.json)|*.json");
				TEXT("Binary File (*.bin)|*.bin"),
				EFileDialogFlags::None,
				OutFiles
			))
			{
				UTTSModelData_Kokoro* ModelData = Cast<UTTSModelData_Kokoro>(Asset.Get());
				if (IsValid(ModelData) && OutFiles.Num() > 0)
				{
					const FScopedTransaction Transaction(LOCTEXT("ImportModelDataBin", "Import Kokoro Voice from File"));
					ModelData->Modify();
					ModelData->ImportVoiceFromFile(OutFiles[0]);
				}
			}
		}
	}
}

bool FAssetTypeActions_TTSModelData_Base::CanImportVoiceFromBin(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects) const
{
	return true;
}

void FAssetTypeActions_TTSModelData_Base::DeleteVoice(TWeakObjectPtr<UTTSModelData_Base> Object, int32 VoiceId)
{
	UTTSModelData_Kokoro* TTSModelData_Kokoro = Cast<UTTSModelData_Kokoro>(Object.Get());
	const FScopedTransaction Transaction(LOCTEXT("DeleteModelVoice", "Delete Voice from TTSModelData"));
	TTSModelData_Kokoro->Modify();
	TTSModelData_Kokoro->DeleteSpeaker(VoiceId);
}

void FAssetTypeActions_TTSModelData_Base::DeleteAllVoices(TWeakObjectPtr<UTTSModelData_Base> Object)
{
	UTTSModelData_Kokoro* TTSModelData_Kokoro = Cast<UTTSModelData_Kokoro>(Object.Get());
	const FScopedTransaction Transaction(LOCTEXT("DeleteModelVoices", "Delete All Voices from TTSModelData"));
	TTSModelData_Kokoro->Modify();
	TTSModelData_Kokoro->DeleteAllSpeakers();
}

#undef LOCTEXT_NAMESPACE
