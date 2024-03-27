from typing import Any, Dict, Optional

from xscreensaver_config.ConfigParser import ConfigParser

from .configuration import adjust_configuration, get_xscreensaver_config_file, read_configuration, \
    update_screensaver_configuration
from .manager import Manager


class ConfigurationManager(Manager):
    UPDATE_DURATION: float = 10  # Seconds

    __configuration: Optional[Dict[str, Any]]
    __has_external_changes: bool
    __has_internal_changes: bool

    def __init__(self, initial_timestamp: float):
        super().__init__(initial_timestamp)
        self.__configuration = None
        self.__has_external_changes = False
        self.__has_internal_changes = False
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

        configuration = adjust_configuration(read_configuration())
        self.__has_internal_changes = self.__configuration is None or self.__configuration != configuration
        self.__configuration = configuration

    @property
    def has_external_changes(self) -> bool:
        return self.__has_external_changes

    @property
    def has_internal_changes(self) -> bool:
        return self.__has_internal_changes
