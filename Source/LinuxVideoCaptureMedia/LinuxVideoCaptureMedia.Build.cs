using UnrealBuildTool;

public class LinuxVideoCaptureMedia : ModuleRules
{
	public LinuxVideoCaptureMedia(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"Media",
				"MediaUtils",
				"RenderCore",
				"RHI"
			}
		);
	}
}

