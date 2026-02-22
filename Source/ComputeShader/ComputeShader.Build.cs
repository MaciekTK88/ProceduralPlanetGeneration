using System.IO;
using UnrealBuildTool; 

public class ComputeShader: ModuleRules 

{ 

	public ComputeShader(ReadOnlyTargetRules Target) : base(Target) 

	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("ComputeShader/Private");

		string RendererPrivatePath = Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private");
		if (Directory.Exists(RendererPrivatePath))
		{
			PrivateIncludePaths.Add(RendererPrivatePath);
		}
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("TargetPlatform");
		}
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("Engine");
		PublicDependencyModuleNames.Add("MaterialShaderQualitySettings");
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"RenderCore",
			"RHI",
			"Projects",
			"Renderer"
		});
		
		if (Target.bBuildEditor == true)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
					"SlateCore",
					"Slate"
				}
			);

			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
				}
			);
		}
	} 

}