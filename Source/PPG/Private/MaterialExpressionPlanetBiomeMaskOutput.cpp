// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
// Biome Mask Output - Material node for passing biome masks and weights to the planet compute shader.

#include "MaterialExpressionPlanetBiomeMaskOutput.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

#define LOCTEXT_NAMESPACE "ProceduralPlanetGeneration"

UMaterialExpressionPlanetBiomeMaskOutput::UMaterialExpressionPlanetBiomeMaskOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("PPGCategory", "PPG"));
#endif
}

#if WITH_EDITOR
int32 SafeCompileFloat(FExpressionInput& Input, FMaterialCompiler* Compiler)
{
	if (Input.IsConnected())
	{
		int32 Result = Input.Compile(Compiler);
		return Result >= 0 ? Result : Compiler->Constant(0.0f);
	}
	return Compiler->Constant(0.0f);
}
	 
int32 UMaterialExpressionPlanetBiomeMaskOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 M0 = SafeCompileFloat(Mask0, Compiler);
	int32 M1 = SafeCompileFloat(Mask1, Compiler);
	int32 M2 = SafeCompileFloat(Mask2, Compiler);
	int32 M3 = SafeCompileFloat(Mask3, Compiler);
	int32 M4 = SafeCompileFloat(Mask4, Compiler);
	int32 M5 = SafeCompileFloat(Mask5, Compiler);
	int32 M6 = SafeCompileFloat(Mask6, Compiler);
	int32 M7 = SafeCompileFloat(Mask7, Compiler);
	 
	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->Inputs.Empty();
	CustomNode->OutputType = CMOT_Float4;
	 
	for (int32 i = 0; i < 8; ++i)
	{
		CustomNode->Inputs.Add(FCustomInput());
		CustomNode->Inputs[i].InputName = *FString::Printf(TEXT("m%d"), i);
	}
	 
	TArray<int32> Inputs = { M0, M1, M2, M3, M4, M5, M6, M7 };
	 
	// Pack 8 masks as UNORM16 pairs (65535 levels) instead of f16 (~1024 levels at 0.5)
	// to eliminate banding artifacts in biome weight transitions.
	CustomNode->Code = TEXT(
		"return asfloat(uint4("
		"(uint(saturate(m0) * 65535.0 + 0.5) & 0xFFFF) | (uint(saturate(m1) * 65535.0 + 0.5) << 16), "
		"(uint(saturate(m2) * 65535.0 + 0.5) & 0xFFFF) | (uint(saturate(m3) * 65535.0 + 0.5) << 16), "
		"(uint(saturate(m4) * 65535.0 + 0.5) & 0xFFFF) | (uint(saturate(m5) * 65535.0 + 0.5) << 16), "
		"(uint(saturate(m6) * 65535.0 + 0.5) & 0xFFFF) | (uint(saturate(m7) * 65535.0 + 0.5) << 16)"
		"));"
	);
	 
	check(Inputs.Num() == CustomNode->Inputs.Num());
	 
	return Compiler->CustomExpression(CustomNode, 0, Inputs);
}

void UMaterialExpressionPlanetBiomeMaskOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Biome Mask Output"));
}

FString UMaterialExpressionPlanetBiomeMaskOutput::GetDescription() const
{
	return TEXT("Encodes 8 biome mask values (0-1) into a single float4 (packed UNORM16).\n"
	            "Used by Planet Generation Output and Sample Biome Curves for efficient mask passing.");
}

#endif // WITH_EDITOR
