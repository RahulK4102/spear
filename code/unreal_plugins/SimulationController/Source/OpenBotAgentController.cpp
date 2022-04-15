#include "OpenBotAgentController.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <Components/SceneCaptureComponent2D.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/TextureRenderTarget2D.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <GameFramework/Actor.h>
#include <UObject/UObjectGlobals.h>

#include <PIPCamera.h>
#include <SimpleVehicle/SimpleVehiclePawn.h>

#include "Assert.h"
#include "Box.h"
#include "Config.h"
#include "TickEvent.h"

OpenBotAgentController::OpenBotAgentController(UWorld* world)
{
    for (TActorIterator<AActor> actor_itr(world, AActor::StaticClass()); actor_itr; ++actor_itr) {
        std::string actor_name = TCHAR_TO_UTF8(*(*actor_itr)->GetName());
        if (actor_name == Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "AGENT_ACTOR_NAME"})) {
            ASSERT(!agent_actor_);
            agent_actor_ = *actor_itr;
        } else if (actor_name == Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "GOAL_ACTOR_NAME"})) {
            ASSERT(!goal_actor_);
            goal_actor_ = *actor_itr;
        }
    }
    ASSERT(agent_actor_);
    ASSERT(goal_actor_);

    // setup observation camera
    if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "OBSERVATION_MODE"}) == "mixed") {

        for (TActorIterator<AActor> actor_itr(world, AActor::StaticClass()); actor_itr; ++actor_itr) {
            std::string actor_name = TCHAR_TO_UTF8(*(*actor_itr)->GetName());
            if (actor_name == Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "MIXED_MODE", "OBSERVATION_CAMERA_ACTOR_NAME"})) {
                ASSERT(!observation_camera_actor_);
                observation_camera_actor_ = *actor_itr;
                break;
            }
        }
        ASSERT(observation_camera_actor_);
        
        // create SceneCaptureComponent2D and TextureRenderTarget2D
        scene_capture_component_ = static_cast<APIPCamera*>(observation_camera_actor_)->GetSceneCaptureComponent();
        ASSERT(scene_capture_component_);
    }

    agent_static_mesh_component_ = Cast<UStaticMeshComponent>(agent_actor_->GetRootComponent());
    ASSERT(agent_static_mesh_component_);

    goal_static_mesh_component_ = Cast<UStaticMeshComponent>(goal_actor_->GetRootComponent());
    ASSERT(goal_static_mesh_component_);

    // need to set this to apply forces or move objects
    agent_static_mesh_component_->SetMobility(EComponentMobility::Type::Movable);
}

OpenBotAgentController::~OpenBotAgentController()
{
    ASSERT(goal_static_mesh_component_);
    goal_static_mesh_component_ = nullptr;

    ASSERT(agent_static_mesh_component_); 
    agent_static_mesh_component_ = nullptr;

    if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "OBSERVATION_MODE"}) == "mixed") {
        ASSERT(scene_capture_component_);
        scene_capture_component_ = nullptr;

        ASSERT(observation_camera_actor_);
        observation_camera_actor_ = nullptr;
    }
    
    ASSERT(agent_actor_);
    agent_actor_ = nullptr;

    ASSERT(goal_actor_);
    goal_actor_ = nullptr;
}

std::map<std::string, Box> OpenBotAgentController::getActionSpace() const
{
    std::map<std::string, Box> action_space;
    Box box;
    
    box.low = -1.f;
    box.high = 1.f;
    box.shape = {2};
    box.dtype = DataType::Float32;
    action_space["apply_voltage"] = std::move(box);

    return action_space;
}

