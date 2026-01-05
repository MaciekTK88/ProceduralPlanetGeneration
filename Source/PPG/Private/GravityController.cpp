#include "GravityController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void AGravityController::UpdateRotation(float DeltaTime)
{
	FVector GravityDirection = FVector::DownVector;
	if (ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement())
		{
			GravityDirection = MoveComp->GetGravityDirection();
		}
	}

	// Get the current control rotation in world space
	FRotator ViewRotation = GetControlRotation();

	// Add any rotation from the gravity changes, if any happened.
	// Delete this code block if you don't want the camera to automatically compensate for gravity rotation.
	if (!LastFrameGravity.Equals(FVector::ZeroVector))
	{
		const FQuat DeltaGravityRotation = FQuat::FindBetweenNormals(LastFrameGravity, GravityDirection);
		const FQuat WarpedCameraRotation = DeltaGravityRotation * FQuat(ViewRotation);

		ViewRotation = WarpedCameraRotation.Rotator();	
	}
	LastFrameGravity = GravityDirection;

	// Convert the view rotation from world space to gravity relative space.
	// Now we can work with the rotation as if no custom gravity was affecting it.
	ViewRotation = GetGravityRelativeRotation(ViewRotation, GravityDirection);

	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);

	if (PlayerCameraManager)
	{
		ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn());

		PlayerCameraManager->ProcessViewRotation(DeltaTime, ViewRotation, DeltaRot);

		// Zero the roll of the camera as we always want it horizontal in relation to the gravity.
		ViewRotation.Roll = 0;

		// Convert the rotation back to world space, and set it as the current control rotation.
		SetControlRotation(GetGravityWorldRotation(ViewRotation, GravityDirection));
	}

	APawn* const P = GetPawnOrSpectator();
	if (P)
	{
		P->FaceRotation(ViewRotation, DeltaTime);
	}
}

FRotator AGravityController::GetGravityRelativeRotation(FRotator Rotation, FVector GravityDirection)
{
	if (!GravityDirection.Equals(FVector::DownVector))
	{
		FQuat GravityRotation = FQuat::FindBetweenNormals(GravityDirection, FVector::DownVector);
		return (GravityRotation * Rotation.Quaternion()).Rotator();
	}

	return Rotation;
}

FRotator AGravityController::GetGravityWorldRotation(FRotator Rotation, FVector GravityDirection)
{
	if (!GravityDirection.Equals(FVector::DownVector))
	{
		FQuat GravityRotation = FQuat::FindBetweenNormals(FVector::DownVector, GravityDirection);
		return (GravityRotation * Rotation.Quaternion()).Rotator();
	}

	return Rotation;
}