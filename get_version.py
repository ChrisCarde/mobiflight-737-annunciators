Import("env")
import os
import re

# Get the version number from the build environment.
firmware_version = os.environ.get('VERSION', "")

# Clean up the version number
if firmware_version == "":
  # When no version is specified default to "0.0.1" for
  # compatibility with MobiFlight desktop app version checks.
  firmware_version = "0.0.1"

# Strip any leading "v" that might be on the version and
# any leading or trailing periods.
firmware_version = firmware_version.lstrip("v")
firmware_version = firmware_version.strip(".")

# The core version is reported to the Connector as the 5th kInfo argument, and the Connector
# feeds it straight into .NET's System.Version to gate features -- adding a custom device,
# setting the board name, and even how outputs are addressed on the wire
# (MobiFlightModule.HasFirmwareFeature). A non-numeric value there makes System.Version throw.
#
# Upstream the template reuses custom_core_firmware_version for this, which works because
# that option is a release tag like "3.1.4". This project clones a fork BRANCH instead
# ("ESP32_support"), so the git ref and the reported version have to be separate options:
# custom_core_firmware_base_version is the upstream release the branch is built on.
core_ref = env.GetProjectOption("custom_core_firmware_version", "")
core_firmware_version = env.GetProjectOption("custom_core_firmware_base_version", core_ref)

if not core_firmware_version:
  print("ERROR!! custom_core_firmware_version must be defined!!")
  env.Exit(1)

if not re.fullmatch(r"\d+(\.\d+){1,3}", core_firmware_version):
  print(f"ERROR!! Core version {core_firmware_version!r} is not a dotted numeric version.")
  print("        The Connector parses it with System.Version and throws on anything else.")
  print("        If custom_core_firmware_version is a branch, set")
  print("        custom_core_firmware_base_version to the upstream release it is based on.")
  env.Exit(1)

print(f'Using version {firmware_version} for the build')
print(f'Using version {core_firmware_version} as core version')

# Append the version to the build defines so it gets baked into the firmware
env.Append(CPPDEFINES=[
  f'BUILD_VERSION={firmware_version}'
])
env.Append(CPPDEFINES=[
  f'CORE_BUILD_VERSION={core_firmware_version}'
])

# Set the output filename to the name of the board and the version
env.Replace(PROGNAME=f'{env["PIOENV"]}_{firmware_version.replace(".", "_")}')
