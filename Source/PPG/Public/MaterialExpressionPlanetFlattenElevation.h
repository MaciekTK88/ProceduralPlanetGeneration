// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPlanetFlattenElevation.generated.h"

UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetFlattenElevation : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Flatten)
	float WaterLevel = 0.0f;

	UPROPERTY(EditAnywhere, Category=Flatten)
	float Threshold = 0.005f;

	UPROPERTY()
	FExpressionInput Elevation;

	UPROPERTY()
	FExpressionInput WaterLevelInput;

	UPROPERTY()
	FExpressionInput ThresholdInput;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
	virtual FName GetInputName(int32 InputIndex) const override;
#endif
};
