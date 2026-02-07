// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
// Planet Layer Blend - Material node for biome-based material layer blending.

#include "MaterialExpressionPlanetLayerBlend.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

#if WITH_EDITOR
#include "MaterialGraph/MaterialGraphNode.h"
#endif

#define LOCTEXT_NAMESPACE "ProceduralPlanetGeneration"

UMaterialExpressionPlanetLayerBlend::UMaterialExpressionPlanetLayerBlend(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("PPGCategory", "PPG"));
#endif
}

#if WITH_EDITOR

uint32 UMaterialExpressionPlanetLayerBlend::GetInputType(int32 InputIndex)
{
	// Input 0 (BiomeTexture) must be a Texture Object
	return (InputIndex == 0) ? MCT_Texture : MCT_Float;
}

int32 UMaterialExpressionPlanetLayerBlend::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Compile inputs
	const int32 TexRef = BiomeTexture.IsConnected() ? BiomeTexture.Compile(Compiler) : Compiler->Constant(0.f);
	const int32 UVRef = UV.IsConnected() ? UV.Compile(Compiler) : Compiler->TextureCoordinate(0, false, false);
	const int32 LayerRef = LayerIndex.IsConnected() ? LayerIndex.Compile(Compiler) : Compiler->Constant(0.f);

	// Create custom HLSL node
	UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(this);
	CustomNode->OutputType = CMOT_Float1;
	CustomNode->Inputs.Empty();
	
	FCustomInput Input0; Input0.InputName = TEXT("BiomeTexture"); CustomNode->Inputs.Add(Input0);
	FCustomInput Input1; Input1.InputName = TEXT("UV");           CustomNode->Inputs.Add(Input1);
	FCustomInput Input2; Input2.InputName = TEXT("LayerIndex");   CustomNode->Inputs.Add(Input2);

	TArray<int32> Inputs;
	Inputs.Add(TexRef);
	Inputs.Add(UVRef);
	Inputs.Add(LayerRef);

	// HLSL code for layer blending
	// BiomeMap texture layout:
	//   - Top half (V 0.0-0.5): Material layer indices (RGB) + elevation (A)
	//   - Bottom half (V 0.5-1.0): Biome strengths (RGB) + padding (A)
	//
	// Mesh UVs span 0-1, so we scale V to sample correct half:
	//   - Indices:   UV.y * 0.5         → maps 0-1 to 0-0.5
	//   - Strengths: UV.y * 0.5 + 0.5   → maps 0-1 to 0.5-1.0
	//
	// Blending algorithm:
	//   1. Sample indices and strengths from texture
	//   2. Filter out zero-strength entries (mark as invalid)
	//   3. Sort by index ascending (lowest = base layer)
	//   4. Calculate associative blend alpha based on stack position

	CustomNode->Code = TEXT(R"(
// Sample both texture halves
// UV maps directly to texture coordinates
// Indices are in top half, strengths in bottom half (offset by 0.5 in V)
float2 uvIndices = UV * float2(1, 0.5);
float2 uvStrength = UV * float2(1, 0.5) + float2(0, 0.5);

float4 indicesRaw = Texture2DSample(BiomeTexture, BiomeTextureSampler, uvIndices);
float4 strengthRaw = Texture2DSample(BiomeTexture, BiomeTextureSampler, uvStrength);

// Decode material layer indices (stored as index/255.0 in 8-bit texture)
int3 idx = int3(round(indicesRaw.rgb * 255.0));
float3 str = strengthRaw.rgb;
int currentLayer = int(round(LayerIndex));

// Mark zero-strength slots as invalid (prevents ghost layers from sorting to front)
const int INVALID = 999;
if (str.x < 0.001) idx.x = INVALID;
if (str.y < 0.001) idx.y = INVALID;
if (str.z < 0.001) idx.z = INVALID;

// Bubble sort by material layer index ascending (maintains idx/str pairing)
int ti; float tf;
if (idx.x > idx.y) { ti = idx.x; idx.x = idx.y; idx.y = ti; tf = str.x; str.x = str.y; str.y = tf; }
if (idx.y > idx.z) { ti = idx.y; idx.y = idx.z; idx.z = ti; tf = str.y; str.y = str.z; str.z = tf; }
if (idx.x > idx.y) { ti = idx.x; idx.x = idx.y; idx.y = ti; tf = str.x; str.x = str.y; str.y = tf; }

// Calculate associative blend alpha based on layer stack position
float Alpha = 0.0;

if (currentLayer == idx.x && idx.x != INVALID)
{
    // Base layer (lowest index) = fully opaque
    Alpha = 1.0;
}
else if (currentLayer == idx.y && idx.y != INVALID)
{
    // Second layer blends over base
    float denom = str.x + str.y;
    Alpha = (denom > 0.001) ? (str.y / denom) : 0.0;
}
else if (currentLayer == idx.z && idx.z != INVALID)
{
    // Third layer blends over base + second
    float denom = str.x + str.y + str.z;
    Alpha = (denom > 0.001) ? (str.z / denom) : 0.0;
}

return saturate(Alpha);
)");

	return Compiler->CustomExpression(CustomNode, OutputIndex, Inputs);
}

void UMaterialExpressionPlanetLayerBlend::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Planet Layer Blend"));
}

FString UMaterialExpressionPlanetLayerBlend::GetDescription() const
{
	return TEXT("Calculates material layer blend alpha from BiomeMap texture.\n"
	            "Connect to Layer Blend's Alpha input.\n\n"
	            "Inputs:\n"
	            "  BiomeTexture: Texture Object (BiomeMap)\n"
	            "  UV: Mesh UVs\n"
	            "  LayerIndex: Material layer index (auto-set by runtime)");
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
