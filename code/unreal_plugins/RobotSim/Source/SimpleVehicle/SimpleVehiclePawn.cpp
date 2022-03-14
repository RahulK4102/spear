
PRAGMA_DISABLE_DEPRECATION_WARNINGS

#include "SimpleVehiclePawn.h"
#include "SimpleWheel.h"
#include "OpenBotWheel.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include <iostream>

FName ASimpleVehiclePawn::VehicleMovementComponentName(
    TEXT("SimpleWheeledVehicleMovement"));
FName ASimpleVehiclePawn::VehicleMeshComponentName(TEXT("VehicleMesh'"));

ASimpleVehiclePawn::ASimpleVehiclePawn(
    const FObjectInitializer& ObjectInitializer)
    : APawn(ObjectInitializer)
{
    // To create components, you can use
    // CreateDefaultSubobject<Type>("InternalName").
    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(
        VehicleMeshComponentName);
    // setup skeletal mesh
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(
        TEXT("/RobotSim/SimpleVehicle/freight/freight.freight"));
    Mesh->SetSkeletalMesh(CarMesh.Object);
    // setup animation
    static ConstructorHelpers::FClassFinder<UAnimInstance> finderAnim(
        TEXT("/RobotSim/SimpleVehicle/freight/"
             "freight_Animation.freight_Animation_C"));
    if (finderAnim.Succeeded())
    {
        Mesh->SetAnimClass(finderAnim.Class);
        UE_LOG(LogTemp, Warning, TEXT("finderAnim success"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("finderAnim failed"));
    }

    Mesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
    Mesh->BodyInstance.bSimulatePhysics = true;
    Mesh->BodyInstance.bNotifyRigidBodyCollision = true;
    Mesh->BodyInstance.bUseCCD = true;
    Mesh->bBlendPhysics = true;
    Mesh->SetGenerateOverlapEvents(true);
    Mesh->SetCanEverAffectNavigation(false);
    // example for adding user-defined collision callback
    // Mesh->OnComponentHit.AddDynamic(this,
    //                                &ASimpleVehiclePawn::OnComponentCollision);

    RootComponent = Mesh;

    VehicleMovement =
        CreateDefaultSubobject<UWheeledVehicleMovementComponent,
                               USimpleWheeledVehicleMovementComponent>(
            VehicleMovementComponentName);

    // setup wheels
    VehicleMovement->WheelSetups.SetNum(4);
    // TODO dynamic tire?
    UClass* wheelClasss = USimpleWheel::StaticClass();
    // https://answers.unrealengine.com/questions/325623/view.html
    VehicleMovement->WheelSetups[0].WheelClass = wheelClasss;
    VehicleMovement->WheelSetups[0].BoneName = FName("FL");
    VehicleMovement->WheelSetups[0].AdditionalOffset = FVector(0.f, -1.2f, 0.f);

    VehicleMovement->WheelSetups[1].WheelClass = wheelClasss;
    VehicleMovement->WheelSetups[1].BoneName = FName("FR");
    VehicleMovement->WheelSetups[1].AdditionalOffset = FVector(0.f, 1.2f, 0.f);

    VehicleMovement->WheelSetups[2].WheelClass = wheelClasss;
    VehicleMovement->WheelSetups[2].BoneName = FName("RL");
    VehicleMovement->WheelSetups[2].AdditionalOffset = FVector(0.f, -1.2f, 0.f);

    VehicleMovement->WheelSetups[3].WheelClass = wheelClasss;
    VehicleMovement->WheelSetups[3].BoneName = FName("RR");
    VehicleMovement->WheelSetups[3].AdditionalOffset = FVector(0.f, 1.2f, 0.f);

    VehicleMovement->SetIsReplicated(true); // Enable replication by default
    VehicleMovement->UpdatedComponent = Mesh;

    vehicle_pawn_ = static_cast<USimpleWheeledVehicleMovementComponent*>(
        GetVehicleMovementComponent());

    wheelVelocity_.setZero();
    motorVelocity_.setZero();
    counterElectromotiveForce_.setZero();
    motorTorque_.setZero();
    wheelTorque_.setZero();
    dutyCycle_.setZero();
    motorWindingCurrent_.setZero();
    actionVec_.setZero();
}

ASimpleVehiclePawn::~ASimpleVehiclePawn()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to
    // improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

void ASimpleVehiclePawn::SetupPlayerInputComponent(
    class UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // set up gameplay key bindings in RobotSimVehicleGameMode
    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this,
                                   &ASimpleVehiclePawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this,
                                   &ASimpleVehiclePawn::MoveRight);
}

