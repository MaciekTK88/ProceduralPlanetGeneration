// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
// Planet Position - Material node providing planet-space vertex position for noise sampling.

#include "MaterialExpressionPlanetPosition.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

UMaterialExpressionPlanetPosition::UMaterialExpressionPlanetPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(FText::FromString(TEXT("PPG")));
#endif
}

#if WITH_EDITOR

int32 UMaterialExpressionPlanetPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Force allocation of TexCoords 0 and 1 in the shader
	Compiler->TextureCoordinate(0, false, false);
	Compiler->TextureCoordinate(1, false, false);

	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->OutputType = CMOT_Float3;
	CustomNode->Inputs.Empty();

	CustomNode->Code = TEXT(
		"#if PLANET_COMPUTE_SHADER_COMPILE\n"
		"    return float3(Parameters.TexCoords[0].x, Parameters.TexCoords[0].y, Parameters.TexCoords[1].x);\n"
		"#else\n"
		"    return normalize(LWCToFloat(GetWorldPosition(Parameters)));\n"
		"#endif"
	);

	TArray<int32> EmptyInputs;
	return Compiler->CustomExpression(CustomNode, 0, EmptyInputs);
}

void UMaterialExpressionPlanetPosition::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Planet Position"));
}

FString UMaterialExpressionPlanetPosition::GetDescription() const
{
	return TEXT("Returns the normalized sphere direction (unit sphere, -1 to +1).\n"
	            "Use this for noise sampling in the Generation Material.\n\n");
}

#endif // WITH_EDITOR
