// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class LocalTTSEditor : ModuleRules
    {
        public LocalTTSEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
            PrivatePCHHeaderFile = "Public/LocalTTSEditorModule.h";

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Settings",
                    "AssetTools",
                    "UnrealEd",
                    "DesktopPlatform",
                    "LocalTTS"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "CoreUObject",
                    "Engine",
                    "Slate",
                    "SlateCore"
                }
            );
        }
    }
}