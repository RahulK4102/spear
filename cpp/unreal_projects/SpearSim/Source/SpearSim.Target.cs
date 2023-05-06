//
// Copyright(c) 2022 Intel. Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//

using System;
using System.IO;
using UnrealBuildTool;

public class SpearSimTarget : TargetRules
{
    public SpearSimTarget(TargetInfo Target) : base(Target)
    {
        Console.WriteLine("[SPEAR | SpearSim.Target.cs] SpearSimTarget::SpearSimTarget");

        // Added to projects by default in UE 5.2. Note that the default value in UE 5.2 preview 2 for IncludeOrderVersion is
        // EngineIncludeOrderVersion.Unreal5_1, but that triggers a build warning.
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;

        // We don't include SpearSimEditor here, because the SpearSimEditor module needs to extend Unreal's UnrealEdEngine class, which is only
        // available in editor builds.
        ExtraModuleNames.AddRange(new string[] {"SpearSim"});

        if (Target.Platform == UnrealTargetPlatform.Win64) {

            // On Windows, we need to build an additional app so that calls to UE_Log and writes to std::cout are visible in the terminal.
            bBuildAdditionalConsoleApp = true;

            // Sometimes useful for debugging
            // bOverrideBuildEnvironment = true;
            // AdditionalCompilerArguments = "/showIncludes";

        } else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux) {

            // On macOS and Linux, we need to remap the paths of our symbolic links as we're compiling our executable, so the paths that get
            // written into the application's debug symbols aren't symbolic links. This is necessary to enable debugging in XCode and LLDB.
            bOverrideBuildEnvironment = true;
            AdditionalCompilerArguments = "";

            string arg = "";
            Console.WriteLine("[SPEAR | SpearSim.Target.cs] Additional compiler arguments:");

            foreach (string pluginDir in Directory.GetDirectories(Path.Combine(ProjectFile.Directory.FullName, "Plugins"))) {
                string plugin = (new DirectoryInfo(pluginDir)).Name;

                // Do the most specific substitution first. If we do a less specific substitution first, then we might not ever perform the
                // more specific substitution, depending on how the -ffile-prefix-map argument is handled by the compiler.                

                // Old: path/to/spear/cpp/unreal_projects/SpearSim/Plugins/MyPlugin/ThirdParty
                // New: path/to/spear/third_party
                arg =
                    " -ffile-prefix-map=" +
                    Path.GetFullPath(Path.Combine(pluginDir, "ThirdParty")) + "=" +
                    Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "..", "..", "..", "third_party"));
                AdditionalCompilerArguments += arg;
                Console.WriteLine("[SPEAR | SpearSim.Target.cs]     " + arg);

                // Old: path/to/spear/cpp/unreal_projects/SpearSim/Plugins/MyPlugin
                // New: path/to/spear/cpp/unreal_plugins/MyPlugin
                arg =
                    " -ffile-prefix-map=" +
                    Path.GetFullPath(Path.Combine(pluginDir)) + "=" +
                    Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "..", "..", "unreal_plugins", plugin));
                AdditionalCompilerArguments += arg;
                Console.WriteLine("[SPEAR | SpearSim.Target.cs]     " + arg);
            }

            // Old: path/to/spear/cpp/unreal_projects/SpearSim/ThirdParty
            // New: path/to/spear/third_party
            arg =
                " -ffile-prefix-map=" +
                Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "ThirdParty")) + "=" +
                Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "..", "..", "..", "third_party"));
            AdditionalCompilerArguments += arg;
            Console.WriteLine("[SPEAR | SpearSim.Target.cs]     " + arg);
        }

        // We can't throw an exception here, because when we invoke GenerateProjectFiles.bat or GenerateProjectFiles.sh (packaged with
        // the Unreal Engine), instances of this class get created with Target.Platform set to many different platforms (e.g., iOS, tvOS,
        // Win32, etc).
        // } else {
        //    throw new Exception("[SPEAR | SpearSim.Target.cs] Unexpected target platform: " + Target.Platform);            
        // }
    }
}
