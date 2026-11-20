// Tandem vehicle: arcade raycast movement.

#include "TandemVehicleMovement.h"

#include "Tandem.h"
#include "TandemVehicleMath.h"
#include "TandemVehicleTuning.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarTandemVehicleDebug(
	TEXT("tandem.Vehicle.Debug"),
	0,
	TEXT("Draw Tandem vehicle debug info (suspension, contacts, forces). 0 = off, 1 = on."),
	ECVF_Cheat);

UTandemVehicleMovement::UTandemVehicleMovement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics; // forces have to be added before the physics step

	constexpr int32 NumDefaultWheels = 4;
	static const TCHAR* DefaultWheelNames[NumDefaultWheels] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };

	WheelSetups.SetNum(NumDefaultWheels);
	for (int32 WheelIndex = 0; WheelIndex < NumDefaultWheels; ++WheelIndex)
	{
		FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
		Setup.Name = FName(DefaultWheelNames[WheelIndex]);
		Setup.bSteered = WheelIndex < 2;
		Setup.bDriven = true;
		Setup.bAffectedByHandbrake = WheelIndex >= 2;
	}
}

void UTandemVehicleMovement::InitializeVehicle(UPrimitiveComponent* InBody, const TArray<FVector>& WheelRestLocations, const TArray<float>& MeasuredRadii)
{
	Body = InBody;
	if (!Body)
	{
		UE_LOG(LogTandem, Error, TEXT("%s: InitializeVehicle was called without a chassis."), *GetNameSafe(GetOwner()));
		return;
	}

	if (WheelRestLocations.Num() != WheelSetups.Num())
	{
		UE_LOG(LogTandem, Warning, TEXT("%s: %d wheels but %d wheel setups. Resizing the setups to match."),
			*GetNameSafe(GetOwner()), WheelRestLocations.Num(), WheelSetups.Num());
		WheelSetups.SetNum(WheelRestLocations.Num());
	}

	const FTransform& BodyTransform = Body->GetComponentTransform();
	WheelRestOffsets.SetNum(WheelRestLocations.Num());

	for (int32 WheelIndex = 0; WheelIndex < WheelRestLocations.Num(); ++WheelIndex)
	{
		WheelRestOffsets[WheelIndex] = BodyTransform.InverseTransformPosition(WheelRestLocations[WheelIndex]);

		if (bMeasureWheelRadius && MeasuredRadii.IsValidIndex(WheelIndex) && MeasuredRadii[WheelIndex] > 1.f)
		{
			WheelSetups[WheelIndex].Radius = MeasuredRadii[WheelIndex];
		}
	}

	RecomputeMounts();

	const float RestLength = Tuning ? Tuning->Suspension.RestLength : 0.f;
	WheelStates.SetNum(WheelSetups.Num());
	for (FTandemWheelState& State : WheelStates)
	{
		State = FTandemWheelState();
		State.SuspensionLength = RestLength;
		State.PreviousSuspensionLength = RestLength;
	}
}

void UTandemVehicleMovement::ApplyTuning()
{
	if (!Body || !Tuning)
	{
		return;
	}

	const FTandemChassisTuning& ChassisTuning = Tuning->Chassis;
	Body->SetMassOverrideInKg(NAME_None, ChassisTuning.MassKg, true);
	Body->SetCenterOfMass(ChassisTuning.CenterOfMassOffset);
	Body->SetLinearDamping(ChassisTuning.LinearDamping);
	Body->SetAngularDamping(ChassisTuning.AngularDamping);

	RecomputeMounts();
}

void UTandemVehicleMovement::RecomputeMounts()
{
	const float RestLength = Tuning ? Tuning->Suspension.RestLength : 0.f;
	const int32 Count = FMath::Min(WheelSetups.Num(), WheelRestOffsets.Num());
	for (int32 WheelIndex = 0; WheelIndex < Count; ++WheelIndex)
	{
		// Each wheel starts where it sits at rest; the mount is RestLength above that.
		WheelSetups[WheelIndex].MountOffset = WheelRestOffsets[WheelIndex] + FVector::UpVector * RestLength;
	}
}

