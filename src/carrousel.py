import asyncio
import os
import sys

from loguru import logger

from .configuration import adjust_configuration, get_log_file, read_configuration, validate_configuration
from .slider import Slider


def configure_logger():
    # Configure logger.
    logger.add(get_log_file(), rotation="1 MB", retention="14 days")


def update_xscreensaver_configuration():
    ...


def main():
    null = open(os.devnull, "w")
    sys.stdout = null
    sys.stderr = null

    configure_logger()

    logger.info("Carousel screensaver was started")

    configuration = read_configuration()
    validate_configuration(configuration)
    configuration = adjust_configuration(configuration)

    update_xscreensaver_configuration()

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    slider = Slider(int(os.environ["XSCREENSAVER_WINDOW"], 16), configuration)
    try:
        loop.run_until_complete(asyncio.gather(slider.start()))
    finally:
        loop.run_until_complete(loop.shutdown_asyncgens())
        loop.close()