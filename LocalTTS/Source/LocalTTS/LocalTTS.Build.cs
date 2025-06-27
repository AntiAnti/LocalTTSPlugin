// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

using UnrealBuildTool;
using System.IO;

public class LocalTTS : ModuleRules
{
	private string ThirdPartyEspeak
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/espeak")); }
    }
    private string ThirdPartyUni
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/uni_algo")); }
    }

	public LocalTTS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// **************************************************
		// ESPEAK-NG LIBRARY
		bool bUseEspeak = true;
        // ESPEAK-NG LIBRARY
        // **************************************************

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.Combine(ThirdPartyUni, "Include"),
                Path.Combine(ModuleDirectory, "../ThirdParty/miniz")
            }
		);
        if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicIncludePaths.Add(Path.Combine(ThirdPartyEspeak, "Include"));
        }

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"NNE"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Json",
				"AudioPlatformConfiguration",
                "AudioExtensions"
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);

		if (bUseEspeak)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("ESPEAK_NG=1");
			}
			else
			{
                PublicDefinitions.Add("ESPEAK_NG=0");
            }
            if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// copy all DLLs to the packaged build
                if (!Target.bBuildEditor && Target.Type == TargetType.Game)
				{
					string BinariesPath = Path.Combine(ThirdPartyEspeak, "Binaries", "Win64");
					string DllDestinationDir = "$(ProjectDir)/Binaries/ThirdParty/espeak";

					string[] DLLs = { "libespeak-ng.dll" };

					// Copy DLLs to the target project's executable directory
					foreach (string FileName in DLLs)
					{
						RuntimeDependencies.Add(Path.Combine(DllDestinationDir, FileName), Path.Combine(BinariesPath, FileName));
					}
				}

				if (!Target.bBuildEditor)
				{
					string PluginContentPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../Content/NonUFS/espeak-ng-data/*"));
					string ProjectContentPath = "$(ProjectDir)/Content/NonUFS/espeak-ng-data/*";
					RuntimeDependencies.Add(ProjectContentPath, PluginContentPath, StagedFileType.NonUFS);
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Add UPL to add configrules.txt to our APK
				string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModulePath, "LocalTTS_UPL.xml"));

				string AndroidLibPath = Path.Combine(ThirdPartyEspeak, "Binaries", "Android");
				string Architecture = "arm64-v8a"; // Target.Architecture.ToString()

				PublicAdditionalLibraries.Add(Path.Combine(AndroidLibPath, Architecture, "libttsespeak.so"));
			}
		}
		else
		{
            PublicDefinitions.Add("ESPEAK_NG=0");
        }
    }
}
