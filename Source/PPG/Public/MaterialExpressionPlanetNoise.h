// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPlanetNoise.generated.h"

UENUM(BlueprintType)
enum class EPlanetNoiseType : uint8
{
	FbmE UMETA(DisplayName="FBM (Gradient + Rotation)"),
	Craters UMETA(DisplayName="Craters"),
	Voronoi UMETA(DisplayName="Voronoi"),
	ErosionTrimplanar UMETA(DisplayName="Erosion (Triplanar)"),
	Tectonic UMETA(DisplayName="Tectonic Plates")
};

UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetNoise : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Noise)
	EPlanetNoiseType NoiseType;

	UPROPERTY(EditAnywhere, Category=Noise)
	float BaseFrequency = 1.0f;

	UPROPERTY(EditAnywhere, Category=Noise)
	int32 Octaves = 6;

	UPROPERTY(EditAnywhere, Category=Noise)
	float RidgePower = 2.0f;

	UPROPERTY()
	FExpressionInput Position;

	UPROPERTY()
	FExpressionInput Seed;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual TArray<FExpressionOutput>& GetOutputs() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
#if WITH_EDITORONLY_DATA
	TArray<FExpressionOutput> NoiseOutputs;
#endif

	void UpdateOutputs();
};
