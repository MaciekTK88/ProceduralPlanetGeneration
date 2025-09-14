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
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;   // root scene component
};