// Tandem vehicle: the drivable pawn (components, input, wheel bones, camera).

#include "TandemVehicle.h"

#include "Tandem.h"
#include "TandemVehicleMovement.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

ATandemVehicle::ATandemVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // move the wheel bones after physics has moved the chassis

	// Physics decides which way the vehicle faces, not the controller. APawn turns controller yaw on by default.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	Chassis->InitBoxExtent(FVector(200.f, 100.f, 50.f));
	Chassis->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Chassis->SetSimulatePhysics(true);
	SetRootComponent(Chassis);

	VehicleMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(Chassis);
	VehicleMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	VehicleMesh->SetGenerateOverlapEvents(false);
	VehicleMesh->SetCanEverAffectNavigation(false);

	WheelBoneNames = { TEXT("Wheel_FL"), TEXT("Wheel_FR"), TEXT("Wheel_BL"), TEXT("Wheel_BR") };

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Chassis);
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 650.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 120.f);
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false; // keep the horizon level when the chassis pitches or rolls
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 5.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	VehicleMovement = CreateDefaultSubobject<UTandemVehicleMovement>(TEXT("VehicleMovement"));
}

void ATandemVehicle::BeginPlay()
{
	// Set the vehicle up before Super::BeginPlay, so Blueprint BeginPlay sees it ready.
	MeshRotationInChassis = VehicleMesh->GetRelativeTransform().GetRotation();

	// The tires are the lowest part of the mesh, so a wheel's radius is its bone's height above the mesh's bottom.
	const double MeshBottomZ = VehicleMesh->Bounds.Origin.Z - VehicleMesh->Bounds.BoxExtent.Z;

	TArray<FVector> WheelRestLocations;
	TArray<float> MeasuredRadii;
	WheelBoneIndices.Reset(WheelBoneNames.Num());
	WheelRestRotations.Reset(WheelBoneNames.Num());

	for (const FName& BoneName : WheelBoneNames)
	{
		const int32 BoneIndex = VehicleMesh->GetBoneIndex(BoneName);
		WheelBoneIndices.Add(BoneIndex);

		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTandem, Warning, TEXT("%s: the vehicle mesh has no bone named %s. Check VehicleMesh and Wheel Bone Names."), *GetNameSafe(this), *BoneName.ToString());
			WheelRestLocations.Add(VehicleMesh->GetComponentLocation());
			MeasuredRadii.Add(0.f);
			WheelRestRotations.Add(FQuat::Identity);
			continue;
		}

		const FVector BoneLocation = VehicleMesh->GetBoneLocationByName(BoneName, EBoneSpaces::WorldSpace);
		WheelRestLocations.Add(BoneLocation);
		MeasuredRadii.Add(static_cast<float>(BoneLocation.Z - MeshBottomZ));
		WheelRestRotations.Add(VehicleMesh->GetBoneRotationByName(BoneName, EBoneSpaces::ComponentSpace).Quaternion());
	}

	BaseArmRotation = SpringArm->GetRelativeRotation();

	VehicleMovement->InitializeVehicle(Chassis, WheelRestLocations, MeasuredRadii);
	VehicleMovement->ApplyTuning();

	if (!Chassis->IsSimulatingPhysics())
	{
		UE_LOG(LogTandem, Warning, TEXT("%s: the chassis isn't simulating physics."), *GetNameSafe(this));
	}

	Super::BeginPlay();
}

void ATandemVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateWheelVisuals();
	UpdateCamera(DeltaTime);
}

void ATandemVehicle::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// A local player took control: add the vehicle's keys on top of the template's.
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem();
	if (Subsystem && VehicleMappingContext)
	{
		Subsystem->AddMappingContext(VehicleMappingContext, MappingContextPriority);
	}
}

void ATandemVehicle::UnPossessed()
{
	// Controller is still set here: take the vehicle's keys back off, and let go of the pedals.
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem();
	if (Subsystem && VehicleMappingContext)
	{
		Subsystem->RemoveMappingContext(VehicleMappingContext);
	}
	ClearDriveInputs();

	Super::UnPossessed();
}

void ATandemVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTandem, Error, TEXT("%s needs an Enhanced Input component (Project Settings > Input > Default Input Component Class)."), *GetNameSafe(this));
		return;
	}

	if (!ThrottleAction || !BrakeAction || !SteerAction)
	{
		UE_LOG(LogTandem, Warning, TEXT("%s is missing its Throttle, Brake or Steer input action."), *GetNameSafe(this));
	}

	// Axes bind Triggered and Completed. Completed fires with a zero value on release, which lets go of the axis.
	if (ThrottleAction)
	{
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATandemVehicle::OnThrottle);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATandemVehicle::OnThrottle);
	}
	if (BrakeAction)
	{
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ATandemVehicle::OnBrake);
		EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ATandemVehicle::OnBrake);
	}
	if (SteerAction)
	{
		EnhancedInput->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ATandemVehicle::OnSteer);
		EnhancedInput->BindAction(SteerAction, ETriggerEvent::Completed, this, &ATandemVehicle::OnSteer);
	}
	if (HandbrakeAction)
	{
		EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ATandemVehicle::OnHandbrakePressed);
		EnhancedInput->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ATandemVehicle::OnHandbrakeReleased);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATandemVehicle::OnLook);
	}
	if (ResetAction)
	{
		EnhancedInput->BindAction(ResetAction, ETriggerEvent::Started, this, &ATandemVehicle::OnResetPressed);
	}
	if (ToggleDebugAction)
	{
		EnhancedInput->BindAction(ToggleDebugAction, ETriggerEvent::Started, this, &ATandemVehicle::OnToggleDebugPressed);
	}
}