void ASimpleVehiclePawn::SetupInputBindings()
{
    UE_LOG(LogTemp, Warning,
           TEXT("ASimpleVehiclePawn::SetupInputBindings start"));
    this->EnableInput(this->GetWorld()->GetFirstPlayerController());
    UE_LOG(LogTemp, Warning,
           TEXT("ASimpleVehiclePawn::SetupInputBindings end"));
    // keyboard control in RobotSimGameMode
    APlayerController* controller =
        this->GetWorld()->GetFirstPlayerController();

    controller->InputComponent->BindAxis("MoveForward", this,
                                         &ASimpleVehiclePawn::MoveForward);
    controller->InputComponent->BindAxis("MoveRight", this,
                                         &ASimpleVehiclePawn::MoveRight);
}

// This command is meant to be bound to keyboard input. It will be executed at
// each press or unpress event.
void ASimpleVehiclePawn::MoveForward(float Forward)
{
    // Forward describes the percentage of input voltage to be applied to the
    // motor by the H-bridge controller: 1.0 = 100%, -1.0 = reverse 100%.

    dutyCycle_(0) += Forward; // in [%]
    dutyCycle_(1) += Forward; // in [%]
    dutyCycle_(2) += Forward; // in [%]
    dutyCycle_(3) += Forward; // in [%]
}

// This command is meant to be bound to keyboard input. It will be executed at
// each press or unpress event.
void ASimpleVehiclePawn::MoveRight(float Right)
{
    // Right describes the percentage of input voltage to be applied to the
    // motor by the H-bridge controller: 1.0 = 100%, -1.0 = reverse 100%.

    dutyCycle_(0) += Right; // in [%]
    dutyCycle_(1) -= Right; // in [%]
    dutyCycle_(2) += Right; // in [%]
    dutyCycle_(3) -= Right; // in [%]
}

// This command is meant to be used by the python client interface.
void ASimpleVehiclePawn::Move(float leftCtrl, float rightCtrl)
{
    // leftCtrl and rightCtrl describe the percentage of input voltage to be
    // applied to the left and right motors by the H-bridge controller: 1 =
    // 100%, -1 = reverse 100%.

    dutyCycle_(0) += leftCtrl;  // in [%]
    dutyCycle_(1) += rightCtrl; // in [%]
    dutyCycle_(2) += leftCtrl;  // in [%]
    dutyCycle_(3) += rightCtrl; // in [%]
}

// Provides feedback on the action executed by the robot. This action can either
// be defined through the python client or by keyboard/game controller input.
void ASimpleVehiclePawn::GetExecutedAction(std::vector<float>& ActionVec)
{
    ActionVec[0] = actionVec_(0);
    ActionVec[1] = actionVec_(1);
}

