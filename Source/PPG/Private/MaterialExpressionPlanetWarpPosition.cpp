// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "MaterialExpressionPlanetWarpPosition.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

UMaterialExpressionPlanetWarpPosition::UMaterialExpressionPlanetWarpPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WarpStrength = 0.25f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(FText::FromString(TEXT("PPG")));
#endif
}

#if WITH_EDITOR

int32 UMaterialExpressionPlanetWarpPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->Inputs.Empty();
	CustomNode->IncludeFilePaths.Add(TEXT("/ComputeShaderShaders/NoiseLib.usf"));
	CustomNode->OutputType = CMOT_Float3;

	TArray<int32> Inputs;

	// Provide fallback zero vector if Position is disconnected
	int32 PositionIndex = Position.IsConnected() ? Position.Compile(Compiler) : Compiler->Constant3(0.0f, 0.0f, 0.0f);
	Inputs.Add(PositionIndex);

	FCustomInput PositionInput;
	PositionInput.InputName = TEXT("Pos");
	CustomNode->Inputs.Add(PositionInput);

	FString WarpStr = FString::SanitizeFloat(WarpStrength);

	FString Code = FString::Printf(TEXT("return warpPosition(Pos, %s);"), *WarpStr);

	CustomNode->Code = Code;

	return Compiler->CustomExpression(CustomNode, 0, Inputs);
}

void UMaterialExpressionPlanetWarpPosition::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Warp Position (%.2f)"), WarpStrength));
}

FString UMaterialExpressionPlanetWarpPosition::GetDescription() const
{
	return TEXT("Applies additional warp to a sphere coordinate for variation.\n\n"
	            "Uses FBM noise to offset the position, creating organic distortion.\n"
	            "WarpStrength controls the magnitude of displacement.");
}

#endif // WITH_EDITOR
