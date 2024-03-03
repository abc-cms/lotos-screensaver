import os
from copy import deepcopy
from datetime import datetime, time
from typing import Any, Dict, Optional

from loguru import logger

from .player import Player


class Slider:
    __configuration: Dict[str, Any]
    __player: Optional[Player]

    def __init__(self, window_id: int, configuration: Dict[str, Any]):
        logger.info(f"Initialize Slider using xid={window_id}")
        # Initialize configuration and create player.
        self.__configuration = deepcopy(configuration)
        self.__player = Player(window_id)

    def terminate(self):
        logger.info("Terminate slider")
        if self.__player is not None:
            # Terminate and clean up player.
            self.__player.terminate()
            self.__player = None
            logger.info("Slider terminated")

    def update_configuration(self, configuration: Dict[str, Any]):
        logger.info("Update Slider configuration")
        # Updated configuration if there are any changes.
        if self.__configuration != configuration:
            self.__configuration = deepcopy(configuration)
            logger.info("Slider configuration updated")

    async def run(self):
        try:
            while True:
                configuration = self.__configuration

                # Process each entry in media.
                for media in configuration["media_files"]:
                    # Finish if player is empty.
                    if self.__player is None:
                        logger.warning("Player is unavailable, exiting Slider")
                        return
                    # Get new configuration and start slide show from the beginning if there are any changes.
                    if configuration != self.__configuration:
                        logger.info("Configuration updated, restart sliding")
                        break

                    # Check if we should show black screen or slide show.
                    if self.__is_active_period(datetime.now().time(),
                                               self.__configuration["screensaver_settings"]["start_time"],
                                               self.__configuration["screensaver_settings"]["end_time"]):
                        path = media["path"]
                        # Skip invalid media.
                        if not os.path.isfile(path):
                            logger.error(f"No media file found: {path}, skipping")
                            continue

                        if media["type"] == "video":
                            # Play video.
                            await self.__player.play_video(path)
                        else:
                            # Show image.
                            await self.__player.show_image(path, media["time"])
                    else:
                        # Show black screen.
                        await self.__player.show_blank_screen(30)
        finally:
            logger.info("Finalize Slider")
            # Terminate slide show.
            self.terminate()

    @staticmethod
    def __is_active_period(now: time, start_time: time, end_time: time) -> bool:
        return (start_time <= end_time) == (start_time <= now < end_time)
