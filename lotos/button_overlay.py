import asyncio
import os
from copy import deepcopy
from datetime import datetime, time
from typing import Any, Dict, Optional
from loguru import logger
from Xlib import X, display, Xutil
import random

class ButtonOverlay:
    SWITCH_DURATION: float = 5  # Seconds.
    ANIMATION_DURATION: float = 1  # Seconds.

    def __init__(self, window_id: int):
        self.__display = display.Display()
        parent_window = self.__display.create_resource_object("window", window_id)

        # Child window
        width, height = 75, 75
        self.__window = parent_window.create_window(
            150, 150, width, height, 0,
            self.__display.screen().root_depth,
            X.CopyFromParent,
            X.CopyFromParent,
            background_pixel=self.__display.screen().white_pixel,
            colormap=X.CopyFromParent,
        )

        logger.info(f"Initialize ButtonOverlay using xid={window_id}")

    def terminate(self):
        logger.info("Terminate slider")
        if self.__player is not None:
            # Terminate and clean up player.
            self.__player.terminate()
            self.__player = None
            logger.info("Slider terminated")

    def __loop(self):
        self.__window.map()
        while True:
            logger.info("!!!!!!!!!!!!!!!!!!!!!!!!")
            e = self.__display.next_event()

    async def __cleanup(self):
        await asyncio.sleep(2)
        self.__window.clear_area(width=30, height=30)
        #self.__window.unmap()

    async def __update(self):
        try:
            while True:
                await asyncio.sleep(self.SWITCH_DURATION)
                x = random.randint(10, 200)
                y = random.randint(10, 200)
                self.__window.configure(x=x, y=y)
                self.__display.sync()
        finally:
            pass


    async def run(self):
        try:
            logger.info("Start XLIB loop")
            await asyncio.gather(asyncio.to_thread(self.__loop), self.__cleanup(), self.__update())

        #except Exception as e:
        #    logger.info(e)

        finally:
            self.__window.clear_area()
            self.__window.unmap()
            self.__window.destroy()
            logger.info("Finalize ButtonOverlay")
            # Terminate slide show.
            # self.terminate()

    @staticmethod
    def __is_active_period(now: time, start_time: time, end_time: time) -> bool:
        return (start_time <= end_time) == (start_time <= now < end_time)