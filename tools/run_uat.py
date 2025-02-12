#
# Copyright(c) 2022 Intel. Licensed under the MIT License <http://opensource.org/licenses/MIT>.
#

import argparse
import os
import pwd
import spear
import subprocess
import sys


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--build_config", default="Development")
    parser.add_argument("--unreal_engine_dir", required=True)
    args, unknown_args = parser.parse_known_args() # get remaining args to pass to RunUAT

    assert os.path.exists(args.unreal_engine_dir)
    assert args.build_config in ["Debug", "DebugGame", "Development", "Shipping", "Test"]

    if sys.platform == "win32":
        run_uat_script = os.path.realpath(os.path.join(args.unreal_engine_dir, "Engine", "Build", "BatchFiles", "RunUAT.bat"))
        target_platform = "Win64"
    elif sys.platform == "darwin":
        run_uat_script = os.path.realpath(os.path.join(args.unreal_engine_dir, "Engine", "Build", "BatchFiles", "RunUAT.sh"))
        target_platform = "Mac"
    elif sys.platform == "linux":
        run_uat_script = os.path.realpath(os.path.join(args.unreal_engine_dir, "Engine", "Build", "BatchFiles", "RunUAT.sh"))
        target_platform = "Linux"
    else:
        assert False

    # The Unreal Build Tool expects "~/.config" to be owned by the user, so it can create and write to "~/.config/Unreal Engine/"
    # without requiring admin privileges. This check might seem esoteric, but we have seen cases where "~/.config/"
    # is owned by root in some corporate environments, so we choose to check it here as a courtesy to new
    # users. We don't know if we need a similar check on Windows.
    if sys.platform in ["darwin", "linux"]:
        config_dir = os.path.expanduser(os.path.join("~", ".config"))
        if os.path.exists(config_dir):
            current_user = pwd.getpwuid(os.getuid()).pw_name
            config_dir_owner = pwd.getpwuid(os.stat(config_dir).st_uid).pw_name
            if current_user != config_dir_owner:
                spear.log(f"ERROR: The Unreal Build Tool expects {current_user} to be the owner of {config_dir}, but the current owner is {config_dir_owner}. To update, run the following command (-R indicates recursive):")
                spear.log(f"    sudo chown {current_user} {config_dir}")
                assert False

    project = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", "cpp", "unreal_projects", "SpearSim", "SpearSim.uproject"))
    archive_dir = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", "cpp", "unreal_projects", "SpearSim", "Standalone-" + args.build_config))

    cmd = [
        run_uat_script,
        "BuildCookRun",
        "-project=" + project,
        "-target=SpearSim",
        "-targetPlatform=" + target_platform,
        "-clientConfig=" + args.build_config,
        "-archivedirectory=" + archive_dir] + unknown_args # append remaining args to pass to RunUAT
    spear.log(f"Executing: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)

    spear.log("Done.")
