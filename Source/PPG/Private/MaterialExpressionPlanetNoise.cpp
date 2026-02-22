// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "MaterialExpressionPlanetNoise.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"
#if WITH_EDITOR
#include "MaterialGraph/MaterialGraphNode.h"
#endif

UMaterialExpressionPlanetNoise::UMaterialExpressionPlanetNoise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NoiseType = EPlanetNoiseType::FbmE;
	Octaves = 6;
	RidgePower = 1.0f;
	BaseFrequency = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(FText::FromString(TEXT("PPG")));
	UpdateOutputs();
#endif
}

void UMaterialExpressionPlanetNoise::UpdateOutputs()
{
#if WITH_EDITORONLY_DATA
	NoiseOutputs.Reset();
	if (NoiseType == EPlanetNoiseType::ErosionTrimplanar)
	{
		NoiseOutputs.Add(FExpressionOutput(TEXT("Height + Erosion")));
	}
	else
	{
		NoiseOutputs.Add(FExpressionOutput(TEXT("Result")));
	}
#endif
}

#if WITH_EDITOR

void UMaterialExpressionPlanetNoise::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMaterialExpressionPlanetNoise, NoiseType))
	{
		UpdateOutputs();
		
		if (UMaterialGraphNode* MatGraphNode = Cast<UMaterialGraphNode>(GraphNode))
		{
			MatGraphNode->RecreateAndLinkNode();
		}
	}
}

TArray<FExpressionOutput>& UMaterialExpressionPlanetNoise::GetOutputs()
{
	NoiseOutputs.Reset();
	NoiseOutputs.Add(FExpressionOutput(TEXT("Result")));
	return NoiseOutputs;
}
	 
int32 UMaterialExpressionPlanetNoise::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Enforce single output only
	if (OutputIndex != 0)
	{
		return Compiler->Errorf(TEXT("PlanetNoise only supports a single output (index 0)"));
	}

	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->Inputs.Empty();
	CustomNode->IncludeFilePaths.Add(TEXT("/ComputeShaderShaders/NoiseLib.usf"));

	TArray<int32> Inputs;

	// Position input (always added)
	{
		FCustomInput Input;
		Input.InputName = TEXT("Pos");
		CustomNode->Inputs.Add(Input);
		int32 Index = Position.IsConnected() ? Position.Compile(Compiler) : Compiler->Constant3(0.0f, 0.0f, 0.0f);
		Inputs.Add(Index);
	}

	// Seed input (always added)
	{
		FCustomInput Input;
		Input.InputName = TEXT("Seed");
		CustomNode->Inputs.Add(Input);
		int32 Index = Seed.IsConnected() ? Seed.Compile(Compiler) : Compiler->Constant(0.0f);
		Inputs.Add(Index);
	}

	FString Param1 = FString::FromInt(Octaves);
	FString Param2 = FString::SanitizeFloat(RidgePower);
	FString Freq = FString::SanitizeFloat(BaseFrequency);

	FString Code;
	Code += TEXT("#ifdef PLANET_COMPUTE_SHADER_COMPILE\n");

	if (!Position.IsConnected())
	{
		Code += TEXT("float3 Pos = float3(Parameters.TexCoords[0].x, Parameters.TexCoords[0].y, Parameters.TexCoords[1].x);\n");
	}

	Code += TEXT("float3 SeedOffset = frac(float3(Seed * 17.31, Seed * 43.27, Seed * 89.13));\n");
	Code += FString::Printf(TEXT("float3 ScaledPos = Pos * %s + SeedOffset;\n"), *Freq);

	switch (NoiseType)
	{
	case EPlanetNoiseType::ErosionTrimplanar:
		CustomNode->OutputType = CMOT_Float2;
		Code += TEXT("float erosion; float h = ErosionHeightTriplanar(ScaledPos, 1.0, erosion); return float2(h, erosion);\n");
		break;
	case EPlanetNoiseType::FbmE:
		CustomNode->OutputType = CMOT_Float1;
		Code += FString::Printf(TEXT("return fbmE(ScaledPos, %s, %s);\n"), *Param1, *Param2);
		break;
	case EPlanetNoiseType::Craters:
		CustomNode->OutputType = CMOT_Float1;
		Code += TEXT("return fbmCraters(ScaledPos);\n");
		break;
	case EPlanetNoiseType::Voronoi:
		CustomNode->OutputType = CMOT_Float1;
		Code += TEXT("return Voronoi3D(ScaledPos, Seed).x;\n");
		break;
	case EPlanetNoiseType::Tectonic:
		CustomNode->OutputType = CMOT_Float1;
		Code += FString::Printf(TEXT("return computeInstantPlateEffect(Pos + SeedOffset, %s);\n"), *Freq);
		break;
	}

	Code += TEXT("#else\n");

	if (!Position.IsConnected())
	{
		Code += TEXT("float3 Pos = normalize(LWCToFloat(GetWorldPosition(Parameters)));\n");
	}

	Code += TEXT("float3 SeedOffset = frac(float3(Seed * 17.31, Seed * 43.27, Seed * 89.13));\n");
	Code += FString::Printf(TEXT("float3 ScaledPos = Pos * %s + SeedOffset;\n"), *Freq);

	switch (NoiseType)
	{
	case EPlanetNoiseType::ErosionTrimplanar:
		CustomNode->OutputType = CMOT_Float2;
		Code += TEXT("float erosion; float h = ErosionHeightTriplanar(ScaledPos, 1.0, erosion); return float2(h, erosion);\n");
		break;
	case EPlanetNoiseType::FbmE:
		CustomNode->OutputType = CMOT_Float1;
		Code += FString::Printf(TEXT("return fbmE(ScaledPos, %s, %s);\n"), *Param1, *Param2);
		break;
	case EPlanetNoiseType::Craters:
		CustomNode->OutputType = CMOT_Float1;
		Code += TEXT("return fbmCraters(ScaledPos);\n");
		break;
	case EPlanetNoiseType::Voronoi:
		CustomNode->OutputType = CMOT_Float1;
		Code += TEXT("return Voronoi3D(ScaledPos, Seed).x;\n");
		break;
	case EPlanetNoiseType::Tectonic:
		CustomNode->OutputType = CMOT_Float1;
		Code += FString::Printf(TEXT("return computeInstantPlateEffect(Pos + SeedOffset, %s);\n"), *Freq);
		break;
	}

	Code += TEXT("#endif");

	CustomNode->Code = Code;

	check(Inputs.Num() == CustomNode->Inputs.Num());

	return Compiler->CustomExpression(CustomNode, 0, Inputs);
}

void UMaterialExpressionPlanetNoise::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Planet Noise"));
}
#endif
