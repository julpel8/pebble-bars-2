#!/usr/bin/env python3
"""Build Bars 2 and generate its deterministic emulator screenshot gallery."""

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import uuid


ROOT = Path(__file__).resolve().parent.parent
PRESETS_PATH = ROOT / "scripts" / "screenshot-presets.json"
SCREENSHOTS_DIR = ROOT / "screenshots"
PACKAGE_PATH = ROOT / "package.json"
PBW_PATH = ROOT / "build" / "pebble-bars-2.pbw"
MESSAGE_KEYS_PATH = ROOT / "build" / "js" / "message_keys.json"
SETTINGS_MODULE_PATH = ROOT / "src" / "pkjs" / "settings.js"

SERIES_COUNT = 11
EXPECTED_PRESET_COUNT = 15
EXPECTED_DIMENSIONS = {"emery": (200, 228)}
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
APP_UUID = uuid.UUID("b908a9cc-378d-46f4-b91a-8a2cc721feec")
SCREENSHOT_TIME_KEY = 20000
SCREENSHOT_STEPS_KEY = 20001

LANGUAGE_NAMES = (
    "English", "French", "German", "Spanish", "Italian", "Dutch",
    "Turkish", "Czech", "Portuguese", "Greek", "Swedish", "Polish",
    "Slovak", "Vietnamese", "Romanian", "Catalan", "Norwegian", "Russian",
    "Estonian", "Basque", "Finnish", "Danish", "Lithuanian", "Slovenian",
    "Hungarian", "Croatian", "Irish", "Latvian", "Serbian", "Chinese",
    "Indonesian", "Ukrainian", "Welsh", "Galician", "Japanese", "Korean",
    "Hebrew", "English (UK)",
)


class ScreenshotError(RuntimeError):
    """A user-facing failure while validating or generating screenshots."""


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Generate deterministic Bars 2 screenshots with Pebble."
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="list available presets without building or launching an emulator",
    )
    parser.add_argument(
        "--preset",
        action="append",
        default=[],
        metavar="NAME",
        help="generate one preset; repeat the option to select several",
    )
    return parser.parse_args()


def load_json(path):
    try:
        with path.open(encoding="utf-8") as source:
            return json.load(source)
    except (OSError, json.JSONDecodeError) as error:
        raise ScreenshotError("Unable to read {}: {}".format(path, error))


def parse_timestamp(value, preset_name):
    if not isinstance(value, str) or not value.endswith("Z"):
        raise ScreenshotError(
            "{}: time must be an ISO-8601 UTC value ending in Z".format(
                preset_name
            )
        )
    try:
        parsed = dt.datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise ScreenshotError(
            "{}: invalid time {!r}: {}".format(preset_name, value, error)
        )
    if parsed.utcoffset() != dt.timedelta(0):
        raise ScreenshotError("{}: time must be in UTC".format(preset_name))
    return parsed


def validate_series_settings(name, settings):
    order = settings.get("SETTING_SERIES_ORDER")
    if order is not None:
        if (
            not isinstance(order, list)
            or len(order) != SERIES_COUNT
            or sorted(order) != list(range(SERIES_COUNT))
        ):
            raise ScreenshotError(
                "{}: SETTING_SERIES_ORDER must be a permutation of 0..{}".format(
                    name, SERIES_COUNT - 1
                )
            )

    visible = settings.get("SETTING_SERIES_VISIBLE")
    if visible is not None:
        if (
            not isinstance(visible, list)
            or len(visible) != SERIES_COUNT
            or any(value not in (0, 1) for value in visible)
            or not any(visible)
        ):
            raise ScreenshotError(
                "{}: SETTING_SERIES_VISIBLE must contain {} zero/one values "
                "with at least one visible series".format(name, SERIES_COUNT)
            )


