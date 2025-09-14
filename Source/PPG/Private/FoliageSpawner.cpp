// Fill out your copyright notice in the Description page of Project Settings.

#include "FoliageSpawner.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

// Sets default values
AFoliageSpawner::AFoliageSpawner()
{
	// Set up root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}



