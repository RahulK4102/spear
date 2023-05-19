//
// Copyright(c) 2022 Intel. Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//

#include "WheeledVehicle/VehiclePawn.h"

#include <vector>

#include <Eigen/Dense>

#include <Animation/AnimInstance.h>
#include <Camera/CameraComponent.h>
#include <Components/BoxComponent.h>
#include <Components/InputComponent.h>
#include <Components/SkeletalMeshComponent.h>
#include <Engine/CollisionProfile.h>
#include <UObject/ConstructorHelpers.h>

#include "CoreUtils/Assert.h"
#include "CoreUtils/Config.h"
#include "CoreUtils/Log.h"
#include "CoreUtils/Unreal.h"
#include "WheeledVehicle/VehicleMovementComponent.h"

AVehiclePawn::AVehiclePawn(const FObjectInitializer& object_initializer) : APawn(object_initializer)
{
    SP_LOG_CURRENT_FUNCTION();

    if (!Config::s_initialized_) {
        return;
    }

    ConstructorHelpers::FObjectFinder<USkeletalMesh> skeletal_mesh(*Unreal::toFString(Config::get<std::string>("WHEELED_VEHICLE.VEHICLE_PAWN.SKELETAL_MESH")));
    SP_ASSERT(skeletal_mesh.Succeeded());

    ConstructorHelpers::FClassFinder<UAnimInstance> anim_instance(*Unreal::toFString(Config::get<std::string>("WHEELED_VEHICLE.VEHICLE_PAWN.ANIM_INSTANCE")));
    SP_ASSERT(anim_instance.Succeeded());

    skeletal_mesh_component_ = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AVehiclePawn::skeletal_mesh_component_"));
    SP_ASSERT(skeletal_mesh_component_);
    skeletal_mesh_component_->SetSkeletalMesh(skeletal_mesh.Object);
    skeletal_mesh_component_->SetAnimClass(anim_instance.Class);
    skeletal_mesh_component_->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
    skeletal_mesh_component_->BodyInstance.bSimulatePhysics = true;
    skeletal_mesh_component_->BodyInstance.bNotifyRigidBodyCollision = true;
    skeletal_mesh_component_->BodyInstance.bUseCCD = true;
    skeletal_mesh_component_->bBlendPhysics = true;
    skeletal_mesh_component_->SetGenerateOverlapEvents(true);
    skeletal_mesh_component_->SetCanEverAffectNavigation(false);

    RootComponent = skeletal_mesh_component_;

    // Setup vehicle movement
    vehicle_movement_component_ = CreateDefaultSubobject<UVehicleMovementComponent>(TEXT("AVehiclePawn::vehicle_movement_component_"));
    SP_ASSERT(vehicle_movement_component_);
    vehicle_movement_component_->SetIsReplicated(true); // Enable replication by default
    vehicle_movement_component_->UpdatedComponent = skeletal_mesh_component_;
    // this ensures that the body doesn't ever sleep. Need this to bypass a Chaos bug that doesn't take torque inputs to wheels into consideration for determining the sleep state of the body.
    vehicle_movement_component_->SleepThreshold = 0;

    // Setup camera
    FVector camera_location(
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.POSITION_X"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.POSITION_Y"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.POSITION_Z"));

    FRotator camera_orientation(
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.PITCH"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.YAW"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.ROLL"));

    camera_component_ = CreateDefaultSubobject<UCameraComponent>(TEXT("AVehiclePawn::camera_component_"));
    SP_ASSERT(camera_component_);

    camera_component_->SetRelativeLocationAndRotation(camera_location, camera_orientation);
    camera_component_->SetupAttachment(skeletal_mesh_component_);
    camera_component_->bUsePawnControlRotation = false;
    camera_component_->FieldOfView = Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.CAMERA_COMPONENT.FOV");

    // Setup IMU sensor
    FVector imu_location(
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.POSITION_X"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.POSITION_Y"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.POSITION_Z"));

    FRotator imu_orientation(
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.PITCH"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.YAW"),
        Config::get<float>("WHEELED_VEHICLE.VEHICLE_PAWN.IMU_COMPONENT.ROLL"));

    imu_component_ = CreateDefaultSubobject<UBoxComponent>(TEXT("AVehiclePawn::imu_component_"));
    ASSERT(imu_component_);

    imu_component_->SetRelativeLocationAndRotation(imu_location, imu_orientation);
    imu_component_->SetupAttachment(skeletal_mesh_component_);
}

AVehiclePawn::~AVehiclePawn()
{
    SP_LOG_CURRENT_FUNCTION();
}

void AVehiclePawn::SetupPlayerInputComponent(UInputComponent* input_component)
{
    SP_ASSERT(input_component);
    APawn::SetupPlayerInputComponent(input_component);

    input_component->BindAxis("MoveForward", this, &AVehiclePawn::moveForward);
    input_component->BindAxis("MoveRight", this, &AVehiclePawn::moveRight);
}

void AVehiclePawn::moveForward(float forward)
{
    float torque = forward * 10.0;
    vehicle_movement_component_->SetDriveTorque(torque, 0);
    vehicle_movement_component_->SetDriveTorque(torque, 1);
    vehicle_movement_component_->SetDriveTorque(torque, 2);
    vehicle_movement_component_->SetDriveTorque(torque, 3);
}

void AVehiclePawn::moveRight(float right)
{
    float torque = right * 10.0;
    vehicle_movement_component_->SetDriveTorque(torque, 0);
    vehicle_movement_component_->SetDriveTorque(-1.0 * torque, 1);
    vehicle_movement_component_->SetDriveTorque(torque, 2);
    vehicle_movement_component_->SetDriveTorque(-1.0 * torque, 3);
}

// Apply the drive torque in[N.m] to the vehicle wheels.The applied driveTorque persists until the
// next call to SetDriveTorque.Note that the SetDriveTorque command can be found in the code of the
// Unreal Engine at the following location :
//     Engine / Plugins / Runtime / PhysXVehicles / Source / PhysXVehicles / Public / SimpleWheeledVehicleMovementComponent.h
// This file also contains a bunch of useful functions such as SetBrakeTorque or SetSteerAngle.
// Please take a look if you want to modify the way the simulated vehicle is being controlled.
void AVehiclePawn::setDriveTorques(const Eigen::Vector4d& drive_torques)
{
    vehicle_movement_component_->SetDriveTorque(drive_torques(0), 0);
    vehicle_movement_component_->SetDriveTorque(drive_torques(1), 1);
    vehicle_movement_component_->SetDriveTorque(drive_torques(2), 2);
    vehicle_movement_component_->SetDriveTorque(drive_torques(3), 3);
}

void AVehiclePawn::setBrakeTorques(const Eigen::Vector4d& brake_torques)
{
    vehicle_movement_component_->SetBrakeTorque(brake_torques(0), 0);
    vehicle_movement_component_->SetBrakeTorque(brake_torques(1), 1);
    vehicle_movement_component_->SetBrakeTorque(brake_torques(2), 2);
    vehicle_movement_component_->SetBrakeTorque(brake_torques(3), 3);
}

Eigen::Vector4d AVehiclePawn::getWheelRotationSpeeds() const
{
    return vehicle_movement_component_->getWheelRotationSpeeds(); // Expressed in [rad/s]
}

void AVehiclePawn::resetVehicle()
{
    vehicle_movement_component_->ResetVehicle();
}