def validate_gallery_coverage(presets):
    if len(presets) != EXPECTED_PRESET_COUNT:
        raise ScreenshotError(
            "The gallery must contain exactly {} presets".format(
                EXPECTED_PRESET_COUNT
            )
        )

    styles = {preset["settings"]["SETTING_STYLE"] for preset in presets}
    if styles != set(range(8)):
        raise ScreenshotError(
            "The gallery must cover every style from 0 through 7"
        )

    placements = {
        preset["settings"].get("SETTING_TEXT_PLACEMENT", 2)
        for preset in presets
        if preset["settings"]["SETTING_STYLE"] < 4
    }
    if placements != set(range(7)):
        raise ScreenshotError(
            "The linear presets must cover every text placement from 0 through 6"
        )

    round_fills = {
        preset["settings"].get("SETTING_ROUND_POLAR_FILL", 0)
        for preset in presets
        if preset["settings"]["SETTING_STYLE"] in (6, 7)
    }
    if round_fills != set(range(4)):
        raise ScreenshotError(
            "The round-polar presets must cover every fill mode from 0 through 3"
        )

    visible_series = set()
    for preset in presets:
        visible = preset["settings"]["SETTING_SERIES_VISIBLE"]
        visible_series.update(
            index for index, shown in enumerate(visible) if shown
        )
    if visible_series != set(range(SERIES_COUNT)):
        raise ScreenshotError("The gallery must show all eleven series")

    required_values = {
        "SETTING_SEAMLESS_BARS": {0, 1},
        "SETTING_TEXT_OUTLINE": {0, 1},
        "SETTING_SMOOTH_PROGRESS": {0, 1},
        "SETTING_CLOCK_FORMAT": {1, 2},
        "SETTING_LEADING_ZERO": {0, 1},
        "SETTING_MERGE_HOUR_MINUTE": {0, 1},
        "SETTING_FULL_DATE_NAMES": {0, 1},
        "SETTING_WEEK_STARTS_SUNDAY": {0, 1},
    }
    defaults = {
        "SETTING_SEAMLESS_BARS": 1,
        "SETTING_TEXT_OUTLINE": 0,
        "SETTING_SMOOTH_PROGRESS": 1,
        "SETTING_CLOCK_FORMAT": 0,
        "SETTING_LEADING_ZERO": 1,
        "SETTING_MERGE_HOUR_MINUTE": 0,
        "SETTING_FULL_DATE_NAMES": 0,
        "SETTING_WEEK_STARTS_SUNDAY": 0,
    }
    for setting, required in required_values.items():
        values = {
            preset["settings"].get(setting, defaults[setting])
            for preset in presets
        }
        if not required.issubset(values):
            raise ScreenshotError(
                "The gallery must cover {} values {}".format(
                    setting, sorted(required)
                )
            )

    color_overrides = {
        setting
        for preset in presets
        for setting in preset["settings"]
        if setting.endswith("_COLOR")
    }
    if not color_overrides:
        raise ScreenshotError("The gallery must include a custom palette")


