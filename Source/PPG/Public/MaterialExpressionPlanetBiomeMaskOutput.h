// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionPlanetBiomeMaskOutput.generated.h"

/**
 * Outputs biome mask values and calculates weights for the planet compute shader.
 * 
 * Connect mask values (0-1) to Mask0-7 inputs.
 * The Weights output provides calculated blend weights - connect to SampleBiomes node.
 */
UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetBiomeMaskOutput : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Mask0;
	UPROPERTY()
	FExpressionInput Mask1;
	UPROPERTY()
	FExpressionInput Mask2;
	UPROPERTY()
	FExpressionInput Mask3;
	UPROPERTY()
	FExpressionInput Mask4;
	UPROPERTY()
	FExpressionInput Mask5;
	UPROPERTY()
	FExpressionInput Mask6;
	UPROPERTY()
	FExpressionInput Mask7;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
#endif
};