std::map<std::string, Box> OpenBotAgentController::getObservationSpace() const
{
    std::map<std::string, Box> observation_space;
    Box box;
    
    if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "OBSERVATION_MODE"}) == "mixed") {
        box.low = std::numeric_limits<float>::lowest();
        box.high = std::numeric_limits<float>::max();
        box.shape = {5};
        box.dtype = DataType::Float32;
        observation_space["physical_observation"] = std::move(box);

        box = Box();
        box.low = 0;
        box.high = 255;
        box.shape = {Config::getValue<unsigned long>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "MIXED_MODE", "IMAGE_HEIGHT"}),
                     Config::getValue<unsigned long>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "MIXED_MODE", "IMAGE_WIDTH"}),
                     3};
        box.dtype = DataType::UInteger8;
        observation_space["visual_observation"] = std::move(box);
    } else if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "OBSERVATION_MODE"}) == "physical") {
        box.low = std::numeric_limits<float>::lowest();
        box.high = std::numeric_limits<float>::max();
        box.shape = {5};
        box.dtype = DataType::Float32;
        observation_space["physical_observation"] = std::move(box);
    } else {
        ASSERT(false);
    }

    return observation_space;
}

void OpenBotAgentController::applyAction(const std::map<std::string, std::vector<float>>& action)
{
    ASSERT(action.count("apply_voltage"));
    ASSERT(isfinite(action.at("apply_voltage").at(0)));
    ASSERT(isfinite(action.at("apply_voltage").at(1)));

    // @TODO: This can be checked in python?
    ASSERT(action.at("apply_voltage").at(0) >= getActionSpace()["apply_voltage"].low && action.at("apply_voltage").at(0) <= getActionSpace()["apply_voltage"].high, "%f", action.at("apply_voltage").at(0));
    ASSERT(action.at("apply_voltage").at(1) >= getActionSpace()["apply_voltage"].low && action.at("apply_voltage").at(1) <= getActionSpace()["apply_voltage"].high, "%f", action.at("apply_voltage").at(1));

    static_cast<ASimpleVehiclePawn*>(agent_actor_)->MoveLeftRight(action.at("apply_voltage").at(0), action.at("apply_voltage").at(1));
}

