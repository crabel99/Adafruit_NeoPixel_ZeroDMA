"""Run the library checks with the pinned SimIO Framework."""
import argparse
import subprocess
import sys
from pathlib import Path


FRAMEWORK_SHA = "456a23a731616784a5262102b10869616e9b0598"
ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd=ROOT):
    subprocess.run(command, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--framework", required=True, type=Path)
    parser.add_argument("--pio", default="pio")
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--doxygen", default="doxygen")
    args = parser.parse_args()
    framework = args.framework.resolve()
    actual = subprocess.check_output(
        ["git", "-C", str(framework), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual != FRAMEWORK_SHA:
        raise SystemExit(f"SimIOFramework must be {FRAMEWORK_SHA}, got {actual}")

    build = ROOT / "build" / "ci"
    build.mkdir(parents=True, exist_ok=True)
    config = build / "platformio.ini"
    config.write_text(
        "[platformio]\n"
        "default_envs = metro_m0, metro_m4\n"
        "src_dir = ../../examples/strandtest\n"
        "\n[env]\n"
        "platform = atmelsam@8.3.0\n"
        "framework = arduino\n"
        "platform_packages =\n"
        f"  framework-arduino-samd-adafruit@symlink://{framework}\n"
        "lib_deps =\n"
        f"  symlink://{ROOT}\n"
        f"  symlink://{framework / 'libraries' / 'Adafruit_ZeroDMA'}\n"
        "  adafruit/Adafruit NeoPixel@1.15.5\n"
        "build_flags =\n"
        "  -DARDUINO_SAMD_ADAFRUIT\n"
        "  -DUSE_ZERODMA\n"
        "  -DUSE_TINYUSB\n"
        f"  -I{framework / 'libraries' / 'Adafruit_ZeroDMA'}\n"
        "\n[env:metro_m0]\n"
        "board = adafruit_metro_m0\n"
        "\n[env:metro_m4]\n"
        "board = adafruit_metro_m4\n"
    )
    run([sys.executable, "-m", "unittest", "discover", "-s", "extras"])
    for style in ("legacy", "native"):
        run(
            [
                sys.executable,
                "extras/native/run.py",
                "--register-style",
                style,
                "--report",
                f"ci-native-{style}.json",
            ]
        )
    run([args.pio, "run", "-d", str(build), "-e", "metro_m0"])
    run([args.pio, "run", "-d", str(build), "-e", "metro_m4"])
    sources = subprocess.check_output(
        [
            "git",
            "ls-files",
            "*.[ch]",
            "*.cc",
            "*.cpp",
            "*.cxx",
            "*.hpp",
        ],
        cwd=ROOT,
        text=True,
    ).splitlines()
    sources = [name for name in sources if name != "pins.inc" and (ROOT / name).is_file()]
    run([args.clang_format, "--dry-run", "--Werror", *sources])
    run([args.doxygen, "Doxyfile"])


if __name__ == "__main__":
    main()
