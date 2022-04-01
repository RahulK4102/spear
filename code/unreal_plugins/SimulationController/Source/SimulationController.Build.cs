// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class SimulationController : ModuleRules
{
    public const String ASIO_INCLUDE = "/home/rachithp/code/github/unreal-ai/thirdparty/asio/asio/include";
    public const String RPCLIB_INCLUDE = "/home/rachithp/code/github/unreal-ai/thirdparty/rpclib/include";
    public const String RPCLIB_LIB = "/home/rachithp/code/github/unreal-ai/thirdparty/rpclib/build/librpc.a";
    public const String YAML_CPP_INCLUDE = "/home/rachithp/code/github/unreal-ai/thirdparty/yaml-cpp/include";
    public const String YAML_CPP_LIB = "/home/rachithp/code/github/unreal-ai/thirdparty/yaml-cpp/build/libyaml-cpp.a";

    public SimulationController(ReadOnlyTargetRules Target) : base(Target)
    {
        bEnableExceptions = true;

        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
				// ... add other public dependencies that you statically link with here ...
			}
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "InputCore", // This is required for using EKeys::W, etc 
				"CoreUObject",
                "Engine",
                "RHI",
                "RenderCore",
                // "RobotSim", // Uncomment this when using RobotSim
				// ... add private dependencies that you statically link with here ...	
			}
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );


        // asio
        PublicDefinitions.Add("ASIO_NO_TYPEID");
        PublicIncludePaths.Add(ASIO_INCLUDE);

        // rpclib
        PublicIncludePaths.Add(RPCLIB_INCLUDE);
        PublicAdditionalLibraries.Add(RPCLIB_LIB);

        // yaml-cpp
        PublicIncludePaths.Add(YAML_CPP_INCLUDE);
        PublicAdditionalLibraries.Add(YAML_CPP_LIB);
    }
}
