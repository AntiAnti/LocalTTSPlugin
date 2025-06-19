// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Widgets/SWindow.h"
#include "AssetTypeActions_Base.h"

class UTTSModelData_Base;

class FAssetTypeActions_TTSModelData_Base : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TTSModelData", "TTS Model Data"); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	// end IAssetTypeActions Implementation
private:
	
	void ImportFromJson(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects);
	bool CanImportFromJson(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects) const;

	void ImportVoiceFromBin(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects);
	bool CanImportVoiceFromBin(TArray<TWeakObjectPtr<UTTSModelData_Base>> Objects) const;

	void DeleteVoice(TWeakObjectPtr<UTTSModelData_Base> Object, int32 VoiceId);
	void DeleteAllVoices(TWeakObjectPtr<UTTSModelData_Base> Object);
};