// --- Input ---------------------------------------------------------------------------------

void UTandemVehicleMovement::SetThrottleInput(float Value)
{
	ThrottleInput = FMath::Clamp(Value, 0.f, 1.f);
}

void UTandemVehicleMovement::SetBrakeInput(float Value)
{
	BrakeInput = FMath::Clamp(Value, 0.f, 1.f);
}

void UTandemVehicleMovement::SetSteeringInput(float Value)
{
	SteeringInput = FMath::Clamp(Value, -1.f, 1.f);
}

void UTandemVehicleMovement::SetHandbrakeInput(bool bEngaged)
{
	bHandbrakeInput = bEngaged;
}

// --- Queries -------------------------------------------------------------------------------

float UTandemVehicleMovement::GetForwardSpeedKmh() const
{
	if (!Body)
	{
		return 0.f;
	}
	const double ForwardSpeedCmPerSec = FVector::DotProduct(Body->GetPhysicsLinearVelocity(), Body->GetForwardVector());
	return TandemVehicleMath::CmPerSecToKmh(static_cast<float>(ForwardSpeedCmPerSec));
}

bool UTandemVehicleMovement::IsGrounded() const
{
	for (const FTandemWheelState& State : WheelStates)
	{
		if (State.bGrounded)
		{
			return true;
		}
	}
	return false;
}

const FTandemWheelSetup& UTandemVehicleMovement::GetWheelSetup(int32 WheelIndex) const
{
	check(WheelSetups.IsValidIndex(WheelIndex));
	return WheelSetups[WheelIndex];
}

const FTandemWheelState& UTandemVehicleMovement::GetWheelState(int32 WheelIndex) const
{
	check(WheelStates.IsValidIndex(WheelIndex));
	return WheelStates[WheelIndex];
}

FVector UTandemVehicleMovement::GetWheelMountWorld(int32 WheelIndex) const
{
	return Body ? Body->GetComponentTransform().TransformPosition(GetWheelSetup(WheelIndex).MountOffset) : FVector::ZeroVector;
}

// --- Debug toggle ---------------------------------------------------------------------------

void UTandemVehicleMovement::ToggleDebugDraw()
{
	CVarTandemVehicleDebug->Set(IsDebugDrawEnabled() ? 0 : 1, ECVF_SetByConsole);
}

bool UTandemVehicleMovement::IsDebugDrawEnabled()
{
	return CVarTandemVehicleDebug.GetValueOnGameThread() > 0;
}

// --- Tick and step order ---------------------------------------------------------------------

void UTandemVehicleMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Body || !Body->IsSimulatingPhysics() || DeltaTime <= 0.f || WheelStates.Num() != WheelSetups.Num())
	{
		return;
	}

	if (!Tuning)
	{
		if (!bWarnedMissingTuning)
		{
			UE_LOG(LogTandem, Warning, TEXT("%s: no TandemVehicleTuning asset assigned, so the vehicle won't drive."), *GetNameSafe(GetOwner()));
			bWarnedMissingTuning = true;
		}
		return;
	}

	StepVehicle(DeltaTime);

	if (IsDebugDrawEnabled())
	{
		DrawDebug();
	}
}

void UTandemVehicleMovement::StepVehicle(float DeltaTime)
{
	// Clear last step's recorded forces so debug draw only shows this step.
	for (FTandemWheelState& State : WheelStates)
	{
		State.SuspensionForce = FVector::ZeroVector;
		State.DriveForce = FVector::ZeroVector;
		State.GripForce = FVector::ZeroVector;
	}

	UpdateSteering(DeltaTime);

	// Suspension first: it decides which wheels touch the ground and how much load each one carries.
	for (int32 WheelIndex = 0; WheelIndex < WheelStates.Num(); ++WheelIndex)
	{
		UpdateSuspension(WheelIndex, DeltaTime);
	}

	for (int32 WheelIndex = 0; WheelIndex < WheelStates.Num(); ++WheelIndex)
	{
		if (WheelStates[WheelIndex].bGrounded)
		{
			ApplyDrive(WheelIndex, DeltaTime);
			ApplyBraking(WheelIndex, DeltaTime);
			ApplyLateralGrip(WheelIndex, DeltaTime);
		}
		UpdateWheelSpin(WheelIndex, DeltaTime);
	}

	ApplyAntiRoll(DeltaTime);
	ApplyDownforce(DeltaTime);
}

