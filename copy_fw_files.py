Import("env")
import os, zipfile, shutil
from pathlib import Path

# Get the version number from the build environment.
firmware_version = os.environ.get('VERSION', "")
if firmware_version == "":
    firmware_version = "0.0.1"
firmware_version = firmware_version.lstrip("v")
firmware_version = firmware_version.strip(".")

# Get the ZIP filename from the build environment.
community_project = env.GetProjectOption('custom_community_project', "")

# Get the custom folder from the build environment.
custom_source_folder = env.GetProjectOption('custom_source_folder', "")

# Get the foldername inside the zip file from the build environment.
zip_filename = env.GetProjectOption('custom_zip_filename', "")

platform = env.BoardConfig().get("platform", {})

def copy_fw_files(source, target, env):
    """Rebuild the Connector package from the current Community folder.

    Copied fresh each time: an earlier version copied Community only when _build did not
    exist yet, and kept zipping that first copy -- definitions added later never reached the
    zip. Runs on every `pio run` and before every upload (see the end of this file), whether
    or not the firmware had to be re-linked."""
    build_community = Path("./_build") / custom_source_folder / "Community"
    if build_community.exists():
        shutil.rmtree(build_community)
    shutil.copytree(Path(custom_source_folder) / "Community", build_community)
    (build_community / "firmware").mkdir(parents=True, exist_ok=True)

    # set FW version within board.json files
    replacements = {
        "0.0.1": firmware_version
    }
    for file_path in (build_community / "boards").rglob("*.json"):
        replace_in_file(file_path, replacements)

    # Every board's firmware, not just this env's.
    #
    # One package serves both boards -- they share all nine device definitions, and the
    # Connector picks a board by its hardware id -- so the zip has to carry a binary per
    # board. This folder is rebuilt from scratch above, so take whatever the other envs
    # have already produced in .pio/build rather than relying on what survived. A board
    # whose env has not been built yet simply has no binary in the zip, which is what
    # check_definitions.py reports.
    suffix = ("uf2" if platform == "raspberrypi" else "bin")
    version_tag = firmware_version.replace(".", "_")
    built = sorted(Path(".pio/build").glob(f"*/*_{version_tag}.{suffix}"))
    for fw_file in built:
        shutil.copy(fw_file, build_community / "firmware")
    print("Firmware in the package: " + (", ".join(f.name for f in built) or "none"))

    zip_file_path = './_dist/' + zip_filename + '_' + firmware_version + '.zip'
    # A package from an earlier VERSION would sit beside this one looking current.
    for old in Path("./_dist").glob(zip_filename + "_*.zip") if os.path.exists("./_dist") else []:
        if old.name != Path(zip_file_path).name:
            old.unlink()
    print("Creating zip file " + zip_file_path)
    createZIP(str(build_community), zip_file_path, community_project)

def createZIP(original_folder_path, zip_file_path, new_folder_name):
    if os.path.exists("./_dist") == False:
        os.mkdir("./_dist")
    with zipfile.ZipFile(zip_file_path, 'w') as zipf:
        for root, dirs, files in os.walk(original_folder_path):
            for file in files:
                # Create a new path in the ZIP file
                new_path = os.path.join(new_folder_name, os.path.relpath(os.path.join(root, file), original_folder_path))
                # Add the file to the ZIP file
                zipf.write(os.path.join(root, file), new_path)

def replace_in_file(file_path, replacements):
    """Replace all keys in `replacements` with their values in the given file."""
    with open(file_path, "r", encoding="utf-8") as file:
        content = file.read()

    for old, new in replacements.items():
        content = content.replace(old, new)

    with open(file_path, "w", encoding="utf-8") as file:
        file.write(content)

# Packaging is an alias of its own, built every time (AlwaysBuild), part of the default build,
# and a dependency of upload. A post-action on the .bin -- or on the "buildprog" alias, which
# SCons treats as up to date whenever the .bin is -- runs only when the firmware re-links, and
# `pio run -t upload` never builds "buildprog" at all; either way a changed definition would
# never reach the zip. `pio run -t annunciator_package` packages on its own.
package = env.Alias("annunciator_package", "$BUILD_DIR/${PROGNAME}.bin", copy_fw_files)
env.AlwaysBuild(package)
env.Default(package)
env.Depends("upload", package)
