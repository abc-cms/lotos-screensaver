#!/usr/bin/env python

from xscreensaver_config.ConfigParser import ConfigParser

from lotos_screensaver.configuration import get_xscreensaver_config_file, update_screensaver_configuration


def main():
    try:
        print("Configuring XScreenSaver")
        # Read XScreenSaver configuration.
        configuration_file = ConfigParser(get_xscreensaver_config_file())
        configuration = configuration_file.read()
        # Update XScreenSaver configuration.
        configuration, configuration_changed = update_screensaver_configuration(configuration)
        # Save XScreensaver configuration.
        configuration_file.update(configuration)
        configuration_file.save()

    except RuntimeError:
        print("Invalid configuration file")


if __name__ == "__main__":
    main()
