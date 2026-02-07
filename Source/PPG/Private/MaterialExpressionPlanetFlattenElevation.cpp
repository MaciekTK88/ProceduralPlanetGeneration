// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "MaterialExpressionPlanetFlattenElevation.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

UMaterialExpressionPlanetFlattenElevation::UMaterialExpressionPlanetFlattenElevation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterLevel = 0.0f;
	Threshold = 0.005f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(FText::FromString(TEXT("PPG")));
#endif
}

#if WITH_EDITOR

int32 UMaterialExpressionPlanetFlattenElevation::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->Inputs.Empty();
	CustomNode->IncludeFilePaths.Add(TEXT("/ComputeShaderShaders/NoiseLib.usf"));
	CustomNode->OutputType = CMOT_Float1;
	 
	TArray<int32> Inputs;
	 
	// Elevation input — required
	int32 ElevationIndex = Elevation.IsConnected() ? Elevation.Compile(Compiler) : Compiler->Constant(0.0f);
	Inputs.Add(ElevationIndex);
	{
		FCustomInput Input;
		Input.InputName = TEXT("Elev");
		CustomNode->Inputs.Add(Input);
	}
	 
	// WaterLevel input — fallback to property if disconnected
	int32 WaterLevelIndex = WaterLevelInput.IsConnected() ? WaterLevelInput.Compile(Compiler) : Compiler->Constant(WaterLevel);
	Inputs.Add(WaterLevelIndex);
	{
		FCustomInput Input;
		Input.InputName = TEXT("Water");
		CustomNode->Inputs.Add(Input);
	}
	 
	// Threshold input — fallback to property if disconnected
	int32 ThresholdIndex = ThresholdInput.IsConnected() ? ThresholdInput.Compile(Compiler) : Compiler->Constant(Threshold);
	Inputs.Add(ThresholdIndex);
	{
		FCustomInput Input;
		Input.InputName = TEXT("Thresh");
		CustomNode->Inputs.Add(Input);
	}
	 
	// Always use all inputs with fixed HLSL call and named parameters for clarity
	CustomNode->Code = TEXT("return flattenElevation(Elev, Water, Thresh);");
	 
	check(Inputs.Num() == CustomNode->Inputs.Num());
	 
	return Compiler->CustomExpression(CustomNode, 0, Inputs);
}

void UMaterialExpressionPlanetFlattenElevation::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Flatten Elevation"));
}

FString UMaterialExpressionPlanetFlattenElevation::GetDescription() const
{
	return TEXT("Smooths elevation near water level to create natural coastlines.\n\n"
	            "Elevation: Input height value to flatten.\n"
	            "WaterLevel: The level to flatten towards (default 0).\n"
	            "Threshold: Width of the smoothing zone.");
}

FName UMaterialExpressionPlanetFlattenElevation::GetInputName(int32 InputIndex) const
{
	switch (InputIndex)
	{
		case 0: return TEXT("Elevation");
		case 1: return TEXT("Water Level");
		case 2: return TEXT("Threshold");
		default: return NAME_None;
	}
}

#endif // WITH_EDITOR
