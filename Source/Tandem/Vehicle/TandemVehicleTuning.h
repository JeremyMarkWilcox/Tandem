// Tandem vehicle: tuning data.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TandemVehicleTuning.generated.h"

class UCurveFloat;

/**
 * Units: cm, kg, s. Forces are Unreal force units (kg*cm/s^2): a 1500 kg car weighs about 1,470,000.
 * The defaults are rough starting points for a ~1500 kg car, not tuned values.
 */
USTRUCT(BlueprintType)
struct TANDEM_API FTandemSuspensionTuning
{
	GENERATED_BODY()

	/** Suspension length with no load (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension", meta = (ClampMin = "1.0", ClampMax = "200.0"))
	float RestLength = 40.f;

	/** Force per cm of compression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension", meta = (ClampMin = "0.0"))
	float Stiffness = 40000.f;

	/** Force per cm/s of compression speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension", meta = (ClampMin = "0.0"))
	float Damping = 3000.f;

	/** How far past rest the wheel can travel before it bottoms out (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspension", meta = (ClampMin = "0.0"))
	float MaxTravel = 25.f;
};

USTRUCT(BlueprintType)
struct TANDEM_API FTandemDriveTuning
{
	GENERATED_BODY()

	/** Drive force available at a given speed. X = forward speed (km/h), Y = force. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	TObjectPtr<UCurveFloat> DriveForceBySpeed = nullptr;

	/** Used when no curve is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "0.0"))
	float FallbackDriveForce = 600000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "0.0"))
	float MaxForwardSpeedKmh = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "0.0"))
	float MaxReverseSpeedKmh = 30.f;
};

USTRUCT(BlueprintType)
struct TANDEM_API FTandemTireTuning
{
	GENERATED_BODY()

	/** Friction coefficient. The most sideways force a wheel can make is Load x Grip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tires", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float Grip = 1.2f;

	/** Grip multiplier by slip. X = slip (0 = rolling straight, 1 = fully sideways), Y = multiplier. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tires")
	TObjectPtr<UCurveFloat> GripBySlip = nullptr;

	/** Grip multiplier on handbrake wheels while the handbrake is held. Lower = more drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tires", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandbrakeGripMultiplier = 0.35f;

	/** Slows the car while coasting. Force per cm/s of forward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tires", meta = (ClampMin = "0.0"))
	float RollingResistance = 30.f;
};

USTRUCT(BlueprintType)
struct TANDEM_API FTandemSteeringTuning
{
	GENERATED_BODY()

	/** Max steering angle by speed. X = speed (km/h), Y = degrees. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	TObjectPtr<UCurveFloat> MaxAngleBySpeed = nullptr;

	/** Used when no curve is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float FallbackMaxAngleDeg = 35.f;

	/** How fast the wheels turn toward the target angle (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering", meta = (ClampMin = "0.0"))
	float SteerSpeedDegPerSec = 180.f;
};

USTRUCT(BlueprintType)
struct TANDEM_API FTandemBrakeTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brakes", meta = (ClampMin = "0.0"))
	float BrakeForce = 800000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brakes", meta = (ClampMin = "0.0"))
	float HandbrakeForce = 400000.f;
};

USTRUCT(BlueprintType)
struct TANDEM_API FTandemChassisTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis", meta = (ClampMin = "1.0"))
	float MassKg = 1500.f;

	/** Offset from the mesh's own center of mass (cm). Lower Z makes it harder to flip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis")
	FVector CenterOfMassOffset = FVector(0.f, 0.f, -30.f);

	/** Resists body roll by evening out left/right compression on each axle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis", meta = (ClampMin = "0.0"))
	float AntiRollStiffness = 20000.f;

	/** Extra downward push per km/h while grounded. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis", meta = (ClampMin = "0.0"))
	float DownforcePerKmh = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis", meta = (ClampMin = "0.0"))
	float LinearDamping = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chassis", meta = (ClampMin = "0.0"))
	float AngularDamping = 0.5f;
};

/** Everything that changes how the vehicle feels. Create one per car or preset (right-click > Miscellaneous > Data Asset). */
UCLASS(BlueprintType)
class TANDEM_API UTandemVehicleTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemSuspensionTuning Suspension;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemDriveTuning Drive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemTireTuning Tires;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemSteeringTuning Steering;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemBrakeTuning Brakes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FTandemChassisTuning Chassis;
};
