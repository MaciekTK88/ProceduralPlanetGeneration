// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FoliageSpawner.generated.h"

UCLASS()
class PPG_API AFoliageSpawner : public AActor
{
	GENERATED_BODY()

public:
	AFoliageSpawner();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	TObjectPtr<USceneComponent> Root;
};