void ASimpleVehiclePawn::computeMotorTorques(float DeltaTime)
{
    std::cout << "Owner->GetVelocity(): " << this->GetVelocity().Size()
              << std::endl; // in cm/s
    // First make sure the duty cycle is not getting above 100%. This is done
    // simillarly on the real OpenBot: (c.f.
    // https://github.com/isl-org/OpenBot/blob/master/android/app/src/main/java/org/openbot/vehicle/Control.java)
    dutyCycle_ = RobotSim::clamp(dutyCycle_, -Eigen::Vector4f::Ones(),
                                 Eigen::Vector4f::Ones());

    // Acquire the ground truth motor and wheel velocity for motor
    // counter-electromotive force computation purposes (or alternatively
    // friction computation purposes):
    wheelVelocity_(0) =
        vehicle_pawn_->PVehicle->mWheelsDynData.getWheelRotationSpeed(
            0); // Expressed in [RPM]
    wheelVelocity_(1) =
        vehicle_pawn_->PVehicle->mWheelsDynData.getWheelRotationSpeed(
            1); // Expressed in [RPM]
    wheelVelocity_(2) =
        vehicle_pawn_->PVehicle->mWheelsDynData.getWheelRotationSpeed(
            2); // Expressed in [RPM]
    wheelVelocity_(3) =
        vehicle_pawn_->PVehicle->mWheelsDynData.getWheelRotationSpeed(
            3); // Expressed in [RPM]

    motorVelocity_ = gearRatio_ * RobotSim::RPMToRadSec(
                                      wheelVelocity_); // Expressed in [rad/s]

    // Compute the counter electromotive force using the motor torque constant:
    counterElectromotiveForce_ =
        motorTorqueConstant_ * motorVelocity_; // Expressed in [V]

    // The current allowed to circulate in the motor is the result of the
    // difference between the applied voltage and the counter electromotive
    // force:
    motorWindingCurrent_ =
        ((batteryVoltage_ * dutyCycle_) - counterElectromotiveForce_) /
        electricalResistance_; // Expressed in [A]
    // motorWindingCurrent_ = motorWindingCurrent_ *
    // (1-(electricalResistance_/electricalInductance_)*DeltaTime) +
    // ((batteryVoltage_*dutyCycle_)-counterElectromotiveForce_)*DeltaTime/electricalInductance_;
    // // If deltaTime is "small enouth" (which is definitely not the case here)

    // The torque is then obtained using the torque coefficient of the motor:
    motorTorque_ = motorTorqueConstant_ * motorWindingCurrent_;

    // Motor torque is saturated to match the motor limits:
    motorTorque_ = RobotSim::clamp(motorTorque_,
                                   Eigen::Vector4f::Constant(-motorTorqueMax_),
                                   Eigen::Vector4f::Constant(motorTorqueMax_));

    // The torque applied to the robot wheels is finally computed acounting for
    // the gear ratio:
    wheelTorque_ = gearRatio_ * motorTorque_;

    // Control dead zone at near-zero velocity:
    // Note: this is a simplified but reliable way to deal with the friction
    // behavior observed on the real vehicle in the
    // low-velocities/low-duty-cycle dommain.
    for (size_t i = 0; i < dutyCycle_.size(); i++)
    {
        if (std::abs(motorVelocity_(i)) < 1e-5 and
            std::abs(dutyCycle_(i)) <=
                controlDeadZone_ /
                    actionScale_) // If the motor is "nearly" stopped
        {
            wheelTorque_(i) = 0.f;
        }
    }

    // Apply the drive torque in [N.m] to the vehicle wheels. Note that the
    // "SetDriveTorque" command can be found in the code of the unreal engine
    // at the following location:
    // UnrealEngine-4.26.2-release/Engine/Plugins/Runtime/PhysXVehicles/Source/PhysXVehicles/Public/SimpleWheeledVehicleMovementComponent.h.
    // This file also contains a bunch of useful functions such as
    // "SetBrakeTorque" or "SetSteerAngle". Please take a look if you want to
    // modify the way the simulated vehicle is being controlled.
    vehicle_pawn_->SetDriveTorque(
        wheelTorque_(0),
        0); // Torque applied to the wheel, expressed in [N.m]
    vehicle_pawn_->SetDriveTorque(
        wheelTorque_(1),
        1); // Torque applied to the wheel, expressed in [N.m]
    vehicle_pawn_->SetDriveTorque(
        wheelTorque_(2),
        2); // Torque applied to the wheel, expressed in [N.m]
    vehicle_pawn_->SetDriveTorque(
        wheelTorque_(3),
        3); // Torque applied to the wheel, expressed in [N.m]

    // Fill the observed action vector to be used for RL purposes:
    actionVec_(0) = (dutyCycle_(0) + dutyCycle_(2)) / 2; // leftCtrl
    actionVec_(1) = (dutyCycle_(1) + dutyCycle_(3)) / 2; // rightCtrl

    // std::cout << " ----------------------------------------------- " <<
    // std::endl; std::cout << "actionVec_ = " << actionVec_.transpose() <<
    // std::endl; std::cout << "dutyCycle_ = " << dutyCycle_.transpose() <<
    // std::endl; std::cout << "motorVelocity_ = " << motorVelocity_.transpose()
    // << std::endl; std::cout << "wheelVelocity_ = " <<
    // wheelVelocity_.transpose() << std::endl; std::cout << "appliedVoltage = "
    // << (batteryVoltage_*dutyCycle_).transpose() << std::endl; std::cout <<
    // "counterElectromotiveForce_ = " << counterElectromotiveForce_.transpose()
    // << std::endl; std::cout << "motorWindingCurrent_ = " <<
    // motorWindingCurrent_.transpose() << std::endl; std::cout << "motorTorque_
    // = " << motorTorque_.transpose() << std::endl; std::cout << "wheelTorque_
    // = " << wheelTorque_.transpose() << std::endl;

    // Reset duty cycle value:
    dutyCycle_.setZero();
}

