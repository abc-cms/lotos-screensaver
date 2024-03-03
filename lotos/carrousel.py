import asyncio
import os
import signal
from asyncio import AbstractEventLoop
from functools import partial
from typing import Optional

from loguru import logger
from xscreensaver_config.ConfigParser import ConfigParser

from .configuration import adjust_configuration, get_log_file, get_xscreensaver_config_file, read_configuration, \
    update_screensaver_configuration, validate_configuration
from .slider import Slider


class Carrousel:
    __loop: Optional[AbstractEventLoop]
    __slider: Slider
    __restart_xscreensaver: bool

    def __init__(self):
        # Configure logger.
        self.__configure_logger()

        logger.info("Initialize Carrousel screensaver")

        # Real screensaver configuration, validate it and adjust to the valid representation.
        configuration = read_configuration()
        try:
            validate_configuration(configuration)
        except RuntimeError:
            logger.info("Invalid configuration file")
            raise
        configuration = adjust_configuration(configuration)

        # Assume that we don't need to restart XScreenSaver daemon initially.
        self.__restart_xscreensaver = False

        # Event loop is empty initially.
        self.__loop = None

        # Get a window id (xid) and create a slider using the one.
        xid = self.__get_xid()
        self.__slider = Slider(xid, configuration)

    @staticmethod
    def __get_xid() -> int:
        return int(os.environ["XSCREENSAVER_WINDOW"], 16)

    @staticmethod
    def __configure_logger():
        # Clean up and set only the one file handler.
        logger.remove()
        logger.add(get_log_file(), rotation="1 MB", retention="14 days")

    def __update_xscreensaver_configuration(self):
        # Read XScreenSaver configuration.
        configuration_file = ConfigParser(get_xscreensaver_config_file())
        configuration = configuration_file.read()
        # Update XScreenSaver configuration.
        configuration, configuration_changed = update_screensaver_configuration(configuration)

        # Save XScreensaver configuration if it has changes and set XScreenSaver restart flag.
        if configuration_changed:
            self.__restart_xscreensaver = True
            configuration_file.update(configuration)
            configuration_file.save()

    async def __shutdown(self):
        logger.info("Gracefully finish tasks")
        # Get list of pending tasks excepting the current one and cancel them.
        tasks = [task for task in asyncio.all_tasks() if task is not asyncio.current_task()]
        [task.cancel() for task in tasks]
        # Waiting tasks to be finished and stop the main event loop.
        await asyncio.gather(*tasks, return_exceptions=True)
        logger.info("Stop main loop")
        self.__loop.stop()

    async def __monitor_configuration(self):
        try:
            while True:
                logger.info("Check for configuration updates")

                # Read, validate and adjust screen saver configuration.
                configuration = read_configuration()
                try:
                    validate_configuration(configuration)
                except RuntimeError:
                    logger.error("Configuration file is invalid")
                configuration = adjust_configuration(configuration)

                # Update slider configuration.
                self.__slider.update_configuration(configuration)
                # Update XScreenSaver configuration.
                self.__update_xscreensaver_configuration()

                # Repeat in 15 seconds.
                await asyncio.sleep(15)
        finally:
            # No need in finalization, so just write to log.
            logger.info("Stop configuration monitor")

    def run(self):
        # Get event loop and install SIGTERM handler. It will shut down tasks gracefully.
        self.__loop = loop = asyncio.get_event_loop()
        logger.info("Install SIGTERM handler")
        loop.add_signal_handler(signal.SIGTERM, partial(asyncio.ensure_future, self.__shutdown()))

        try:
            # Create slider and configuration monitor tasks.
            # Slider task shows slides and monitor task looking for configuration file updates and applies them.
            logger.info("Run Slider and monitor tasks")
            loop.create_task(self.__slider.run())
            loop.create_task(self.__monitor_configuration())
            loop.run_forever()

        finally:
            # Shut down async generators. We don't have them, but this is a good practice in any case.
            logger.info("Close event loop")
            loop.run_until_complete(loop.shutdown_asyncgens())
            # Close event loop.
            loop.close()
            # Check if XScreenSaver daemon should be restarted. It should be done in cases when some daemon
            # parameters has been changed.
            if self.__restart_xscreensaver:
                logger.info("Restart XScreenSaver to refresh its configuration")
                os.system("xscreensaver-command --restart")
