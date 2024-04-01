from copy import deepcopy
from datetime import datetime, time, timedelta
from operator import itemgetter
from typing import Any, Dict, Tuple, Union

import json
from pathvalidate import Platform, is_valid_filepath
from schema import And, Optional, Regex, Schema, SchemaError

from .utils import expand_path

__LOG_FILE: str = "lotos.log"

__ENTRY_FILE: str = "lotos_saver.py"

__BLACK_SCREEN_FILE: str = "media/internal/black.png"

__XSCREENSAVER_CONFIGURATION_FILE: str = "~/.xscreensaver"

__SAVER_CONFIGURATION_FILE: str = "config.json"

__CONFIGURATION_SCHEMA: Schema = Schema({
    "media_files": [
        {
            "type": And(str, lambda value: value in ("image", "video")),
            "path": And(str, lambda value: is_valid_filepath(value, platform=Platform.LINUX)),
            Optional("time"): And(int, lambda value: value > 1),
        }
    ],
    "screensaver_settings": {
        "start_time": Regex("^([0-9]|0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]$"),
        "end_time": Regex("^([0-9]|0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]$"),
        "inactivity_timeout": And(int, lambda value: value > 0),
    },
})


def get_log_file() -> str:
    return expand_path(__LOG_FILE)


def get_entry_file() -> str:
    return expand_path(__ENTRY_FILE)


def get_xscreensaver_config_file() -> str:
    return expand_path(__XSCREENSAVER_CONFIGURATION_FILE)


def get_black_screen_file() -> str:
    return expand_path(__BLACK_SCREEN_FILE)


def get_configuration_file() -> str:
    return expand_path(__SAVER_CONFIGURATION_FILE)


def validate_configuration(configuration: Dict[str, Any]):
    try:
        __CONFIGURATION_SCHEMA.validate(configuration)
    except SchemaError as error:
        raise RuntimeError("Invalid configuration") from error


def read_configuration() -> Dict[str, Any]:
    with open(get_configuration_file()) as file:
        return json.load(file)


def adjust_configuration(configuration: Dict[str, Any]) -> Dict[str, Any]:
    configuration = deepcopy(configuration)

    # Expand path to real representations.
    for media in configuration["media_files"]:
        media["path"] = expand_path(media["path"])

    # Convert strings to time objects.
    configuration["screensaver_settings"]["start_time"] = datetime.strptime(
        configuration["screensaver_settings"]["start_time"], "%H:%M").time()
    configuration["screensaver_settings"]["end_time"] = datetime.strptime(
        configuration["screensaver_settings"]["end_time"], "%H:%M").time()

    return configuration


def update_screensaver_configuration(xscreensaver_configuration: Dict[str, Any], command: Union[str, None] = None) -> \
        Tuple[Dict[str, str], bool]:
    configuration_changed = False
    # Fetch and validate internal and XScreenSaver configurations.
    xscreensaver_configuration = deepcopy(xscreensaver_configuration)
    internal_configuration = read_configuration()
    validate_configuration(internal_configuration)

    # Check and update timeout parameter.
    internal_timeout = int(internal_configuration["screensaver_settings"]["inactivity_timeout"])
    internal_timeout = (datetime.combine(datetime.today(), time(hour=0, minute=0, second=0)) + timedelta(
        seconds=internal_timeout)).time()
    xscreensaver_timeout = datetime.strptime(xscreensaver_configuration["timeout"], "%H:%M:%S").time()
    if internal_timeout != xscreensaver_timeout:
        xscreensaver_configuration["timeout"] = internal_timeout.strftime("%H:%M:%S")
        configuration_changed = True

    # Check and update lock parameter. The screen must never been locked.
    if xscreensaver_configuration["lock"] == "True":
        xscreensaver_configuration["lock"] = "False"
        configuration_changed = True

    # Check and update captureStderr parameter. Hide all stdout/stderr output.
    if xscreensaver_configuration["captureStderr"] == "True":
        xscreensaver_configuration["captureStderr"] = "False"
        configuration_changed = True

    # Check and update mode parameter. Use only one (ours) screensaver.
    if xscreensaver_configuration["mode"] != "one":
        xscreensaver_configuration["mode"] = "one"
        configuration_changed = True

    # Look for our screensaver entry and add new if not found.
    default_program = {
        "enabled": "True",
        "renderer": "",
        "command": get_entry_file(),
    }

    entry_file = get_entry_file()
    try:
        screensaver_index = list(map(itemgetter("command"),
                                     xscreensaver_configuration["programs"])).index(entry_file)
    except ValueError:
        screensaver_index = -1

    if screensaver_index == -1:
        xscreensaver_configuration["programs"].append(default_program)
        screensaver_index = xscreensaver_configuration["programs"].index(command or default_program)
        configuration_changed = True

    # Check and update index of a screensaver.
    if int(xscreensaver_configuration["selected"]) != screensaver_index:
        xscreensaver_configuration["selected"] = str(screensaver_index)
        configuration_changed = True

    return xscreensaver_configuration, configuration_changed