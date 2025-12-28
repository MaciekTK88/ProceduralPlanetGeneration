#include "PlanetData.h"

FVector UPlanetData::PlanetTransformLocation(const FVector& TransformPos, const FIntVector& TransformRotDeg, const FVector& LocalLocation) const
{
	FIntVector Rotation = TransformRotDeg;

	if (Rotation.X == 0 && Rotation.Y == -1 && Rotation.Z == 0)
	{
		return FVector(LocalLocation.X, 0.f, LocalLocation.Y) + TransformPos;
	}

	if (Rotation.X == 0 && Rotation.Y == 1 && Rotation.Z == 0)
	{
		return FVector(LocalLocation.X, 0.f, -LocalLocation.Y) + TransformPos;
	}

	if (Rotation.X == -1 && Rotation.Y == 0 && Rotation.Z == 0)
	{
		return FVector(0.f, LocalLocation.Y, LocalLocation.X) + TransformPos;
	}

	if (Rotation.X == 0 && Rotation.Y == 0 && Rotation.Z == 1)
	{
		return FVector(LocalLocation.X, LocalLocation.Y, 0.f) + TransformPos;
	}

	if (Rotation.X == 0 && Rotation.Y == 0 && Rotation.Z == -1)
	{
		return FVector(-LocalLocation.X, LocalLocation.Y, 0.f) + TransformPos;
	}

	if (Rotation.X == 1 && Rotation.Y == 0 && Rotation.Z == 0)
	{
		return FVector(0.f, LocalLocation.Y, -LocalLocation.X) + TransformPos;
	}

	return LocalLocation + TransformPos;
}

FVector2f UPlanetData::InversePlanetTransformLocation(const FVector& TransformPos, const FIntVector& TransformRotDeg, const FVector& PlanetSpaceLocation) const
{
	FVector Pos = PlanetSpaceLocation - TransformPos;

	if (TransformRotDeg.X == 0 && TransformRotDeg.Y == -1 && TransformRotDeg.Z == 0)
	{
		return FVector2f(Pos.X, Pos.Z);
	}

	if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 1 && TransformRotDeg.Z == 0)
	{
		return FVector2f(Pos.X, -Pos.Z);
	}

	if (TransformRotDeg.X == -1 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 0)
	{
		return FVector2f(Pos.Z, Pos.Y);
	}

	if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 1)
	{
		return FVector2f(Pos.X, Pos.Y);
	}

	if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == -1)
	{
		return FVector2f(-Pos.X, Pos.Y);
	}

	if (TransformRotDeg.X == 1 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 0)
	{
		return FVector2f(-Pos.Z, Pos.Y);
	}

	return FVector2f(0.f, 0.f);
}
