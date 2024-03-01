from copy import deepcopy
from datetime import datetime
from typing import Any, Dict

import yaml
from pathvalidate import Platform, is_valid_filepath
from schema import And, Optional, Regex, Schema, SchemaError

from .utils import expand_path

__LOG_FILE: str = "carrousel.log"

__SAVER_CONFIGURATION_FILE: str = "config.yaml"

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


def get_configuration_file() -> str:
    return expand_path(__SAVER_CONFIGURATION_FILE)


def validate_configuration(configuration: Dict[str, Any]):
    try:
        __CONFIGURATION_SCHEMA.validate(configuration)
    except SchemaError as se:
        raise se


def read_configuration() -> Dict[str, Any]:
    with open(get_configuration_file()) as file:
        return yaml.safe_load(file)


def adjust_configuration(configuration: Dict[str, Any]) -> Dict[str, Any]:
    configuration = deepcopy(configuration)

    for media in configuration["media_files"]:
        media["path"] = expand_path(media["path"])

    configuration["screensaver_settings"]["start_time"] = datetime.strptime(
        configuration["screensaver_settings"]["start_time"], "%H:%M").time()
    configuration["screensaver_settings"]["end_time"] = datetime.strptime(
        configuration["screensaver_settings"]["end_time"], "%H:%M").time()

    return configuration