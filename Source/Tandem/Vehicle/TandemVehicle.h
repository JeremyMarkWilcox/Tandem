// Tandem vehicle: the drivable pawn (components, input, wheel bones, camera).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TandemVehicle.generated.h"

class UBoxComponent;
class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UPoseableMeshComponent;
class USpringArmComponent;
class UTandemVehicleMovement;
struct FInputActionValue;

/**
 * Drivable arcade vehicle.
 *
 * Chassis is an invisible box: the simulated physics body. VehicleMesh is the rigged, weight-painted skeletal mesh,
 * visual only. UTandemVehicleMovement does the physics, and this pawn moves the mesh's wheel bones to match the
 * wheel states every frame.
 *
 * Make a Blueprint child, assign the skeletal mesh, size the Chassis box to the body, then assign a
 * TandemVehicleTuning asset (on VehicleMovement) and the input assets.
 */
UCLASS(Abstract)
class TANDEM_API ATandemVehicle : public APawn
{
	GENERATED_BODY()

public:
	ATandemVehicle();

	virtual void Tick(float DeltaTime) override;
	virtual void UnPossessed() override;

	/** Puts the vehicle back on its wheels a little above where it is, keeping its heading. */
	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle")
	void ResetVehicle();

	UBoxComponent* GetChassis() const { return Chassis; }
	UPoseableMeshComponent* GetVehicleMesh() const { return VehicleMesh; }
	UTandemVehicleMovement* GetVehicleMovement() const { return VehicleMovement; }

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
	 * Simulated physics body (hidden in game). Size it to the truck's body, with its bottom above the bottoms of the
	 * tires, so the wheels hold it off the ground. Keep its scale at 1 and change Box Extent instead.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Chassis;

	/** The rigged vehicle mesh. Visual only, no collision. Rotate it so the truck faces along the Chassis' X axis. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPoseableMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTandemVehicleMovement> VehicleMovement;

	/** Wheel bones on VehicleMesh, in the same order as the movement's wheel setups (FL, FR, BL, BR). */
	UPROPERTY(EditAnywhere, Category = "Tandem|Vehicle")
	TArray<FName> WheelBoneNames;

	// --- Input ---

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	/** Above the template's contexts (priority 0), so the vehicle's keys win while you drive. */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 MappingContextPriority = 10;

	/** Axis1D, 0..1. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ThrottleAction;

	/** Axis1D, 0..1. Brakes, then reverses once stopped. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BrakeAction;

	/** Axis1D, -1..1. Positive steers right. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SteerAction;

	/** Digital (bool). */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> HandbrakeAction;

	/** Axis2D. Orbits the camera. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** Digital (bool). Calls ResetVehicle. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ResetAction;

	/** Digital (bool). Flips the tandem.Vehicle.Debug console variable. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleDebugAction;

	// --- Camera ---

	/** Degrees of orbit per unit of look input. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
	float LookSpeed = 1.5f;

	/** Camera pitch limits (degrees). Negative looks down from above. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinCameraPitch = -60.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxCameraPitch = 10.f;

	/** Seconds without look input before the camera swings back behind the vehicle. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
	float CameraRecenterDelay = 1.5f;

	/** How fast it swings back. 0 turns recentering off. */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0.0"))
	float CameraRecenterSpeed = 3.f;

	/** How far above its current spot ResetVehicle puts the vehicle (cm). */
	UPROPERTY(EditAnywhere, Category = "Tandem|Vehicle", meta = (ClampMin = "0.0"))
	float ResetLiftHeight = 100.f;

private:
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnHandbrakePressed();
	void OnHandbrakeReleased();
	void OnLook(const FInputActionValue& Value);
	void OnResetPressed();
	void OnToggleDebugPressed();

	void UpdateWheelVisuals();
	void UpdateCamera(float DeltaTime);
	void ClearDriveInputs();
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	/** Bone index per wheel (INDEX_NONE if the mesh doesn't have that bone). */
	TArray<int32> WheelBoneIndices;

	/** Wheel bone rotations in the mesh's rest pose (component space). Steer and spin are applied on top. */
	TArray<FQuat> WheelRestRotations;

	/** VehicleMesh's rotation relative to Chassis, for turning chassis-space steer and spin into mesh space. */
	FQuat MeshRotationInChassis = FQuat::Identity;

	/** Spring arm rotation from the Blueprint. The camera recenters to this. */
	FRotator BaseArmRotation = FRotator::ZeroRotator;

	float TimeSinceLookInput = 0.f;
};
