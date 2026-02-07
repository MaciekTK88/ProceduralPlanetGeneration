// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPlanetLayerBlend.generated.h"

UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetLayerBlend : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput BiomeTexture;

	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Texture Coordinates"))
	FExpressionInput UV;

	UPROPERTY()
	FExpressionInput LayerIndex;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif
};