// --- Input handlers ----------------------------------------------------------------------------

void ATandemVehicle::OnThrottle(const FInputActionValue& Value)
{
	VehicleMovement->SetThrottleInput(Value.Get<float>());
}

void ATandemVehicle::OnBrake(const FInputActionValue& Value)
{
	VehicleMovement->SetBrakeInput(Value.Get<float>());
}

void ATandemVehicle::OnSteer(const FInputActionValue& Value)
{
	VehicleMovement->SetSteeringInput(Value.Get<float>());
}

void ATandemVehicle::OnHandbrakePressed()
{
	VehicleMovement->SetHandbrakeInput(true);
}

void ATandemVehicle::OnHandbrakeReleased()
{
	VehicleMovement->SetHandbrakeInput(false);
}

void ATandemVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();

	FRotator ArmRotation = SpringArm->GetRelativeRotation();
	ArmRotation.Yaw = FRotator::NormalizeAxis(ArmRotation.Yaw + LookAxis.X * LookSpeed);
	ArmRotation.Pitch = FMath::Clamp(static_cast<float>(ArmRotation.Pitch + LookAxis.Y * LookSpeed), MinCameraPitch, MaxCameraPitch);
	SpringArm->SetRelativeRotation(ArmRotation);

	TimeSinceLookInput = 0.f;
}

void ATandemVehicle::OnResetPressed()
{
	ResetVehicle();
}

void ATandemVehicle::OnToggleDebugPressed()
{
	UTandemVehicleMovement::ToggleDebugDraw();
}

// --- Vehicle -----------------------------------------------------------------------------------

void ATandemVehicle::ResetVehicle()
{
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	const FVector LiftedLocation = GetActorLocation() + FVector::UpVector * ResetLiftHeight;
	const FRotator UprightRotation(0.f, GetActorRotation().Yaw, 0.f);
	SetActorLocationAndRotation(LiftedLocation, UprightRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATandemVehicle::ClearDriveInputs()
{
	VehicleMovement->SetThrottleInput(0.f);
	VehicleMovement->SetBrakeInput(0.f);
	VehicleMovement->SetSteeringInput(0.f);
	VehicleMovement->SetHandbrakeInput(false);
}

UEnhancedInputLocalPlayerSubsystem* ATandemVehicle::GetInputSubsystem() const
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	return PlayerController ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()) : nullptr;
}

void ATandemVehicle::UpdateWheelVisuals()
{
	const int32 Count = FMath::Min3(WheelBoneIndices.Num(), WheelRestRotations.Num(), VehicleMovement->GetNumWheelStates());
	const FTransform& ChassisTransform = Chassis->GetComponentTransform();
	const FTransform& MeshTransform = VehicleMesh->GetComponentTransform();
	const FQuat ChassisToMesh = MeshRotationInChassis.Inverse();

	for (int32 WheelIndex = 0; WheelIndex < Count; ++WheelIndex)
	{
		if (WheelBoneIndices[WheelIndex] == INDEX_NONE)
		{
			continue;
		}

		const FName BoneName = WheelBoneNames[WheelIndex];
		const FTandemWheelSetup& Setup = VehicleMovement->GetWheelSetup(WheelIndex);
		const FTandemWheelState& State = VehicleMovement->GetWheelState(WheelIndex);

		// Hang the wheel SuspensionLength below its mount (chassis space), then bring it into the mesh's space.
		const FVector WheelCenter = ChassisTransform.TransformPosition(Setup.MountOffset - FVector::UpVector * State.SuspensionLength);
		VehicleMesh->SetBoneLocationByName(BoneName, MeshTransform.InverseTransformPosition(WheelCenter), EBoneSpaces::ComponentSpace);

		// Steer and spin are chassis-space rotations; convert them to mesh space and apply them on top of the rest pose.
		const FQuat Steer(FVector::UpVector, FMath::DegreesToRadians(State.SteerAngleDeg));
		const FQuat Spin = FRotator(-State.SpinAngleDeg, 0.f, 0.f).Quaternion(); // negative pitch rolls the wheel forward
		const FQuat SteerSpinInMesh = ChassisToMesh * (Steer * Spin) * MeshRotationInChassis;
		VehicleMesh->SetBoneRotationByName(BoneName, (SteerSpinInMesh * WheelRestRotations[WheelIndex]).Rotator(), EBoneSpaces::ComponentSpace);
	}
}

void ATandemVehicle::UpdateCamera(float DeltaTime)
{
	TimeSinceLookInput += DeltaTime;
	if (CameraRecenterSpeed <= 0.f || TimeSinceLookInput < CameraRecenterDelay)
	{
		return;
	}

	const FRotator CurrentRotation = SpringArm->GetRelativeRotation();
	if (!CurrentRotation.Equals(BaseArmRotation, 0.01f))
	{
		SpringArm->SetRelativeRotation(FMath::RInterpTo(CurrentRotation, BaseArmRotation, DeltaTime, CameraRecenterSpeed));
	}
}
