# Pre-build hook: ensure esptool's runtime dependency `intelhex` is installed
# in the active PlatformIO Python environment (the Homebrew core or the VS Code
# extension's penv -- whichever is running this build).
#
# The Seeed platform's prepackaged esptoolpy omits this transitive dependency,
# which otherwise breaks the build at bootloader.bin with:
#   ModuleNotFoundError: No module named 'intelhex'
Import("env")

import subprocess

python = env.subst("$PYTHONEXE")


def _has_module(name):
    return subprocess.run(
        [python, "-c", "import %s" % name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode == 0


if _has_module("intelhex"):
    print("[ensure_deps] intelhex present")
else:
    print("[ensure_deps] intelhex missing -> installing into the PlatformIO Python env")
    subprocess.run([python, "-m", "pip", "install", "intelhex"], check=True)
