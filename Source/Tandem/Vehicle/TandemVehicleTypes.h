// Tandem vehicle: shared wheel types.

#pragma once

#include "CoreMinimal.h"
#include "TandemVehicleTypes.generated.h"

/**
 * Static setup for one wheel.
 * Defaults come from UTandemVehicleMovement's constructor (FL, FR, BL, BR). Edit Radius and the flags on the vehicle Blueprint.
 */
USTRUCT(BlueprintType)
struct TANDEM_API FTandemWheelSetup
{
	GENERATED_BODY()

	/** Display name used by debug draw, e.g. "FL". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	FName Name = NAME_None;

	/** Wheel radius (cm). Measured from the vehicle mesh at BeginPlay when bMeasureWheelRadius is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "1.0"))
	float Radius = 35.f;

	/** Front wheels steer by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bSteered = false;

	/** All four wheels drive by default; turn the front ones off for rear-wheel drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bDriven = true;

	/** Rear wheels respond to the handbrake by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bAffectedByHandbrake = false;

	/**
	 * Top of the suspension, in the chassis' local space (cm).
	 * Computed at BeginPlay as Suspension.RestLength above the wheel bone's rest position,
	 * so a wheel at rest length lines up with the mesh as it was modeled. Keep the chassis scale at 1.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	FVector MountOffset = FVector::ZeroVector;
};

/**
 * Live state for one wheel. Your step functions write it; debug draw and the wheel meshes read it.
 * Units: cm, s. Forces are Unreal force units (kg*cm/s^2).
 */
USTRUCT(BlueprintType)
struct TANDEM_API FTandemWheelState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	bool bGrounded = false;

	/** Distance from the mount down to the wheel center (cm). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float SuspensionLength = 0.f;

	/** Last step's SuspensionLength, for working out how fast the spring is moving. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float PreviousSuspensionLength = 0.f;

	/** Ground contact (world space). Valid while bGrounded. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	FVector ContactPoint = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	FVector ContactNormal = FVector::UpVector;

	/** How hard this wheel presses on the ground this step. Grip scales with it. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float Load = 0.f;

	/** Current steering angle (degrees). Positive turns right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float SteerAngleDeg = 0.f;

	/** Wheel spin for the visuals. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float SpinSpeedDegPerSec = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float SpinAngleDeg = 0.f;

	/** Sideways speed at the contact (cm/s). Positive = sliding right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel")
	float LateralSpeed = 0.f;

	/** Forces applied this step (world space). Recorded by AddForceAtWheel for debug draw. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel|Debug")
	FVector SuspensionForce = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel|Debug")
	FVector DriveForce = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wheel|Debug")
	FVector GripForce = FVector::ZeroVector;
};

/** Which bucket a force belongs to, so debug draw can color it. */
UENUM(BlueprintType)
enum class ETandemWheelForce : uint8
{
	Suspension,
	Drive,
	Grip
};
