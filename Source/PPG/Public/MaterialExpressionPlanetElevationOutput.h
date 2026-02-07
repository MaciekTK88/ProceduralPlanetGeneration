// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
// Planet Generation Output - Outputs elevation and material layer masks to the planet compute shader.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionPlanetElevationOutput.generated.h"

/**
 * Outputs the final elevation and material layer masks to the planet compute shader.
 * 
 * Connect your final calculated elevation to the Elevation input.
 * Connect biome masks from a Biome Mask Output node for material layer blending.
 */
UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetElevationOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	/** Final elevation value to output. */
	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Elevation;

	/** Biome masks (float4). Connect from Biome Mask Output node. */
	UPROPERTY()
	FExpressionInput Masks;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
	virtual FName GetInputName(int32 InputIndex) const override;
#endif

	//~ Begin UMaterialExpressionCustomOutput Interface
	virtual int32 GetNumOutputs() const override;
	virtual FString GetFunctionName() const override;
	virtual FString GetDisplayName() const override;
	//~ End UMaterialExpressionCustomOutput Interface
};
