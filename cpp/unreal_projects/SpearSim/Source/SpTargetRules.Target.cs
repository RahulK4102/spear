//
// Copyright(c) 2022 Intel. Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//

using System;                          // Console, Exception
using System.IO;                       // Directory, DirectoryInfo, Path
using System.Runtime.CompilerServices; // CallerFilePath, CallerLineNumber, CallerMemberName
using UnrealBuildTool;                 // TargetInfo, TargetRules

public class SpTargetRulesTarget : TargetRules
{
    public SpTargetRulesTarget(TargetInfo targetInfo) : base(targetInfo)
    {
        SP_LOG_CURRENT_FUNCTION();

        // We need to set this to something other than Game or Editor or Program in order to successfully generate Visual Studio project files.
        // Needs to be overridden in derived classes.
        Type = TargetType.Client;

        // Added to projects by default in UE 5.2.
        DefaultBuildSettings = BuildSettingsVersion.V2;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
        ExtraModuleNames.Add("SpearSim");

        if (targetInfo.Platform == UnrealTargetPlatform.Win64) {

            // On Windows, we need to build an additional app so that calls to UE_Log and writes to std::cout are visible in the terminal.
            bBuildAdditionalConsoleApp = true;

            // Sometimes useful for debugging
            // bOverrideBuildEnvironment = true;
            // AdditionalCompilerArguments = "/showIncludes";

        } else if (targetInfo.Platform == UnrealTargetPlatform.Mac || targetInfo.Platform == UnrealTargetPlatform.Linux) {

            // On macOS and Linux, we need to remap the paths of our symbolic links as we're compiling our executable, so the paths that get
            // written into the application's debug symbols aren't symbolic links. This is necessary to enable debugging in XCode and LLDB.
            bOverrideBuildEnvironment = true;

            string arg = "";
            SP_LOG("Additional compiler arguments:");

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
                SP_LOG("    " + arg);

                // Old: path/to/spear/cpp/unreal_projects/SpearSim/Plugins/MyPlugin
                // New: path/to/spear/cpp/unreal_plugins/MyPlugin
                arg =
                    " -ffile-prefix-map=" +
                    Path.GetFullPath(Path.Combine(pluginDir)) + "=" +
                    Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "..", "..", "unreal_plugins", plugin));
                AdditionalCompilerArguments += arg;
                SP_LOG("    " + arg);
            }

            // Old: path/to/spear/cpp/unreal_projects/SpearSim/ThirdParty
            // New: path/to/spear/third_party
            arg =
                " -ffile-prefix-map=" +
                Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "ThirdParty")) + "=" +
                Path.GetFullPath(Path.Combine(ProjectFile.Directory.FullName, "..", "..", "..", "third_party"));
            AdditionalCompilerArguments += arg;
            SP_LOG("    " + arg);

            // The "-fexperimental-library" flag is required to enable support for std::ranges on Linux. This is because
            // UE 5.2 builds using Clang 15 on Linux, but std::ranges are not fully supported in Clang 15 without this
            // additional flag. The flag will not be necessary UE 5.3, which builds using Clang 16 on Linux.
            if (targetInfo.Platform == UnrealTargetPlatform.Linux) {
                AdditionalCompilerArguments = "-fexperimental-library";
            }

        } else if (targetInfo.Platform == UnrealTargetPlatform.IOS || targetInfo.Platform == UnrealTargetPlatform.TVOS) {
            SP_LOG("NOTE: We only expect to see targetInfo.Platform == UnrealTargetPlatform.IOS or targetInfo.Platform == UnrealTargetPlatform.TVOS when we're on macOS and we're attempting to generate XCode project files. If we're not on macOS generating XCode project files, target.Platform == UnrealTargetPlatform.IOS and target.Platform == UnrealTargetPlatform.TVOS are unexpected.");

        } else if (targetInfo.Platform == UnrealTargetPlatform.LinuxArm64) {
            SP_LOG("NOTE: We only expect to see targetInfo.Platform == UnrealTargetPlatform.LinuxArm64 when we're on Linux and the editor is attempting to open a uproject for the first time. If the editor is not attempting to open a uproject for the first time on Linux, target.Platform == UnrealTargetPlatform.LinuxArm64 is unexpected.");

        } else {
            throw new Exception(SP_LOG_GET_PREFIX() + "Unexpected target platform: " + targetInfo.Platform);
        }
    }

    protected void SP_LOG(string message, [CallerFilePath] string filePath="", [CallerLineNumber] int lineNumber=0)
    {
        Console.WriteLine(GetPrefix(filePath, lineNumber) + message);
    }

    protected void SP_LOG_CURRENT_FUNCTION([CallerFilePath] string filePath="", [CallerLineNumber] int lineNumber=0, [CallerMemberName] string memberName="")
    {
        Console.WriteLine(GetPrefix(filePath, lineNumber) + GetCurrentFunctionExpanded(memberName));
    }

    protected string SP_LOG_GET_PREFIX([CallerFilePath] string filePath="", [CallerLineNumber] int lineNumber=0)
    {
        return GetPrefix(filePath, lineNumber);
    }

    private string GetPrefix(string filePath, int lineNumber)
    {
        return "[SPEAR | " + GetCurrentFileAbbreviated(filePath) + ":" + lineNumber.ToString("D4") + "] ";
    }

    private string GetCurrentFileAbbreviated(string filePath)
    {
        return Path.GetFileName(filePath);
    }

    private string GetCurrentFunctionExpanded(string memberName)
    {
        string sep = memberName.StartsWith(".") ? "" : ".";
        return this.GetType() + sep + memberName;
    }
}