std::map<std::string, std::vector<uint8_t>> OpenBotAgentController::getObservation() const
{
    std::map<std::string, std::vector<uint8_t>> observation;

    // get observations
    if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "OBSERVATION_MODE"}) == "mixed") {
        ASSERT(IsInGameThread());

        FTextureRenderTargetResource* target_resource = scene_capture_component_->TextureTarget->GameThread_GetRenderTargetResource();
        ASSERT(target_resource);

        TArray<FColor> pixels;

        struct FReadSurfaceContext
        {
            FRenderTarget* src_render_target_;
            TArray<FColor>& out_data_;
            FIntRect rect_;
            FReadSurfaceDataFlags flags_;
        };

        FReadSurfaceContext context = {target_resource, pixels, FIntRect(0, 0, target_resource->GetSizeXY().X, target_resource->GetSizeXY().Y), FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)};

        // Required for uint8 read mode
        context.flags_.SetLinearToGamma(false);

        ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)([context](FRHICommandListImmediate& RHICmdList) {
            RHICmdList.ReadSurfaceData(context.src_render_target_->GetRenderTargetTexture(), context.rect_, context.out_data_, context.flags_);
        });

        FRenderCommandFence ReadPixelFence;
        ReadPixelFence.BeginFence(true);
        ReadPixelFence.Wait(true);


        std::vector<uint8_t> image(Config::getValue<int>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "MIXED_MODE", "IMAGE_HEIGHT"}) *
                                   Config::getValue<int>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "MIXED_MODE", "IMAGE_WIDTH"}) *
                                   3);

        for (uint32 i = 0; i < static_cast<uint32>(pixels.Num()); ++i) {
            image.at(3 * i + 0) = pixels[i].R;
            image.at(3 * i + 1) = pixels[i].G;
            image.at(3 * i + 2) = pixels[i].B;
        }
        
        observation["visual_observation"] = std::move(image);
    } 
    
    const FVector agent_current_location = agent_actor_->GetActorLocation();
    const FRotator agent_current_orientation = agent_actor_->GetActorRotation();

    if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "PHYSICAL_MODE", "OBSERVATION"}) == "dist-sin-cos") {
        // Get relative position to the goal in the global coordinate system:
        const FVector2D relative_position_to_goal((goal_actor_->GetActorLocation() - agent_current_location).X, (goal_actor_->GetActorLocation() - agent_current_location).Y);

        // Compute Euclidean distance to target:
        float mag_relative_position_to_goal = relative_position_to_goal.Size();

        // Compute robot forward axis (global coordinate system)
        FVector forward_axis = FVector(1.f, 0.f, 0.f); // Front axis is the X axis.
        FVector forward_axis_rotated = agent_current_orientation.RotateVector(forward_axis);

        // Compute yaw in [rad]:
        float delta_yaw = std::atan2f(forward_axis_rotated.Y, forward_axis_rotated.X) - std::atan2f(relative_position_to_goal.Y, relative_position_to_goal.X);

        // Fit to range [-pi, pi]:
        if (delta_yaw > PI) {
            delta_yaw -= 2 * PI;
        } else if (delta_yaw <= -PI) {
            delta_yaw += 2 * PI;
        }

        // Check the actual OpenBot code:
        // https://github.com/isl-org/OpenBot/blob/7868c54742f8ba3df0ba2a886247a753df982772/android/app/src/main/java/org/openbot/pointGoalNavigation/PointGoalNavigationFragment.java#L103
        float sin_yaw = std::sinf(delta_yaw);
        float cos_yaw = std::cosf(delta_yaw);

        // Fuses the actions received from the python client with those received from the keyboard interface (if this interface is activated in the settings.json file)
        Eigen::Vector2f control_state = static_cast<ASimpleVehiclePawn*>(agent_actor_)->GetControlState();
        observation["physical_observation"] = serializeToUint8(std::vector<float>{control_state(0), control_state(1), mag_relative_position_to_goal, sin_yaw, cos_yaw});

    } else if (Config::getValue<std::string>({"SIMULATION_CONTROLLER", "OPENBOT_AGENT_CONTROLLER", "PHYSICAL_MODE", "OBSERVATION"}) == "yaw-x-y") {

        Eigen::Vector2f control_state = static_cast<ASimpleVehiclePawn*>(agent_actor_)->GetControlState();
        observation["physical_observation"] = serializeToUint8(std::vector<float>{control_state(0), control_state(1), FMath::DegreesToRadians(agent_current_orientation.Yaw), agent_current_location.X, agent_current_location.Y});

    } else {
        ASSERT(false);
    }

    //     if (mag_relative_position_to_goal < 10) // in [cm]
    //     {
    //         hitInfo_ = UHitInfo::Goal;
    //     }

    //     // Reward function is defined based on https://github.com/isl-org/OpenBot-Distributed/blob/main/trainer/ob_agents/envs/open_bot_replay_env.py
    //     switch (hitInfo_)
    //     {
    //     case UHitInfo::Goal:
    //         // Goal reached reward:
    //         AddReward(unrealrl::Config::GetValue<float>({"INTERIOR_SIM_BRIDGE", "REWARD_GOAL_REACHED"}));
    //         EndEpisode();
    //         break;
    //     case UHitInfo::Edge:
    //         // Obstacle penalty
    //         AddReward(-unrealrl::Config::GetValue<float>({"INTERIOR_SIM_BRIDGE", "REWARD_COLLISION"}));
    //         EndEpisode();
    //         break;
    //     case UHitInfo::NoHit:
    //         // Constant timestep penalty:
    //         AddReward(-1 / unrealrl::Config::GetValue<float>({"INTERIOR_SIM_BRIDGE", "MAX_STEPS_PER_EPISODE"}));
    //         // Goal distance penalty:
    //         AddReward(-mag_relative_position_to_goal * unrealrl::Config::GetValue<float>({"INTERIOR_SIM_BRIDGE", "REWARD_DISTANCE_GAIN"}));
    //         AddReward(-unrealrl::Config::GetValue<float>({"INTERIOR_SIM_BRIDGE", "REWARD_DISTANCE_BIAS"}));
    //         break;
    //     default:
    //         SetCurrentReward(0.0);
    //         break;
    //     }
    // } else {
    //     ASSERT(false);
    // }

    return observation;
}
