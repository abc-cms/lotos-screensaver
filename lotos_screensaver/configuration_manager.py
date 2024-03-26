from typing import Any, Dict

from xscreensaver_config.ConfigParser import ConfigParser

from .configuration import get_xscreensaver_config_file, update_screensaver_configuration
from .manager import Manager


class ConfigurationManager(Manager):
    UPDATE_DURATION: float = 10  # Seconds

    __configuration: Dict[str, Any]
    __has_external_changes: bool

    def __init__(self, initial_timestamp: float):
        super().__init__(initial_timestamp)
        self.__has_external_changes = False
        self.update()

    def is_update_required(self, timestamp: float) -> bool:
        return timestamp >= self.initial_timestamp + self.duration(timestamp)

    def duration(self, timestamp: float) -> float:
        return self.UPDATE_DURATION

    @property
    def configuration(self) -> Dict[str, Any]:
        return self.__configuration

    def update(self):
        # Read XScreenSaver configuration.
        configuration_file = ConfigParser(get_xscreensaver_config_file())
        configuration = configuration_file.read()
        # Update XScreenSaver configuration.
        configuration, configuration_changed = update_screensaver_configuration(configuration)
        # Save XScreensaver configuration if it has changes and set XScreenSaver restart flag.
        if configuration_changed:
            self.__has_external_changes = True
            configuration_file.update(configuration)
            configuration_file.save()

    @property
    def has_external_changes(self) -> bool:
        return self.__has_external_changes
