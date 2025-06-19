// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSEditorModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ISettingsModule.h"
#include "LocalTTSSettings.h"
#include "AssetTypeActions_TTSModelData_Base.h"

#define LOCTEXT_NAMESPACE "FLocalTTSEditor"

void FLocalTTSEditor::StartupModule()
{
	// Type actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_TTSModelData_Base>());
	
	// Register plugin settings
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.RegisterSettings(TEXT("Project"), TEXT("Plugins"), TEXT("LocalTTS"),
		LOCTEXT("LocalTTS", "Local Text-to-Speech"),
		LOCTEXT("LocalTTSConfigs", "Configure speech synthesis settings"),
		GetMutableDefault<UTtsSettings>());
}


void FLocalTTSEditor::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLocalTTSEditor, LocalTTSEditor)
