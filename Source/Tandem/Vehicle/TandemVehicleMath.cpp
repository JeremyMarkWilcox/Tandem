// Tandem vehicle: pure math.

#include "TandemVehicleMath.h"

namespace TandemVehicleMath
{
	float CmPerSecToKmh(float CmPerSec)
	{
		return CmPerSec * 0.036f; // 1 cm/s = 0.01 m/s = 0.036 km/h
	}

	float KmhToCmPerSec(float Kmh)
	{
		return Kmh / 0.036f;
	}

	float SpringDamperForce(float Compression, float CompressionSpeed, float Stiffness, float Damping)
	{
		// Hooke's law plus a damper. Clamped at zero: a suspension can push the chassis up but never pull it down.
		const float Force = Compression * Stiffness + CompressionSpeed * Damping;
		return FMath::Max(Force, 0.f);
	}

	float DriveForce(float Throttle, float AvailableForce, float ForwardSpeedKmh, float MaxForwardSpeedKmh)
	{
		if (Throttle <= 0.f || MaxForwardSpeedKmh <= 0.f || ForwardSpeedKmh >= MaxForwardSpeedKmh)
		{
			return 0.f;
		}

		// Full force from a standstill (or rolling backwards), easing off on a curve so it reaches 0 at top speed.
		// Squaring keeps the car pulling hard through the middle of the range instead of fading early.
		const float SpeedRatio = FMath::Clamp(ForwardSpeedKmh / MaxForwardSpeedKmh, 0.f, 1.f);
		const float Falloff = 1.f - SpeedRatio * SpeedRatio;
		return FMath::Clamp(Throttle, 0.f, 1.f) * AvailableForce * Falloff;
	}

	float LateralGripForce(float LateralSpeed, float Load, float Grip, float DeltaTime, float CornerMass)
	{
		if (Load <= 0.f || DeltaTime <= 0.f)
		{
			return 0.f;
		}

		// The force that would cancel the slide in exactly one step, limited by what the tire can hold (Load x Grip).
		// Never pushing harder than that is what keeps the car from jittering side to side.
		const float StopSlide = -LateralSpeed * CornerMass / DeltaTime;
		const float MaxGrip = Load * Grip;
		return FMath::Clamp(StopSlide, -MaxGrip, MaxGrip);
	}

	float SteerAngleDeg(float SteerInput, float MaxAngleDeg)
	{
		return FMath::Clamp(SteerInput, -1.f, 1.f) * MaxAngleDeg;
	}
}
