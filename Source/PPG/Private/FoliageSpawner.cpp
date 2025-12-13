// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "FoliageSpawner.h"


// Sets default values
AFoliageSpawner::AFoliageSpawner()
{
	// Set up root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}



