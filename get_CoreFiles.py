import os
import subprocess

Import("env")

# The MobiFlight core firmware is not vendored into this repo -- it is cloned here at
# build time and compiled alongside the sources in Annunciator/.
#
# Upstream MobiFlight/MobiFlight-FirmwareSource has NO ESP32 support at any released
# version (its envs are AVR and RP2040 only), so we pin to elral's fork instead, which
# carries the ESP32 work on the "ESP32_support" branch. Both the repo and the ref are
# read from the env so they stay visible in Annunciator_platformio.ini.
CORESOURCE = env.GetProjectOption(
    "custom_core_firmware_source",
    "https://github.com/MobiFlight/MobiFlight-FirmwareSource",
)
CORESOURCE_DIR = env.subst("$PROJECT_DIR/src")
CORESOURCE_TAG = env.GetProjectOption("custom_core_firmware_version")

# Optional, and strongly recommended: the exact core commit to build against.
#
# ESP32_support is a branch, not a tag, so without this every build pulls whatever landed
# upstream that day -- and a core change then looks exactly like a bug in our own. With it
# the build is reproducible, and it also skips the network entirely once ./src is already on
# that commit. Clear the key to follow the branch head again.
CORESOURCE_COMMIT = env.GetProjectOption("custom_core_firmware_commit", "")

print("Compiling for Core Version: " + CORESOURCE_TAG + " from " + CORESOURCE
      + (" @ " + CORESOURCE_COMMIT[:9] if CORESOURCE_COMMIT else ""))


def git(*args):
    """Run git against the cloned core. Returns the CompletedProcess; never raises."""
    return subprocess.run(
        ["git", f"--work-tree={CORESOURCE_DIR}", f"--git-dir={CORESOURCE_DIR}/.git", *args],
        capture_output=True, text=True)


def head():
    return git("rev-parse", "HEAD").stdout.strip()


def checkout_pinned():
    """Put ./src on CORESOURCE_COMMIT, fetching it first if the shallow clone lacks it."""
    if git("cat-file", "-e", CORESOURCE_COMMIT + "^{commit}").returncode != 0:
        # GitHub serves individual reachable commits; deepening the branch is the fallback
        # for servers that refuse an unadvertised one.
        git("fetch", "--depth", "1", "origin", CORESOURCE_COMMIT)
        if git("cat-file", "-e", CORESOURCE_COMMIT + "^{commit}").returncode != 0:
            git("fetch", "--depth", "100", "origin", CORESOURCE_TAG)
    result = git("checkout", "--detach", CORESOURCE_COMMIT)
    if result.returncode != 0:
        print(f"ERROR: cannot check out the pinned core commit {CORESOURCE_COMMIT}.\n"
              f"{result.stderr.strip()}\n"
              f"Delete ./src to re-clone, or update custom_core_firmware_commit in "
              f"Annunciator_platformio.ini.")
        env.Exit(1)


if not os.path.exists(CORESOURCE_DIR):
    print("Cloning Mobiflight-Firmware repo ... ")
    env.Execute(f'git clone --depth 1 --filter=blob:none --sparse --branch {CORESOURCE_TAG} "{CORESOURCE}" "{CORESOURCE_DIR}"')
    env.Execute(f'git --work-tree="{CORESOURCE_DIR}" --git-dir="{CORESOURCE_DIR}/.git" sparse-checkout set _Boards src')
    if CORESOURCE_COMMIT:
        checkout_pinned()
elif CORESOURCE_COMMIT and head() == CORESOURCE_COMMIT:
    # Already where we want to be. Drop our patches (they go back on below) and stay offline.
    print(f"Core pinned at {CORESOURCE_COMMIT[:9]}, already checked out -- skipping fetch.")
    git("checkout", "--", "src")
else:
    print("Checking for Mobiflight-Firmware repo updates ... ")
    # Drop our patches first (see below) so the pull never trips over them; they go back on
    # straight after.
    git("checkout", "--", "src")
    if CORESOURCE_COMMIT:
        checkout_pinned()
    else:
        env.Execute(f'git --work-tree="{CORESOURCE_DIR}" --git-dir="{CORESOURCE_DIR}/.git" pull origin {CORESOURCE_TAG} --depth 100')

if os.path.isfile(CORESOURCE_DIR + "/platformio.ini"):
    os.remove(CORESOURCE_DIR + "/platformio.ini")

# Local fixes to the core, applied after every clone or pull.
#
# The Connector sends a custom device message with no value at all for Stop (-1) -- on every
# board detection, every Stop and every AutoRun restart -- and an empty one for an empty
# string output. CmdMessenger's readStringArg() returns NULL for both, and the core hands that
# straight to unescape(). On an AVR a read of address 0 is harmless; on the ESP32 it panics
# and the board reboots, losing whatever values the Connector sent while it was down (the
# Connector counts them as delivered and does not send them again).
#
# Each patch is (file, text the core ships, replacement). If neither is found the core has
# changed under us, so the build stops rather than silently shipping the crash.
CORE_PATCHES = [
    ("src/MF_CustomDevice/CustomDevice.cpp",
     "        cmdMessenger.unescape(output);                    // and unescape the string if escape characters are used",
     "        if (output) cmdMessenger.unescape(output);        // NULL for a message with no value (Stop) -- annunciator patch"),
    ("src/MF_LCDDisplay/LCDDisplay.cpp",
     "        cmdMessenger.unescape(output);",
     "        if (output) cmdMessenger.unescape(output); // annunciator patch"),
]

for rel, original, patched in CORE_PATCHES:
    path = os.path.join(CORESOURCE_DIR, rel)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if patched in text:
        continue
    if text.count(original) != 1:
        print(f"ERROR: cannot apply the annunciator patch to {rel}: the core has changed. "
              f"Check whether it still needs one and update CORE_PATCHES in get_CoreFiles.py.")
        env.Exit(1)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(original, patched))
    print(f"Patched core: {rel}")