def load_and_validate_presets():
    manifest = load_json(PRESETS_PATH)
    if manifest.get("version") != 1 or not isinstance(
        manifest.get("presets"), list
    ):
        raise ScreenshotError(
            "{} must contain version 1 and a presets array".format(PRESETS_PATH)
        )

    package = load_json(PACKAGE_PATH)
    known_settings = set(package.get("pebble", {}).get("messageKeys", []))
    if not known_settings:
        raise ScreenshotError("package.json does not declare Pebble message keys")

    names = set()
    filenames = set()
    languages = set()
    presets = manifest["presets"]
    if not presets:
        raise ScreenshotError("The screenshot preset list is empty")

    for preset in presets:
        if not isinstance(preset, dict):
            raise ScreenshotError("Every preset must be a JSON object")

        name = preset.get("name")
        if not isinstance(name, str) or not NAME_PATTERN.fullmatch(name):
            raise ScreenshotError(
                "Preset names must use lowercase letters, digits and hyphens"
            )
        if name in names:
            raise ScreenshotError("Duplicate preset name: {}".format(name))
        names.add(name)

        filename = preset.get("filename")
        if (
            not isinstance(filename, str)
            or Path(filename).name != filename
            or not filename.endswith(".png")
        ):
            raise ScreenshotError(
                "{}: filename must be a plain .png filename".format(name)
            )
        if filename in filenames:
            raise ScreenshotError("Duplicate output filename: {}".format(filename))
        filenames.add(filename)

        platform = preset.get("platform")
        if platform not in EXPECTED_DIMENSIONS:
            raise ScreenshotError(
                "{}: unsupported screenshot platform {!r}".format(name, platform)
            )

        language = preset.get("language")
        if not isinstance(language, dict):
            raise ScreenshotError("{}: language must be an object".format(name))
        language_id = language.get("id")
        language_name = language.get("name")
        if (
            not isinstance(language_id, int)
            or not 0 <= language_id < len(LANGUAGE_NAMES)
            or language_name != LANGUAGE_NAMES[language_id]
        ):
            raise ScreenshotError(
                "{}: language id/name does not match the app language table".format(
                    name
                )
            )
        if language_id in languages:
            raise ScreenshotError(
                "{}: every screenshot must use a different language".format(name)
            )
        languages.add(language_id)

        parse_timestamp(preset.get("time"), name)

        battery = preset.get("battery_percent", 100)
        if not isinstance(battery, int) or not 0 <= battery <= 100:
            raise ScreenshotError(
                "{}: battery_percent must be between 0 and 100".format(name)
            )

        settings = preset.get("settings")
        if not isinstance(settings, dict):
            raise ScreenshotError("{}: settings must be an object".format(name))
        unknown = set(settings) - known_settings
        if unknown:
            raise ScreenshotError(
                "{}: unknown settings: {}".format(name, ", ".join(sorted(unknown)))
            )
        if "SETTING_LANGUAGE" in settings:
            raise ScreenshotError(
                "{}: declare the language in the language object".format(name)
            )
        style = settings.get("SETTING_STYLE")
        if not isinstance(style, int) or not 0 <= style <= 7:
            raise ScreenshotError(
                "{}: SETTING_STYLE must be between 0 and 7".format(name)
            )
        placement = settings.get("SETTING_TEXT_PLACEMENT")
        if placement is not None and (
            not isinstance(placement, int) or not 0 <= placement <= 6
        ):
            raise ScreenshotError(
                "{}: SETTING_TEXT_PLACEMENT must be between 0 and 6".format(
                    name
                )
            )
        round_fill = settings.get("SETTING_ROUND_POLAR_FILL")
        if round_fill is not None and (
            not isinstance(round_fill, int) or not 0 <= round_fill <= 3
        ):
            raise ScreenshotError(
                "{}: SETTING_ROUND_POLAR_FILL must be between 0 and 3".format(
                    name
                )
            )
        validate_series_settings(name, settings)
        if "SETTING_SERIES_VISIBLE" not in settings:
            raise ScreenshotError(
                "{}: every preset must declare SETTING_SERIES_VISIBLE".format(
                    name
                )
            )

    validate_gallery_coverage(presets)
    return presets


def select_presets(presets, requested_names):
    if not requested_names:
        return presets
    by_name = {preset["name"]: preset for preset in presets}
    missing = [name for name in requested_names if name not in by_name]
    if missing:
        raise ScreenshotError(
            "Unknown preset(s): {}. Use --list to see valid names.".format(
                ", ".join(missing)
            )
        )
    selected = []
    seen = set()
    for name in requested_names:
        if name not in seen:
            selected.append(by_name[name])
            seen.add(name)
    return selected


def print_presets(presets):
    print("{} screenshot presets:".format(len(presets)))
    for preset in presets:
        print(
            "  {:36} {:12} {}".format(
                preset["name"],
                preset["language"]["name"],
                preset["filename"],
            )
        )


def ensure_pebble_python():
    try:
        __import__("pebble_tool")
        return
    except ImportError:
        pass

    if os.environ.get("BARS_SCREENSHOT_REEXEC") == "1":
        raise ScreenshotError(
            "The Python environment used by pebble cannot import pebble_tool"
        )

    pebble = shutil.which("pebble")
    if not pebble:
        raise ScreenshotError("pebble is not available on PATH")
    try:
        first_line = Path(pebble).read_text(encoding="utf-8").splitlines()[0]
    except (OSError, IndexError) as error:
        raise ScreenshotError(
            "Unable to inspect the pebble launcher: {}".format(error)
        )
    if not first_line.startswith("#!"):
        raise ScreenshotError(
            "The pebble launcher has no Python shebang; install pebble-tool "
            "in the current Python environment"
        )

    command = shlex.split(first_line[2:].strip())
    if not command:
        raise ScreenshotError("The pebble launcher has an empty shebang")
    environment = os.environ.copy()
    environment["BARS_SCREENSHOT_REEXEC"] = "1"
    os.execve(
        command[0],
        command + [str(Path(__file__).resolve())] + sys.argv[1:],
        environment,
    )


