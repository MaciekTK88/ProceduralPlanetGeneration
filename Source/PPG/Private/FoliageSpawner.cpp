// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "FoliageSpawner.h"


// Sets default values
AFoliageSpawner::AFoliageSpawner()
{
	// Set up root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}



