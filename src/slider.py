import asyncio
import os
from copy import deepcopy
from datetime import datetime
from itertools import cycle
from typing import Any, Dict, Optional

from loguru import logger
from vlc import EventType, Instance, MediaPlayer, callbackmethod


class Slider:
    __window_id: int
    __configuration: Dict[str, Any]
    __instance: Optional[Instance]
    __player: Optional[MediaPlayer]
    __event: asyncio.Event

    def __init__(self, window_id: int, configuration: Dict[str, Any]):
        self.__window_id = window_id
        self.__configuration = deepcopy(configuration)

        self.__instance = None
        self.__player = None

        self.__event = asyncio.Event()

    async def start(self):
        self.__instance = Instance()
        self.__player = self.__instance.media_player_new()
        self.__player.set_xwindow(self.__window_id)

        print("show")
        await self.__show()

    def stop(self):
        if self.__player is not None and self.__player.is_playing():
            self.__player.stop()
        self.__player = None
        self.__instance = None

    async def __show(self):
        configuration = self.__configuration

        print(configuration["media_files"])
        for media in configuration["media_files"]:
            if True:  #self.__is_active_period():
                path = media["path"]
                if not os.path.isfile(path):
                    logger.error("No file found")
                    continue

                if media["type"] == "video":
                    await self.__play_video(path)
                else:
                    print(path)
                    await self.__show_image(path, media["time"])
            else:
                await self.__show_blank_screen()

            print("finish")

    async def __play_video(self, path: str):
        media = self.__instance.media_new(path)
        media.get_mrl()
        player = self.__player
        player.set_media(media)
        events = player.event_manager()
        events.event_attach(EventType.MediaPlayerEndReached, self.__end_reached_callback)
        self.__event.clear()
        self.__player.play()
        await self.__event.wait()
        events.event_detach(EventType.MediaPlayerEndReached)

    @callbackmethod
    def __end_reached_callback(self, event):
        self.__event.set()

    async def __show_image(self, path: str, duration: int):
        print("IMAGE")
        media = self.__instance.media_new(path)
        media.get_mrl()
        self.__player.set_media(media)
        self.__player.play()
        await asyncio.sleep(duration)

    async def __show_blank_screen(self):
        ...

    def __is_active_period(self) -> bool:
        start_time = self.__configuration["screensaver_settings"]["start_time"]
        end_time = self.__configuration["screensaver_settings"]["end_time"]
        now_time = datetime.now().time()
        return start_time <= end_time == start_time <= now_time < end_time