// Called every simulator update
void ASimpleVehiclePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // std::cout << "DeltaTime = " << DeltaTime<< std::endl;

    computeMotorTorques(DeltaTime);

    const FVector CurrentLocation = this->GetActorLocation();
    const FRotator CurrentRotation = this->GetActorRotation();
    URobotBlueprintLib::LogMessage(FString("Tick - "),
                                   FString::SanitizeFloat(this->count),
                                   LogDebugLevel::Informational, 30);
    count++;
}

void ASimpleVehiclePawn::SetRobotParameters(
    const RobotSim::RobotSimSettings::VehicleSetting& settings)
{
    gearRatio_ = settings.actuationSetting.gearRatio;
    motorVelocityConstant_ = settings.actuationSetting.motorVelocityConstant;
    motorTorqueConstant_ = 1 / motorVelocityConstant_;
    controlDeadZone_ = settings.actuationSetting.controlDeadZone;
    motorTorqueMax_ = settings.actuationSetting.motorTorqueMax;
    electricalResistance_ = settings.actuationSetting.electricalResistance;
    electricalInductance_ = settings.actuationSetting.electricalInductance;
    actionScale_ = settings.actuationSetting.actionScale;
    batteryVoltage_ = settings.actuationSetting.batteryVoltage;
}

void ASimpleVehiclePawn::NotifyHit(class UPrimitiveComponent* HitComponent,
                                   class AActor* OtherActor,
                                   class UPrimitiveComponent* otherComp,
                                   bool bSelfMoved,
                                   FVector hitLocation,
                                   FVector hitNormal,
                                   FVector normalImpulse,
                                   const FHitResult& hit)
{
    /*
        URobotBlueprintLib::LogMessage(FString("NotifyHit: ") +
                                           OtherActor->GetName(),
                                       " location: " + hitLocation.ToString() +
                                           " normal: " +
       normalImpulse.ToString(), LogDebugLevel::Informational, 30);
                                       */
    this->mPawnEvents.getCollisionSignal().emit(
        HitComponent, OtherActor, otherComp, bSelfMoved, hitLocation, hitNormal,
        normalImpulse, hit);
}

void ASimpleVehiclePawn::OnComponentCollision(UPrimitiveComponent* HitComponent,
                                              AActor* OtherActor,
                                              UPrimitiveComponent* OtherComp,
                                              FVector NormalImpulse,
                                              const FHitResult& Hit)
{
    URobotBlueprintLib::LogMessage(FString("OnComponentCollision: ") +
                                       OtherActor->GetName(),
                                   " location: " + Hit.Location.ToString() +
                                       " normal: " + Hit.Normal.ToString(),
                                   LogDebugLevel::Informational, 30);
}

// Called when the game starts or when spawned
void ASimpleVehiclePawn::BeginPlay()
{
    Super::BeginPlay();
}

USceneComponent* ASimpleVehiclePawn::GetComponent(FString componentName)
{
    throw std::runtime_error("Requested component named " +
                             std::string(TCHAR_TO_UTF8(*componentName)) +
                             " in GetComponent(), which does not exist.");
}

void ASimpleVehiclePawn::GetComponentReferenceTransform(FString componentName,
                                                        FVector& translation,
                                                        FRotator& rotation)
{
    throw std::runtime_error("Requested component named " +
                             std::string(TCHAR_TO_UTF8(*componentName)) +
                             " in GetComponent(), which does not exist.");
}

void ASimpleVehiclePawn::TeleportToLocation(FVector position,
                                            FQuat orientation,
                                            bool teleport)
{
    FVector translation =
        (position * URobotBlueprintLib::GetWorldToMetersScale(this)) -
        this->GetActorLocation();
    FRotator rotation =
        (orientation * this->GetActorQuat().Inverse()).Rotator();
    RobotBase::TeleportToLocation(
        position * URobotBlueprintLib::GetWorldToMetersScale(this), orientation,
        teleport);
}
PawnEvents* ASimpleVehiclePawn::GetPawnEvents()
{
    return &(this->mPawnEvents);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
