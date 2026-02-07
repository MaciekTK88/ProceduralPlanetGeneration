// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
// Planet Generation Output - Outputs elevation and material layer masks to the planet compute shader.

#include "MaterialExpressionPlanetElevationOutput.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "ProceduralPlanetGeneration"

UMaterialExpressionPlanetElevationOutput::UMaterialExpressionPlanetElevationOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("PPGCategory", "PPG"));
#endif
}

#if WITH_EDITOR

int32 UMaterialExpressionPlanetElevationOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// OutputIndex 0: Elevation maps to suffix 1
	// OutputIndex 1: Masks maps to suffix 2

	if (OutputIndex == 0)
	{
		int32 ElevationCode = Elevation.IsConnected() ? Elevation.Compile(Compiler) : Compiler->Constant(0.0f);
		return Compiler->CustomOutput(this, 1, ElevationCode);
	}
	else if (OutputIndex == 1)
	{
		int32 MasksCode = Masks.IsConnected() ? Masks.Compile(Compiler) : Compiler->Constant4(0.f, 0.f, 0.f, 0.f);
		return Compiler->CustomOutput(this, 2, MasksCode);
	}

	// Invalid OutputIndex requested
	return INDEX_NONE;
}

void UMaterialExpressionPlanetElevationOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Planet Generation Output"));
}

FString UMaterialExpressionPlanetElevationOutput::GetDescription() const
{
	return TEXT("Outputs final elevation and material layer masks to the planet compute shader.\n\n"
	            "Connect your final calculated elevation to the Elevation input.\n"
	            "Connect Masks from a Biome Mask Output node for material layer blending.");
}

FName UMaterialExpressionPlanetElevationOutput::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0) return TEXT("Elevation");
	if (InputIndex == 1) return TEXT("Masks");
	return NAME_None;
}

#endif // WITH_EDITOR

int32 UMaterialExpressionPlanetElevationOutput::GetNumOutputs() const
{
	return 2;
}

FString UMaterialExpressionPlanetElevationOutput::GetFunctionName() const
{
	// This generates GetElevation1() and GetElevation2() in the shader
	return TEXT("GetElevation");
}

FString UMaterialExpressionPlanetElevationOutput::GetDisplayName() const
{
	return TEXT("Planet Generation Output");
}

#undef LOCTEXT_NAMESPACE