// --- Helpers ----------------------------------------------------------------------------------

FVector UTandemVehicleMovement::GetBodyUp() const
{
	return Body ? Body->GetUpVector() : FVector::UpVector;
}

FVector UTandemVehicleMovement::GetWheelForward(int32 WheelIndex) const
{
	if (!Body)
	{
		return FVector::ForwardVector;
	}
	const float SteerDeg = WheelStates.IsValidIndex(WheelIndex) ? WheelStates[WheelIndex].SteerAngleDeg : 0.f;
	return Body->GetForwardVector().RotateAngleAxis(SteerDeg, Body->GetUpVector());
}

FVector UTandemVehicleMovement::GetWheelRight(int32 WheelIndex) const
{
	if (!Body)
	{
		return FVector::RightVector;
	}
	const float SteerDeg = WheelStates.IsValidIndex(WheelIndex) ? WheelStates[WheelIndex].SteerAngleDeg : 0.f;
	return Body->GetRightVector().RotateAngleAxis(SteerDeg, Body->GetUpVector());
}

FVector UTandemVehicleMovement::GetPointVelocity(const FVector& WorldPoint) const
{
	return Body ? Body->GetPhysicsLinearVelocityAtPoint(WorldPoint) : FVector::ZeroVector;
}

float UTandemVehicleMovement::GetBodyMassKg() const
{
	return Body ? Body->GetMass() : 0.f;
}

bool UTandemVehicleMovement::SweepGround(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TandemWheelSweep), false, GetOwner());
	if (Radius <= 0.f)
	{
		return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	}
	return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params);
}

void UTandemVehicleMovement::AddForceAtWheel(int32 WheelIndex, const FVector& Force, const FVector& WorldLocation, ETandemWheelForce Kind)
{
	if (!Body || !WheelStates.IsValidIndex(WheelIndex))
	{
		return;
	}

	Body->AddForceAtLocation(Force, WorldLocation);

	FTandemWheelState& State = WheelStates[WheelIndex];
	switch (Kind)
	{
	case ETandemWheelForce::Suspension:
		State.SuspensionForce += Force;
		break;
	case ETandemWheelForce::Drive:
		State.DriveForce += Force;
		break;
	case ETandemWheelForce::Grip:
		State.GripForce += Force;
		break;
	}
}

float UTandemVehicleMovement::EvaluateCurve(const UCurveFloat* Curve, float X, float Fallback)
{
	return Curve ? Curve->GetFloatValue(X) : Fallback;
}

// --- Debug draw -----------------------------------------------------------------------------
// Green/red line: suspension (grounded/airborne). White circle: wheel. Yellow dot: contact.
// Arrows: cyan = suspension, orange = drive/brake, magenta = grip. Yellow sphere: center of mass.

