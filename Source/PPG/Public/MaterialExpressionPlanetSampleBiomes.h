// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPlanetSampleBiomes.generated.h"

/**
 * Samples custom values through per-biome curves and blends based on biome weights.
 * 
 * Connect Masks from a Biome Mask Output node.
 * Each Arg input should connect to a SampleBiomeData node to fetch per-biome curve values.
 * Outputs the weighted blend of those curve-sampled values based on current biome coverage.
 */
UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetSampleBiomes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Biome masks (float4). Connect from Biome Mask Output node. Each channel encodes 2 masks. */
	UPROPERTY()
	FExpressionInput Masks;

	/** Per-biome value 1 */
	UPROPERTY()
	FExpressionInput Arg1;

	/** Per-biome value 2.*/
	UPROPERTY()
	FExpressionInput Arg2;

	/** Per-biome value 3.*/
	UPROPERTY()
	FExpressionInput Arg3;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
#endif
};
