"""
Generates version.json at the repo root from FIRMWARE_VERSION in platformio.ini.

Hooked into the build via platformio.ini's extra_scripts so version.json is
always in sync with the compiled firmware. The file is gitignored and meant
to be uploaded as a GitHub Release asset alongside firmware.bin — OTA reads
release_notes and critical from it.

release_notes and critical default to empty / false; edit the generated file
before uploading to a release if you want richer metadata.
"""

import json
import datetime
import os

Import("env")  # noqa: F821 -- provided by PlatformIO


def generate_version_json(source, target, env):
    version = env.GetProjectOption("build_flags", "")
    firmware_version = None
    for flag in env.get("BUILD_FLAGS", []):
        if "FIRMWARE_VERSION=" in flag:
            firmware_version = flag.split("FIRMWARE_VERSION=")[1].strip().strip('"').strip("\\").strip('"')
            break

    if not firmware_version:
        print("[gen_version_json] WARNING: FIRMWARE_VERSION not found in build flags")
        return

    project_dir = env["PROJECT_DIR"]
    output_path = os.path.join(project_dir, "version.json")

    payload = {
        "version": firmware_version,
        "min_version": "1.0.0",
        "release_date": datetime.date.today().isoformat(),
        "release_notes": "",
        "critical": False,
    }

    with open(output_path, "w") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")

    print(f"[gen_version_json] wrote {output_path} with version={firmware_version}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_version_json)  # noqa: F821
