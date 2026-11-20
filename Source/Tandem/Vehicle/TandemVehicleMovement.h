// Tandem vehicle: arcade raycast movement.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TandemVehicleTypes.h"
#include "TandemVehicleMovement.generated.h"

class UCurveFloat;
class UPrimitiveComponent;
class UTandemVehicleTuning;
struct FHitResult;

/**
 * Arcade raycast vehicle movement.
 *
 * The chassis is a simulated physics body. Every step, each wheel looks for the ground under it and pushes on the
 * chassis with suspension, drive, brake and grip forces.
 *
 * Each step: steering, then suspension (which decides which wheels are grounded and how much load each carries),
 * then drive, brakes and grip on the grounded wheels, then anti-roll and downforce.
 */
UCLASS(ClassGroup = (Tandem), meta = (BlueprintSpawnableComponent))
class TANDEM_API UTandemVehicleMovement : public UActorComponent
{
	GENERATED_BODY()

public:
	UTandemVehicleMovement();

	/**
	 * Called by the owning pawn in BeginPlay, before anything moves the wheels.
	 * @param InBody              The simulated chassis.
	 * @param WheelRestLocations  World position of each wheel center at rest, in WheelSetups order (FL, FR, BL, BR).
	 * @param MeasuredRadii       Radius per wheel measured from the mesh (cm). Used when bMeasureWheelRadius is on; 0 = unknown.
	 */
	void InitializeVehicle(UPrimitiveComponent* InBody, const TArray<FVector>& WheelRestLocations, const TArray<float>& MeasuredRadii);

	/** Pushes mass, center of mass and damping from the tuning asset onto the chassis, and updates the suspension mounts. */
	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle")
	void ApplyTuning();

	// --- Input (called by the pawn) ---

	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle|Input")
	void SetThrottleInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle|Input")
	void SetBrakeInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle|Input")
	void SetSteeringInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Tandem|Vehicle|Input")
	void SetHandbrakeInput(bool bEngaged);

	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle|Input")
	float GetThrottleInput() const { return ThrottleInput; }

	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle|Input")
	float GetBrakeInput() const { return BrakeInput; }

	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle|Input")
	float GetSteeringInput() const { return SteeringInput; }

	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle|Input")
	bool GetHandbrakeInput() const { return bHandbrakeInput; }

	// --- Queries (HUD, tests, wheel visuals) ---

	/** Signed speed along the chassis' forward axis (km/h). */
	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle")
	float GetForwardSpeedKmh() const;

	/** True if any wheel touches the ground. */
	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle")
	bool IsGrounded() const;

	UFUNCTION(BlueprintPure, Category = "Tandem|Vehicle")
	int32 GetNumWheels() const { return WheelSetups.Num(); }

	int32 GetNumWheelStates() const { return WheelStates.Num(); }
	const FTandemWheelSetup& GetWheelSetup(int32 WheelIndex) const;
	const FTandemWheelState& GetWheelState(int32 WheelIndex) const;

	/** World position of a wheel's suspension mount. */
	FVector GetWheelMountWorld(int32 WheelIndex) const;

	UTandemVehicleTuning* GetTuning() const { return Tuning; }

	// --- Debug ---

	/** Flips the tandem.Vehicle.Debug console variable. */
	static void ToggleDebugDraw();
	static bool IsDebugDrawEnabled();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Springs, grip, curves and the rest. Create a TandemVehicleTuning data asset and assign it on the vehicle Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tandem|Vehicle")
	TObjectPtr<UTandemVehicleTuning> Tuning;

	/** One entry per wheel, in the order FL, FR, BL, BR. */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category = "Tandem|Vehicle")
	TArray<FTandemWheelSetup> WheelSetups;

	/** Use the wheel radii the pawn measures from the mesh at BeginPlay. Turn off to set each Radius by hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tandem|Vehicle")
	bool bMeasureWheelRadius = true;

	/** Live per-wheel state. Visible on the instance while playing. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tandem|Vehicle")
	TArray<FTandemWheelState> WheelStates;

	// =================================================================================
	//  The physics step, called in this order by StepVehicle.
	// =================================================================================

	void StepVehicle(float DeltaTime);
	void UpdateSteering(float DeltaTime);
	void UpdateSuspension(int32 WheelIndex, float DeltaTime);
	void ApplyDrive(int32 WheelIndex, float DeltaTime);
	void ApplyBraking(int32 WheelIndex, float DeltaTime);
	void ApplyLateralGrip(int32 WheelIndex, float DeltaTime);
	void ApplyAntiRoll(float DeltaTime);
	void ApplyDownforce(float DeltaTime);
	void UpdateWheelSpin(int32 WheelIndex, float DeltaTime);

	// =================================================================================
	//  Helpers for the step functions.
	// =================================================================================

	/** Chassis up axis (world). The suspension pushes along this. */
	FVector GetBodyUp() const;

	/** Wheel forward and right axes (world), including the wheel's current steering angle. */
	FVector GetWheelForward(int32 WheelIndex) const;
	FVector GetWheelRight(int32 WheelIndex) const;

	/** Velocity of the chassis at a world point (cm/s), including its spin. */
	FVector GetPointVelocity(const FVector& WorldPoint) const;

	/** Chassis mass (kg). */
	float GetBodyMassKg() const;

	/** Center of a wheel (world): SuspensionLength below its mount. */
	FVector GetWheelCenterWorld(int32 WheelIndex) const;

	/** Sweeps a sphere (or traces a line when Radius <= 0) for ground, ignoring this vehicle. */
	bool SweepGround(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const;

	/** Applies a force to the chassis at a world location and records it on the wheel for debug draw. */
	void AddForceAtWheel(int32 WheelIndex, const FVector& Force, const FVector& WorldLocation, ETandemWheelForce Kind);

	/** Evaluates a curve, or returns Fallback when no curve is assigned. */
	static float EvaluateCurve(const UCurveFloat* Curve, float X, float Fallback);

private:
	void RecomputeMounts();
	void DrawDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Body;

	/** Where each wheel sits at rest, in chassis space. */
	TArray<FVector> WheelRestOffsets;

	float ThrottleInput = 0.f;
	float BrakeInput = 0.f;
	float SteeringInput = 0.f;
	bool bHandbrakeInput = false;
	bool bWarnedMissingTuning = false;
};