def run_build(screenshot_build=False, announce=True):
    if announce:
        build_kind = "screenshot fixture" if screenshot_build else "production"
        print("Building Bars 2 ({})...".format(build_kind))
    environment = os.environ.copy()
    if screenshot_build:
        environment["BARS_SCREENSHOT_BUILD"] = "1"
    else:
        environment.pop("BARS_SCREENSHOT_BUILD", None)
    try:
        subprocess.run(
            ["pebble", "build"],
            cwd=str(ROOT),
            check=True,
            env=environment,
        )
    except FileNotFoundError:
        raise ScreenshotError("pebble is not available on PATH")
    except subprocess.CalledProcessError as error:
        raise ScreenshotError(
            "pebble build failed with exit code {}".format(error.returncode)
        )
    if not PBW_PATH.is_file() or not MESSAGE_KEYS_PATH.is_file():
        raise ScreenshotError("pebble build did not create the expected outputs")


def load_default_settings():
    program = (
        "const settings=require(process.argv[1]);"
        "process.stdout.write(JSON.stringify(settings.DEFAULTS));"
    )
    try:
        result = subprocess.run(
            ["node", "-e", program, str(SETTINGS_MODULE_PATH)],
            cwd=str(ROOT),
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        raise ScreenshotError("node is required to load the app defaults")
    except subprocess.CalledProcessError as error:
        raise ScreenshotError(
            "Unable to load app defaults: {}".format(error.stderr.strip())
        )
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ScreenshotError("App defaults are not valid JSON: {}".format(error))


def typed_message(settings, message_keys, timestamp):
    from libpebble2.services.appmessage import (
        ByteArray,
        CString,
        Int32,
        Uint32,
    )

    message = {}
    for setting_name, key in message_keys.items():
        if setting_name not in settings:
            raise ScreenshotError(
                "No default value is available for {}".format(setting_name)
            )
        value = settings[setting_name]
        if setting_name == "SETTING_SERIES_ORDER":
            message[key] = ByteArray(bytes(value))
        elif setting_name == "SETTING_SERIES_VISIBLE":
            mask = sum(
                (1 << index) for index, shown in enumerate(value) if shown
            )
            message[key] = Uint32(mask)
        elif setting_name == "SETTING_CUSTOM_LABEL":
            message[key] = CString(str(value))
        elif setting_name.endswith("_COLOR"):
            message[key] = Uint32(int(str(value).lstrip("#"), 16))
        else:
            message[key] = Int32(int(value))
    message[SCREENSHOT_TIME_KEY] = Uint32(int(timestamp.timestamp()))
    message[SCREENSHOT_STEPS_KEY] = Int32(0)
    return message


def send_settings(pebble, settings, message_keys, timestamp):
    from libpebble2.services.appmessage import AppMessageService

    service = AppMessageService(pebble)
    try:
        service.send_message(
            APP_UUID, typed_message(settings, message_keys, timestamp)
        )
    finally:
        service.shutdown()


def set_battery(pebble, percent):
    from libpebble2.communication.transports.qemu.protocol import QemuBattery
    from pebble_tool.commands.emucontrol import send_data_to_qemu

    send_data_to_qemu(
        pebble.transport, QemuBattery(percent=percent, charging=False)
    )


def capture_png(pebble, output_path):
    import png
    from pebble_tool.commands.screenshot import ScreenshotCommand

    processor = ScreenshotCommand()
    processor.pebble = pebble
    monitor_port = getattr(pebble.transport, "qemu_monitor_port", None)
    if not monitor_port:
        raise ScreenshotError("The emulator did not expose a QEMU monitor port")

    monitor_image = None
    for attempt in range(1, 4):
        try:
            frame_index = time.monotonic_ns()
            monitor_image = processor._grab_qemu_monitor_image_fast(
                monitor_port,
                str(output_path.parent),
                frame_index,
            )
            break
        except Exception as error:
            if attempt == 3:
                raise ScreenshotError(
                    "Unable to read {} from QEMU after three attempts: {}".format(
                        output_path.name, error
                    )
                )
            print("  QEMU framebuffer request failed; retrying...")
            time.sleep(0.75)

    # QEMU's monitor output includes the current backlight intensity. Recover
    # the four logical channel levels before applying pebble-tool's canonical
    # colour correction, so captures do not depend on a fading backlight.
    monitor_rgb = monitor_image.convert("RGB")
    maximum = max(channel[1] for channel in monitor_rgb.getextrema())
    if maximum <= 0:
        raise ScreenshotError("The QEMU framebuffer contains no lit pixels")

    logical_image = []
    for row_index in range(monitor_rgb.height):
        row = []
        for pixel in (
            monitor_rgb.getpixel((column, row_index))
            for column in range(monitor_rgb.width)
        ):
            for channel in pixel:
                level = int(round(channel * 3 / maximum))
                expected = level * maximum / 3
                if not 0 <= level <= 3 or abs(channel - expected) > 2:
                    raise ScreenshotError(
                        "Unexpected QEMU colour level {} (white level {})".format(
                            channel, maximum
                        )
                    )
                row.append(level * 85)
        logical_image.append(row)

    corrected_rgb = processor._correct_colours(logical_image)
    image = []
    for corrected_row in corrected_rgb:
        row = []
        for index in range(0, len(corrected_row), 3):
            row.extend(corrected_row[index : index + 3])
            row.append(255)
        image.append(row)
    png.from_array(image, mode="RGBA;8").save(str(output_path))

    logical_levels = (0, 85, 170, 255)
    corrected_palette = set()
    for red in logical_levels:
        for green in logical_levels:
            for blue in logical_levels:
                corrected = processor._correct_colours([[red, green, blue]])[0]
                corrected_palette.add(tuple(corrected))
    return corrected_palette


def validate_png(path, expected_dimensions, expected_palette):
    import png

    try:
        header = path.read_bytes()[:26]
    except OSError as error:
        raise ScreenshotError("Unable to read {}: {}".format(path, error))
    if (
        len(header) < 26
        or header[:8] != PNG_SIGNATURE
        or header[12:16] != b"IHDR"
    ):
        raise ScreenshotError("{} is not a valid PNG".format(path))
    width, height = struct.unpack(">II", header[16:24])
    if (width, height) != expected_dimensions:
        raise ScreenshotError(
            "{} is {}x{}, expected {}x{}".format(
                path,
                width,
                height,
                expected_dimensions[0],
                expected_dimensions[1],
            )
        )
    if header[24] != 8 or header[25] != 6:
        raise ScreenshotError("{} is not an 8-bit RGBA PNG".format(path))

    try:
        _, _, rows, metadata = png.Reader(filename=str(path)).read()
        if metadata.get("planes") != 4 or metadata.get("bitdepth") != 8:
            raise ScreenshotError("{} is not an 8-bit RGBA PNG".format(path))
        for row in rows:
            for index in range(0, len(row), 4):
                color = tuple(row[index : index + 3])
                alpha = row[index + 3]
                if color not in expected_palette or alpha != 255:
                    raise ScreenshotError(
                        "{} contains an uncorrected display colour".format(path)
                    )
    except ScreenshotError:
        raise
    except Exception as error:
        raise ScreenshotError(
            "Unable to validate PNG pixels in {}: {}".format(path, error)
        )


def capture_preset(pebble, preset, defaults, message_keys, output_path):
    settings = dict(defaults)
    settings.update(preset["settings"])
    settings["SETTING_LANGUAGE"] = preset["language"]["id"]
    settings["SETTING_ANIMATE"] = 0
    settings["SETTING_CLOCK_REFRESH"] = 60

    print(
        "Capturing {} ({})...".format(
            preset["name"], preset["language"]["name"]
        )
    )
    timestamp = parse_timestamp(preset["time"], preset["name"])
    set_battery(pebble, preset.get("battery_percent", 100))
    send_settings(pebble, settings, message_keys, timestamp)
    time.sleep(0.15)
    expected_palette = capture_png(pebble, output_path)
    validate_png(
        output_path,
        EXPECTED_DIMENSIONS[preset["platform"]],
        expected_palette,
    )


def commit_outputs(staged_outputs):
    backup_dir = staged_outputs[0][0].parent / "backups"
    backup_dir.mkdir()
    replaced = []
    try:
        for staged, destination in staged_outputs:
            if (
                destination.is_file()
                and destination.read_bytes() == staged.read_bytes()
            ):
                print("Unchanged: {}".format(destination.relative_to(ROOT)))
                continue
            backup = backup_dir / destination.name
            if destination.exists():
                shutil.copy2(str(destination), str(backup))
            os.replace(str(staged), str(destination))
            replaced.append((destination, backup if backup.exists() else None))
            print("Saved: {}".format(destination.relative_to(ROOT)))
    except Exception:
        for destination, backup in reversed(replaced):
            if backup is None:
                try:
                    destination.unlink()
                except FileNotFoundError:
                    pass
            else:
                os.replace(str(backup), str(destination))
        raise


def install_watchface(platform, emulator_was_running):
    # Going through the public CLI performs its normal SDK/toolchain setup
    # before the long-lived Python connection takes over.
    for attempt in range(1, 4):
        try:
            subprocess.run(
                [
                    "pebble",
                    "install",
                    "--emulator",
                    platform,
                    str(PBW_PATH),
                ],
                cwd=str(ROOT),
                check=True,
            )
            return
        except subprocess.CalledProcessError:
            if attempt == 3:
                break
            if not emulator_was_running:
                subprocess.run(
                    ["pebble", "kill"],
                    cwd=str(ROOT),
                    check=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
    raise ScreenshotError(
        "Unable to install Bars 2 on the {} emulator after three attempts".format(
            platform
        )
    )


def generate(selected):
    ensure_pebble_python()
    run_build(screenshot_build=True)
    try:
        generate_from_screenshot_build(selected)
    finally:
        print("Restoring the production build...")
        run_build(screenshot_build=False, announce=False)


def generate_from_screenshot_build(selected):
    defaults = load_default_settings()
    message_keys = load_json(MESSAGE_KEYS_PATH)

    from libpebble2.communication import PebbleConnection
    from pebble_tool.commands.screenshot import ScreenshotCommand
    from pebble_tool.sdk import sdk_manager
    from pebble_tool.sdk.emulator import ManagedEmulatorTransport

    platforms = {preset["platform"] for preset in selected}
    if len(platforms) != 1:
        raise ScreenshotError(
            "This gallery runner currently expects one selected platform"
        )
    platform = next(iter(platforms))
    sdk_version = sdk_manager.get_current_sdk()
    emulator_was_running = ManagedEmulatorTransport.is_emulator_alive(
        platform, sdk_version
    )
    install_watchface(platform, emulator_was_running)

    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)
    temporary_dir = Path(
        tempfile.mkdtemp(prefix=".bars2-captures-", dir=str(SCREENSHOTS_DIR))
    )
    pebble = None
    try:
        transport = ManagedEmulatorTransport(platform, sdk_version, False)
        pebble = PebbleConnection(transport)
        print("Connecting to the {} emulator...".format(platform))
        pebble.connect()
        pebble.run_async()
        time.sleep(0.5)

        staged_outputs = []
        for preset in selected:
            staged = temporary_dir / preset["filename"]
            capture_preset(pebble, preset, defaults, message_keys, staged)
            staged_outputs.append(
                (staged, SCREENSHOTS_DIR / preset["filename"])
            )
        commit_outputs(staged_outputs)
    finally:
        ScreenshotCommand._close_pebble_connection(pebble)
        if not emulator_was_running:
            ScreenshotCommand._shutdown_platform_emulator(
                platform, sdk_version
            )
        shutil.rmtree(str(temporary_dir), ignore_errors=True)


def main():
    try:
        args = parse_arguments()
        presets = load_and_validate_presets()
        selected = select_presets(presets, args.preset)
        if args.list:
            print_presets(selected)
            return 0
        generate(selected)
        print("Generated {} screenshot(s).".format(len(selected)))
        return 0
    except ScreenshotError as error:
        print("error: {}".format(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("error: interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
