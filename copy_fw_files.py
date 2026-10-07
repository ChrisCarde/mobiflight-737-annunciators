Import("env")
import os, json, zipfile, shutil, subprocess
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
    # whose env has not been built yet simply has no binary in the zip, which
    # check_definitions.py reports as an error.
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

    if platform == "espressif32":
        write_full_image(env)
        write_web_installer(env)
    publish_extras()


def write_full_image(env):
    """A single image a person can flash without building anything.

    The .bin in the Connector package is only the application, which is all the Connector
    would want and is useless on its own: a blank board also needs the bootloader, the
    partition table and boot_app0 at their own offsets. This merges all four into one file
    that goes at 0x0 -- what a browser flasher or a one-line esptool command expects.

    The layout is taken from PlatformIO rather than written down here, because it differs
    by chip: a classic ESP32's bootloader goes at 0x1000, an ESP32-S3's at 0x0. Flash mode,
    speed and size are kept exactly as built, for the same reason."""
    app = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    if not app.exists():
        return
    chip = env.BoardConfig().get("build.mcu", "esp32")
    out = Path("./_dist") / (env.subst("${PROGNAME}") + "_full.bin")
    for old in Path("./_dist").glob(env.subst("$PIOENV") + "_*_full.bin"):
        if old.name != out.name:
            old.unlink()

    cmd = [env.subst("$PYTHONEXE"), "-m", "esptool", "--chip", chip, "merge-bin",
           "-o", str(out), "--flash-mode", "keep", "--flash-freq", "keep",
           "--flash-size", "keep"]
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        cmd += [env.subst(offset), env.subst(image)]
    cmd += [env.subst("$ESP32_APP_OFFSET") or "0x10000", str(app)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("WARNING: could not write the full flash image:\n" + result.stderr.strip())
        return
    print("Full flash image (write at 0x0): " + str(out))


# ESP Web Tools names a chip the way esptool-js reports it.
CHIP_FAMILY = {"esp32": "ESP32", "esp32s2": "ESP32-S2", "esp32s3": "ESP32-S3",
               "esp32c3": "ESP32-C3", "esp32c6": "ESP32-C6", "esp32h2": "ESP32-H2"}


def write_web_installer(env):
    """The browser installer: _site/, which the release publishes to GitHub Pages.

    ESP Web Tools is given the four parts at their own offsets rather than the full image,
    and that is the point of it. The full image runs from 0x0 to the end of the application,
    so it also covers the NVS partition -- padded with 0xFF -- and every flash of it erases
    the board's MobiFlight configuration and its touch calibration. Written as parts, NVS is
    left alone and an update keeps both; the installer asks about erasing instead.

    Like the zip, the manifest lists every board built at this version, not just this env:
    one Install button serves both boards and picks the build by the chip it finds."""
    app = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    if not app.exists():
        return
    site = Path("./_site")
    pioenv = env.subst("$PIOENV")
    board_dir = site / "firmware" / pioenv
    if board_dir.exists():
        shutil.rmtree(board_dir)
    board_dir.mkdir(parents=True)

    images = [(env.subst(offset), env.subst(image))
              for offset, image in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET") or "0x10000", str(app)))
    parts = []
    for offset, image in images:
        shutil.copy(image, board_dir / Path(image).name)
        parts.append({"path": "firmware/%s/%s" % (pioenv, Path(image).name),
                      "offset": int(offset, 0)})
    mcu = env.BoardConfig().get("build.mcu", "esp32")
    (board_dir / "build.json").write_text(json.dumps(
        {"version": firmware_version, "chipFamily": CHIP_FAMILY.get(mcu, mcu.upper()),
         "parts": parts}, indent=2) + "\n")

    # Every board's parts at this version. One left over from an earlier VERSION is removed
    # rather than listed: it would install as current.
    builds = []
    for build_json in sorted(site.glob("firmware/*/build.json")):
        build = json.loads(build_json.read_text())
        if build.get("version") != firmware_version:
            shutil.rmtree(build_json.parent)
            continue
        builds.append({"chipFamily": build["chipFamily"], "parts": build["parts"]})
    manifest = {
        "name": "737 Annunciator Panels",
        "version": firmware_version,
        # Without this the installer erases the whole flash on every install, because the
        # firmware does not speak Improv -- so every update would lose the board's setup.
        "new_install_prompt_erase": True,
        # Improv is ESPHome's Wi-Fi provisioning protocol. Probing for it would send its
        # packets to the MobiFlight command parser and wait ten seconds for an answer.
        "new_install_improv_wait_time": 0,
        "builds": builds,
    }
    (site / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copy(Path("web/index.html"), site / "index.html")
    print("Web installer: _site/ (%s)" % ", ".join(
        "%s at %s" % (b["chipFamily"], " ".join(hex(p["offset"]) for p in b["parts"]))
        for b in builds))


def publish_extras():
    """Files a person downloads alongside the package: the Windows launcher for the 3.2in
    board, and the licences. Copied into _dist so that everything a release needs is in one
    place.

    The licences are not optional: the MIT and BSD components ask for their notices to
    travel with a binary, and the LGPL for its text. THIRD-PARTY-NOTICES.md has all of it.
    A licence text that is no longer in licenses/ is removed from _dist rather than left to
    look current -- a release must not claim a licence its binaries no longer carry."""
    dist = Path("./_dist")
    for extra in Path("windows").glob("*.bat"):
        shutil.copy(extra, dist / extra.name)
    for doc in ("LICENSE", "THIRD-PARTY-NOTICES.md"):
        if Path(doc).exists():
            shutil.copy(doc, dist / doc)
    current = {text.name for text in Path("licenses").glob("*.txt")}
    for stale in dist.glob("*.txt"):
        if stale.name not in current:
            stale.unlink()
    for text in Path("licenses").glob("*.txt"):
        shutil.copy(text, dist / text.name)

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
