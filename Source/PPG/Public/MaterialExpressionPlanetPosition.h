// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPlanetPosition.generated.h"

UCLASS(CollapseCategories, HideCategories=Object, MinimalAPI)
class UMaterialExpressionPlanetPosition : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetDescription() const override;
#endif
};
