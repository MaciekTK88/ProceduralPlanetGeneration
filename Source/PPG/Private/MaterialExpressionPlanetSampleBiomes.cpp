// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski


#include "MaterialExpressionPlanetSampleBiomes.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

UMaterialExpressionPlanetSampleBiomes::UMaterialExpressionPlanetSampleBiomes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(FText::FromString(TEXT("PPG")));
#endif
}

#if WITH_EDITOR

int32 SafeCompile(FExpressionInput& Input, FMaterialCompiler* Compiler, int32 ConstFallback)
{
    if (Input.IsConnected())
    {
        int32 Result = Input.Compile(Compiler);
        return Result >= 0 ? Result : ConstFallback;
    }
    return ConstFallback;
}
 
int32 SafeCompileVec4(FExpressionInput& Input, FMaterialCompiler* Compiler)
{
    if (Input.IsConnected())
    {
        int32 Result = Input.Compile(Compiler);
        return Result >= 0 ? Result : Compiler->Constant4(0.f, 0.f, 0.f, 0.f);
    }
    return Compiler->Constant4(0.f, 0.f, 0.f, 0.f);
}
 
int32 UMaterialExpressionPlanetSampleBiomes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
    // Validate and fallback on error indices to eliminate crash
    int32 MasksIndex = SafeCompileVec4(Masks, Compiler);
    int32 Arg1Index = SafeCompile(Arg1, Compiler, Compiler->Constant(0.f));
    int32 Arg2Index = SafeCompile(Arg2, Compiler, Compiler->Constant(0.f));
    int32 Arg3Index = SafeCompile(Arg3, Compiler, Compiler->Constant(0.f));
 
    UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
    CustomNode->Inputs.Empty();
    CustomNode->OutputType = CMOT_Float3;
    CustomNode->IncludeFilePaths.Add(TEXT("/ComputeShaderShaders/GenerationUtilities.usf"));
 
    CustomNode->Inputs.Add(FCustomInput()); CustomNode->Inputs.Last().InputName = TEXT("Masks");
    CustomNode->Inputs.Add(FCustomInput()); CustomNode->Inputs.Last().InputName = TEXT("Arg1");
    CustomNode->Inputs.Add(FCustomInput()); CustomNode->Inputs.Last().InputName = TEXT("Arg2");
    CustomNode->Inputs.Add(FCustomInput()); CustomNode->Inputs.Last().InputName = TEXT("Arg3");
 
    TArray<int32> Inputs;
    Inputs.Add(MasksIndex);
    Inputs.Add(Arg1Index);
    Inputs.Add(Arg2Index);
    Inputs.Add(Arg3Index);
 
    FString Code = TEXT("return sampleBiomesPacked(Masks, Arg1, Arg2, Arg3);");
    CustomNode->Code = Code;
 
    check(Inputs.Num() == CustomNode->Inputs.Num());
 
    return Compiler->CustomExpression(CustomNode, 0, Inputs);
}

void UMaterialExpressionPlanetSampleBiomes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Sample Biome Curves"));
}

FString UMaterialExpressionPlanetSampleBiomes::GetDescription() const
{
	return TEXT("Samples custom values through per-biome curves and blends based on biome weights.\n\n"
	            "Inputs:\n"
	            "  Masks: Connect from Biome Mask Output node (float4)\n"
	            "  Arg1-3: Connect to per-biome curve data sources\n\n"
	            "Outputs a float3 with weighted curve-sampled blend results (x=Arg1, y=Arg2, z=Arg3).");
}

#endif
