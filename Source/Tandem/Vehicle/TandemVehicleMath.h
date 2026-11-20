// Tandem vehicle: pure math (no actors, components or world), unit-tested in Tests/TandemVehicleMathTests.cpp.

#pragma once

#include "CoreMinimal.h"

/** Units: cm, kg, s. Forces are Unreal force units (kg*cm/s^2). */
namespace TandemVehicleMath
{
	TANDEM_API float CmPerSecToKmh(float CmPerSec);
	TANDEM_API float KmhToCmPerSec(float Kmh);

	/**
	 * Suspension force pushing the chassis away from the ground.
	 * @param Compression       How far the spring is compressed past rest (cm). 0 = at rest, positive = compressed.
	 * @param CompressionSpeed  How fast the compression is changing (cm/s). Positive = compressing.
	 * @param Stiffness         Force per cm of compression.
	 * @param Damping           Force per cm/s of compression speed.
	 */
	TANDEM_API float SpringDamperForce(float Compression, float CompressionSpeed, float Stiffness, float Damping);

	/**
	 * Drive force along the wheel's forward direction (positive = forward).
	 * @param Throttle            0..1.
	 * @param AvailableForce      Force from the drive curve at the current speed.
	 * @param ForwardSpeedKmh     Signed forward speed (km/h).
	 * @param MaxForwardSpeedKmh  Top speed (km/h).
	 */
	TANDEM_API float DriveForce(float Throttle, float AvailableForce, float ForwardSpeedKmh, float MaxForwardSpeedKmh);

	/**
	 * Sideways tire force along the wheel's right axis (positive = right).
	 * @param LateralSpeed  Velocity at the contact along the wheel's right axis (cm/s). Positive = sliding right.
	 * @param Load          How hard the wheel presses on the ground.
	 * @param Grip          Friction coefficient, after curve and handbrake multipliers.
	 * @param DeltaTime     Step length (s).
	 * @param CornerMass    Share of the vehicle's mass on this wheel (kg).
	 */
	TANDEM_API float LateralGripForce(float LateralSpeed, float Load, float Grip, float DeltaTime, float CornerMass);

	/**
	 * Target steering angle in degrees (positive = right).
	 * @param SteerInput   -1..1.
	 * @param MaxAngleDeg  Max angle at the current speed.
	 */
	TANDEM_API float SteerAngleDeg(float SteerInput, float MaxAngleDeg);
}