void UTandemVehicleMovement::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World || !Body)
	{
		return;
	}

	// Unreal forces are big numbers (kg*cm/s^2), so scale them down to fit on screen.
	constexpr float ForceToCm = 0.0005f;
	const FVector Up = GetBodyUp();

	auto DrawForceArrow = [&](const FVector& Origin, const FVector& Force, const FColor& ArrowColor)
	{
		if (!Force.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, Origin, Origin + Force * ForceToCm, 15.f, ArrowColor, false, -1.f, 0, 2.f);
		}
	};

	for (int32 WheelIndex = 0; WheelIndex < WheelStates.Num(); ++WheelIndex)
	{
		const FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
		const FTandemWheelState& State = WheelStates[WheelIndex];
		const FVector Mount = GetWheelMountWorld(WheelIndex);
		const FVector WheelCenter = Mount - Up * State.SuspensionLength;

		DrawDebugLine(World, Mount, WheelCenter, State.bGrounded ? FColor::Green : FColor::Red, false, -1.f, 0, 1.5f);
		DrawDebugCircle(World, WheelCenter, Setup.Radius, 24, FColor::White, false, -1.f, 0, 1.f, GetWheelForward(WheelIndex), Up, false);

		if (State.bGrounded)
		{
			DrawDebugPoint(World, State.ContactPoint, 8.f, FColor::Yellow);
		}

		DrawForceArrow(WheelCenter, State.SuspensionForce, FColor::Cyan);
		DrawForceArrow(WheelCenter, State.DriveForce, FColor::Orange);
		DrawForceArrow(WheelCenter, State.GripForce, FColor::Magenta);

		DrawDebugString(World, WheelCenter + Up * (Setup.Radius + 15.f),
			FString::Printf(TEXT("%s  len %.1f  load %.0f  slip %.0f"), *Setup.Name.ToString(), State.SuspensionLength, State.Load, State.LateralSpeed),
			nullptr, FColor::White, 0.f, true);
	}

	DrawDebugSphere(World, Body->GetCenterOfMass(), 10.f, 8, FColor::Yellow);
	DrawDebugString(World, Body->GetComponentLocation() + Up * 200.f,
		FString::Printf(TEXT("%.0f km/h  throttle %.2f  brake %.2f  steer %.2f%s"),
			GetForwardSpeedKmh(), ThrottleInput, BrakeInput, SteeringInput, bHandbrakeInput ? TEXT("  HANDBRAKE") : TEXT("")),
		nullptr, FColor::White, 0.f, true);
#endif
}

// =============================================================================================
//  The physics step. Units: cm, kg, s. Forces are kg*cm/s^2, so a 1500 kg car weighs about 1,470,000.
//  The math lives in TandemVehicleMath so the unit tests can cover it.
//  TickComponent guarantees Body and Tuning are valid before StepVehicle runs.
// =============================================================================================

namespace TandemVehicleMovementPrivate
{
	static int32 CountWheelsWith(const TArray<FTandemWheelSetup>& Setups, bool FTandemWheelSetup::*Flag)
	{
		int32 Count = 0;
		for (const FTandemWheelSetup& Setup : Setups)
		{
			Count += (Setup.*Flag) ? 1 : 0;
		}
		return Count;
	}

	/** A direction flattened onto the ground, so wheel forces push along the road rather than into it. */
	static FVector AlongGround(const FVector& Direction, const FVector& GroundNormal)
	{
		return FVector::VectorPlaneProject(Direction, GroundNormal).GetSafeNormal();
	}
}

FVector UTandemVehicleMovement::GetWheelCenterWorld(int32 WheelIndex) const
{
	const float Length = WheelStates.IsValidIndex(WheelIndex) ? WheelStates[WheelIndex].SuspensionLength : 0.f;
	return GetWheelMountWorld(WheelIndex) - GetBodyUp() * Length;
}

void UTandemVehicleMovement::UpdateSteering(float DeltaTime)
{
	const FTandemSteeringTuning& SteeringTuning = Tuning->Steering;
	const float SpeedKmh = FMath::Abs(GetForwardSpeedKmh());

	// Without a curve, steer less as speed builds (down to 40% at top speed) so the car isn't twitchy when it's fast.
	const float SpeedAlpha = FMath::Clamp(SpeedKmh / FMath::Max(Tuning->Drive.MaxForwardSpeedKmh, 1.f), 0.f, 1.f);
	const float FallbackMaxAngle = SteeringTuning.FallbackMaxAngleDeg * FMath::Lerp(1.f, 0.4f, SpeedAlpha);
	const float MaxAngle = EvaluateCurve(SteeringTuning.MaxAngleBySpeed, SpeedKmh, FallbackMaxAngle);
	const float TargetAngle = TandemVehicleMath::SteerAngleDeg(SteeringInput, MaxAngle);

	// Turn the wheels toward the target at a limited rate, like hands on a steering wheel.
	const float MaxStep = SteeringTuning.SteerSpeedDegPerSec * DeltaTime;
	for (int32 WheelIndex = 0; WheelIndex < WheelStates.Num(); ++WheelIndex)
	{
		if (WheelSetups[WheelIndex].bSteered)
		{
			float& Angle = WheelStates[WheelIndex].SteerAngleDeg;
			Angle += FMath::Clamp(TargetAngle - Angle, -MaxStep, MaxStep);
		}
	}
}

