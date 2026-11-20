// Tandem vehicle: unit tests for TandemVehicleMath.
// Run them in the editor from Tools > Test Automation (filter "Tandem.Vehicle"), or from Rider's Unit Tests window.
// SpeedConversion passes now. The others describe the functions you're writing, so they fail until each one is done.

#include "TandemVehicleMath.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TandemVehicleMathTests
{
	// A 1500 kg car spread over four wheels, stepped at 60 fps.
	constexpr float CornerMassKg = 375.f;
	constexpr float CornerLoad = CornerMassKg * 980.f; // one corner's weight in Unreal force units
	constexpr float StepSeconds = 1.f / 60.f;

	// Same as the default tuning.
	constexpr float Stiffness = 40000.f;
	constexpr float Damping = 3000.f;
	constexpr float AvailableDriveForce = 600000.f;
	constexpr float TopSpeedKmh = 140.f;
	constexpr float TireGrip = 1.2f;
	constexpr float MaxSteerDeg = 35.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTandemVehicleMathSpeedConversionTest, "Tandem.Vehicle.Math.SpeedConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTandemVehicleMathSpeedConversionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("36 km/h is 1000 cm/s"), TandemVehicleMath::KmhToCmPerSec(36.f), 1000.f, 0.01f);
	TestEqual(TEXT("1000 cm/s is 36 km/h"), TandemVehicleMath::CmPerSecToKmh(1000.f), 36.f, 0.001f);
	TestEqual(TEXT("Round trip"), TandemVehicleMath::CmPerSecToKmh(TandemVehicleMath::KmhToCmPerSec(123.4f)), 123.4f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTandemVehicleMathSpringDamperTest, "Tandem.Vehicle.Math.SpringDamper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTandemVehicleMathSpringDamperTest::RunTest(const FString& Parameters)
{
	using namespace TandemVehicleMathTests;

	TestEqual(TEXT("At rest and still: no force"), TandemVehicleMath::SpringDamperForce(0.f, 0.f, Stiffness, Damping), 0.f, 0.01f);
	TestEqual(TEXT("Compressed 10 cm: Stiffness is force per cm"), TandemVehicleMath::SpringDamperForce(10.f, 0.f, Stiffness, Damping), 400000.f, 1.f);
	TestEqual(TEXT("Compressing at 50 cm/s: Damping adds force per cm/s"), TandemVehicleMath::SpringDamperForce(10.f, 50.f, Stiffness, Damping), 550000.f, 1.f);
	TestEqual(TEXT("Extending at 50 cm/s: Damping takes force away"), TandemVehicleMath::SpringDamperForce(10.f, -50.f, Stiffness, Damping), 250000.f, 1.f);

	// Design choice: a suspension pushes the chassis up but never pulls it down. Delete these two if you want a sticky one.
	TestEqual(TEXT("Extending fast never pulls"), TandemVehicleMath::SpringDamperForce(1.f, -1000.f, Stiffness, Damping), 0.f, 0.01f);
	TestEqual(TEXT("Past rest never pulls"), TandemVehicleMath::SpringDamperForce(-5.f, 0.f, Stiffness, Damping), 0.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTandemVehicleMathDriveForceTest, "Tandem.Vehicle.Math.DriveForce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTandemVehicleMathDriveForceTest::RunTest(const FString& Parameters)
{
	using namespace TandemVehicleMathTests;

	const float FullFromStop = TandemVehicleMath::DriveForce(1.f, AvailableDriveForce, 0.f, TopSpeedKmh);
	const float HalfFromStop = TandemVehicleMath::DriveForce(0.5f, AvailableDriveForce, 0.f, TopSpeedKmh);
	const float FullRollingBack = TandemVehicleMath::DriveForce(1.f, AvailableDriveForce, -20.f, TopSpeedKmh);

	TestEqual(TEXT("No throttle: no push"), TandemVehicleMath::DriveForce(0.f, AvailableDriveForce, 50.f, TopSpeedKmh), 0.f, 0.01f);
	TestTrue(TEXT("Full throttle from a stop pushes forward"), FullFromStop > 0.f);
	TestTrue(TEXT("Never more than the available force"), FullFromStop <= AvailableDriveForce + 1.f);
	TestTrue(TEXT("Rolling backwards: still forward, still within the available force"), FullRollingBack > 0.f && FullRollingBack <= AvailableDriveForce + 1.f);
	TestTrue(TEXT("Half throttle pushes, but less than full"), HalfFromStop > 0.f && HalfFromStop < FullFromStop);
	TestEqual(TEXT("No push at top speed"), TandemVehicleMath::DriveForce(1.f, AvailableDriveForce, TopSpeedKmh, TopSpeedKmh), 0.f, 1.f);
	TestTrue(TEXT("No push above top speed"), TandemVehicleMath::DriveForce(1.f, AvailableDriveForce, TopSpeedKmh + 10.f, TopSpeedKmh) <= 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTandemVehicleMathLateralGripTest, "Tandem.Vehicle.Math.LateralGrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTandemVehicleMathLateralGripTest::RunTest(const FString& Parameters)
{
	using namespace TandemVehicleMathTests;

	const float MaxGripForce = CornerLoad * TireGrip;

	TestEqual(TEXT("Not sliding: no force"), TandemVehicleMath::LateralGripForce(0.f, CornerLoad, TireGrip, StepSeconds, CornerMassKg), 0.f, 0.01f);
	TestTrue(TEXT("Sliding right pushes left"), TandemVehicleMath::LateralGripForce(200.f, CornerLoad, TireGrip, StepSeconds, CornerMassKg) < 0.f);
	TestTrue(TEXT("Sliding left pushes right"), TandemVehicleMath::LateralGripForce(-200.f, CornerLoad, TireGrip, StepSeconds, CornerMassKg) > 0.f);
	TestEqual(TEXT("Sliding hard uses all the grip: Load * Grip"), TandemVehicleMath::LateralGripForce(2000.f, CornerLoad, TireGrip, StepSeconds, CornerMassKg), -MaxGripForce, 1.f);
	TestEqual(TEXT("No load (wheel in the air): no grip"), TandemVehicleMath::LateralGripForce(200.f, 0.f, TireGrip, StepSeconds, CornerMassKg), 0.f, 0.01f);

	// Design choice: never push harder than it takes to stop the slide this step, or the car jitters side to side.
	const float SmallSlide = 2.f;
	const float StopThisStep = SmallSlide * CornerMassKg / StepSeconds;
	TestTrue(TEXT("A small slide is stopped, not overshot"),
		FMath::Abs(TandemVehicleMath::LateralGripForce(SmallSlide, CornerLoad, TireGrip, StepSeconds, CornerMassKg)) <= StopThisStep + 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTandemVehicleMathSteeringTest, "Tandem.Vehicle.Math.Steering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTandemVehicleMathSteeringTest::RunTest(const FString& Parameters)
{
	using namespace TandemVehicleMathTests;

	const float HalfRight = TandemVehicleMath::SteerAngleDeg(0.5f, MaxSteerDeg);

	TestEqual(TEXT("No input: wheels straight"), TandemVehicleMath::SteerAngleDeg(0.f, MaxSteerDeg), 0.f, 0.001f);
	TestEqual(TEXT("Full right"), TandemVehicleMath::SteerAngleDeg(1.f, MaxSteerDeg), MaxSteerDeg, 0.001f);
	TestEqual(TEXT("Full left"), TandemVehicleMath::SteerAngleDeg(-1.f, MaxSteerDeg), -MaxSteerDeg, 0.001f);
	TestTrue(TEXT("Half input steers part way"), HalfRight > 0.f && HalfRight < MaxSteerDeg);
	TestEqual(TEXT("Input past 1 is clamped"), TandemVehicleMath::SteerAngleDeg(2.f, MaxSteerDeg), MaxSteerDeg, 0.001f);
	TestEqual(TEXT("Input past -1 is clamped"), TandemVehicleMath::SteerAngleDeg(-2.f, MaxSteerDeg), -MaxSteerDeg, 0.001f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