void UTandemVehicleMovement::UpdateSuspension(int32 WheelIndex, float DeltaTime)
{
	const FTandemSuspensionTuning& SuspensionTuning = Tuning->Suspension;
	const FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
	FTandemWheelState& State = WheelStates[WheelIndex];
	State.PreviousSuspensionLength = State.SuspensionLength;

	// Sweep the wheel (a sphere of its radius) from the top of the suspension down to its rest position.
	const FVector Up = GetBodyUp();
	const FVector Mount = GetWheelMountWorld(WheelIndex);
	FHitResult Hit;
	if (!SweepGround(Mount, Mount - Up * SuspensionTuning.RestLength, Setup.Radius, Hit))
	{
		// Airborne: no push, and the wheel eases back down to rest length.
		State.bGrounded = false;
		State.Load = 0.f;
		State.SuspensionLength += (SuspensionTuning.RestLength - State.SuspensionLength) * FMath::Clamp(DeltaTime * 15.f, 0.f, 1.f);
		return;
	}

	// How far the sphere got before touching is where the wheel center sits. Past MaxTravel it has bottomed out.
	const float MinLength = FMath::Max(SuspensionTuning.RestLength - SuspensionTuning.MaxTravel, 0.f);
	State.bGrounded = true;
	State.ContactPoint = Hit.ImpactPoint;
	State.ContactNormal = Hit.ImpactNormal;
	State.SuspensionLength = FMath::Clamp(static_cast<float>(Hit.Distance), MinLength, SuspensionTuning.RestLength);

	const float Compression = SuspensionTuning.RestLength - State.SuspensionLength;
	const float CompressionSpeed = (State.PreviousSuspensionLength - State.SuspensionLength) / DeltaTime;
	State.Load = TandemVehicleMath::SpringDamperForce(Compression, CompressionSpeed, SuspensionTuning.Stiffness, SuspensionTuning.Damping);

	AddForceAtWheel(WheelIndex, Up * State.Load, GetWheelCenterWorld(WheelIndex), ETandemWheelForce::Suspension);
}

void UTandemVehicleMovement::ApplyDrive(int32 WheelIndex, float DeltaTime)
{
	const int32 NumDriven = TandemVehicleMovementPrivate::CountWheelsWith(WheelSetups, &FTandemWheelSetup::bDriven);
	if (!WheelSetups[WheelIndex].bDriven || ThrottleInput <= 0.f || NumDriven == 0)
	{
		return;
	}

	const FTandemDriveTuning& DriveTuning = Tuning->Drive;
	const float SpeedKmh = GetForwardSpeedKmh();
	const float AvailableForce = EvaluateCurve(DriveTuning.DriveForceBySpeed, FMath::Abs(SpeedKmh), DriveTuning.FallbackDriveForce);
	const float WheelForce = TandemVehicleMath::DriveForce(ThrottleInput, AvailableForce, SpeedKmh, DriveTuning.MaxForwardSpeedKmh)
		/ static_cast<float>(NumDriven);

	const FVector Forward = TandemVehicleMovementPrivate::AlongGround(GetWheelForward(WheelIndex), WheelStates[WheelIndex].ContactNormal);
	AddForceAtWheel(WheelIndex, Forward * WheelForce, GetWheelCenterWorld(WheelIndex), ETandemWheelForce::Drive);
}

void UTandemVehicleMovement::ApplyBraking(int32 WheelIndex, float DeltaTime)
{
	using namespace TandemVehicleMovementPrivate;

	const FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
	const FTandemWheelState& State = WheelStates[WheelIndex];
	const FVector Forward = AlongGround(GetWheelForward(WheelIndex), State.ContactNormal);
	const FVector WheelCenter = GetWheelCenterWorld(WheelIndex);
	const float WheelSpeed = static_cast<float>(FVector::DotProduct(GetPointVelocity(State.ContactPoint), Forward)); // cm/s
	const float NumWheels = static_cast<float>(FMath::Max(WheelStates.Num(), 1));
	const float SpeedKmh = GetForwardSpeedKmh();

	// Everything that slows the wheel shares one budget: the force that would stop it this step, and no more.
	// Going past that would push the car backwards and make it shudder at a standstill.
	float StopBudget = FMath::Abs(WheelSpeed) * (GetBodyMassKg() / NumWheels) / DeltaTime;
	auto Resist = [&](float Force)
	{
		const float Applied = FMath::Min(Force, StopBudget);
		if (Applied > 0.f)
		{
			StopBudget -= Applied;
			AddForceAtWheel(WheelIndex, Forward * (-FMath::Sign(WheelSpeed) * Applied), WheelCenter, ETandemWheelForce::Drive);
		}
	};

	if (BrakeInput > 0.f)
	{
		if (SpeedKmh > 2.f)
		{
			Resist(Tuning->Brakes.BrakeForce * BrakeInput / NumWheels);
		}
		else if (Setup.bDriven)
		{
			// Nearly stopped or already rolling back: the brake becomes reverse.
			const FTandemDriveTuning& DriveTuning = Tuning->Drive;
			const float AvailableForce = EvaluateCurve(DriveTuning.DriveForceBySpeed, FMath::Abs(SpeedKmh), DriveTuning.FallbackDriveForce);
			const float NumDriven = static_cast<float>(FMath::Max(CountWheelsWith(WheelSetups, &FTandemWheelSetup::bDriven), 1));
			const float ReverseForce = TandemVehicleMath::DriveForce(BrakeInput, AvailableForce, -SpeedKmh, DriveTuning.MaxReverseSpeedKmh) / NumDriven;
			AddForceAtWheel(WheelIndex, -Forward * ReverseForce, WheelCenter, ETandemWheelForce::Drive);
		}
	}

	if (bHandbrakeInput && Setup.bAffectedByHandbrake)
	{
		const float NumHandbrake = static_cast<float>(FMath::Max(CountWheelsWith(WheelSetups, &FTandemWheelSetup::bAffectedByHandbrake), 1));
		Resist(Tuning->Brakes.HandbrakeForce / NumHandbrake);
	}

	if (ThrottleInput <= 0.f && BrakeInput <= 0.f)
	{
		// Coasting: rolling resistance, plus a gentle hold at walking pace so the car doesn't creep down slopes.
		const float Rolling = FMath::Abs(WheelSpeed) * Tuning->Tires.RollingResistance;
		const float Hold = FMath::Abs(WheelSpeed) < 50.f ? State.Load * Tuning->Tires.Grip * 0.5f : 0.f;
		Resist(Rolling + Hold);
	}
}

void UTandemVehicleMovement::ApplyLateralGrip(int32 WheelIndex, float DeltaTime)
{
	const FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
	FTandemWheelState& State = WheelStates[WheelIndex];
	const FTandemTireTuning& TireTuning = Tuning->Tires;

	const FVector Right = TandemVehicleMovementPrivate::AlongGround(GetWheelRight(WheelIndex), State.ContactNormal);
	const FVector ContactVelocity = GetPointVelocity(State.ContactPoint);
	State.LateralSpeed = static_cast<float>(FVector::DotProduct(ContactVelocity, Right));

	// Slip: 0 = rolling straight, 1 = fully sideways. A GripBySlip curve can make the tire let go as it slides.
	const float ContactSpeed = FMath::Max(static_cast<float>(ContactVelocity.Size()), 1.f);
	const float Slip = FMath::Clamp(FMath::Abs(State.LateralSpeed) / ContactSpeed, 0.f, 1.f);
	float Grip = TireTuning.Grip * EvaluateCurve(TireTuning.GripBySlip, Slip, 1.f);
	if (bHandbrakeInput && Setup.bAffectedByHandbrake)
	{
		Grip *= TireTuning.HandbrakeGripMultiplier;
	}

	const float CornerMass = GetBodyMassKg() / static_cast<float>(FMath::Max(WheelStates.Num(), 1));
	const float GripForce = TandemVehicleMath::LateralGripForce(State.LateralSpeed, State.Load, Grip, DeltaTime, CornerMass);

	// Applied at the wheel center rather than the contact patch, so cornering leans the body less.
	AddForceAtWheel(WheelIndex, Right * GripForce, GetWheelCenterWorld(WheelIndex), ETandemWheelForce::Grip);
}

void UTandemVehicleMovement::ApplyAntiRoll(float DeltaTime)
{
	const float AntiRollStiffness = Tuning->Chassis.AntiRollStiffness;
	if (AntiRollStiffness <= 0.f)
	{
		return;
	}

	const float RestLength = Tuning->Suspension.RestLength;
	const FVector Up = GetBodyUp();

	// Wheels 0/1 are the front axle and 2/3 the rear. Lift the more compressed side and press down the other,
	// which evens out how far the body leans.
	for (int32 LeftIndex = 0; LeftIndex + 1 < WheelStates.Num(); LeftIndex += 2)
	{
		const int32 RightIndex = LeftIndex + 1;
		const FTandemWheelState& LeftState = WheelStates[LeftIndex];
		const FTandemWheelState& RightState = WheelStates[RightIndex];

		const float LeftCompression = RestLength - LeftState.SuspensionLength;
		const float RightCompression = RestLength - RightState.SuspensionLength;
		const float BarForce = (LeftCompression - RightCompression) * AntiRollStiffness;

		if (LeftState.bGrounded)
		{
			AddForceAtWheel(LeftIndex, Up * BarForce, GetWheelCenterWorld(LeftIndex), ETandemWheelForce::Suspension);
		}
		if (RightState.bGrounded)
		{
			AddForceAtWheel(RightIndex, Up * -BarForce, GetWheelCenterWorld(RightIndex), ETandemWheelForce::Suspension);
		}
	}
}

void UTandemVehicleMovement::ApplyDownforce(float DeltaTime)
{
	const float DownforcePerKmh = Tuning->Chassis.DownforcePerKmh;
	if (DownforcePerKmh <= 0.f || !IsGrounded())
	{
		return;
	}

	const float Downforce = FMath::Abs(GetForwardSpeedKmh()) * DownforcePerKmh;
	Body->AddForceAtLocation(-GetBodyUp() * Downforce, Body->GetCenterOfMass());
}

void UTandemVehicleMovement::UpdateWheelSpin(int32 WheelIndex, float DeltaTime)
{
	const FTandemWheelSetup& Setup = WheelSetups[WheelIndex];
	FTandemWheelState& State = WheelStates[WheelIndex];

	if (State.bGrounded)
	{
		// Rolling without slipping: spin rate = ground speed / radius. Positive = rolling forward.
		const FVector Forward = TandemVehicleMovementPrivate::AlongGround(GetWheelForward(WheelIndex), State.ContactNormal);
		const float GroundSpeed = static_cast<float>(FVector::DotProduct(GetPointVelocity(State.ContactPoint), Forward));
		const bool bLocked = bHandbrakeInput && Setup.bAffectedByHandbrake;
		State.SpinSpeedDegPerSec = bLocked ? 0.f : FMath::RadiansToDegrees(GroundSpeed / FMath::Max(Setup.Radius, 1.f));
	}
	else
	{
		State.SpinSpeedDegPerSec *= FMath::Max(1.f - 0.5f * DeltaTime, 0.f); // wheels in the air slowly coast down
	}

	State.SpinAngleDeg = static_cast<float>(FRotator::ClampAxis(State.SpinAngleDeg + State.SpinSpeedDegPerSec * DeltaTime));
}
