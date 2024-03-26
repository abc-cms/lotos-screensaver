import signal
from threading import Event, Thread
from time import monotonic
from typing import Optional

import cv2
import numpy as np
from PIL import Image, ImageDraw
from loguru import logger

from lotos_screensaver import ConfigurationManager, FrameManager, OperationManager, OverlayManager, ScreenManager
from lotos_screensaver.configuration import get_log_file
from lotos_screensaver.utils import get_xid


class Screensaver:
    __configuration_manager: ConfigurationManager
    __frame_manager: FrameManager
    __overlay_manager: OverlayManager
    __screen_manager: ScreenManager
    __update_thread: Optional[Thread]
    __exit_update_thread_event: Optional[Event]

    def __init__(self):
        self.__screen_manager = ScreenManager(get_xid())
        screen_size = self.__screen_manager.screen_size

        initial_time = monotonic()
        self.__configuration_manager = ConfigurationManager(initial_time)
        configuration = self.__configuration_manager.configuration
        self.__frame_manager = FrameManager(initial_time, configuration)
        self.__overlay_manager = OverlayManager(initial_time, screen_size, configuration)

        self.__update_thread = None
        self.__exit_update_thread_event = None

        signal.signal(signal.SIGINT, self.__sigint_handler)

    def run(self):
        self.__exit_update_thread_event = Event()
        self.__update_thread = Thread(target=self.__run_update_loop)
        self.__update_thread.start()

    def __run_screen_loop(self):
        self.__screen_manager.run()

    def __run_update_loop(self):
        for timestamp, operations in OperationManager(self.__configuration_manager, self.__frame_manager,
                                                      self.__overlay_manager):
            current_timestamp = monotonic()

            for operation in operations:
                operation_type, parameters = operation["type"], operation["parameters"]
                if operation_type == "update_configuration":
                    self.__update_configuration(current_timestamp)
                elif operation_type == "update_overlay":
                    self.__update_overlay(*parameters)
                elif operation_type == "update_frame":
                    self.__update_frame(*parameters)
                elif operation_type == "redraw":
                    self.__redraw(current_timestamp)

            delay = timestamp - monotonic()
            if delay > 0:
                self.__exit_update_thread_event.wait(timeout=delay)
            if self.__exit_update_thread_event.is_set():
                break

    def __update_configuration(self, timestamp: float):
        self.__configuration_manager.update()
        configuration = self.__configuration_manager.configuration
        self.__frame_manager.update_configuration(timestamp, configuration)
        self.__overlay_manager.update_configuration(timestamp, configuration)

    def __update_overlay(self, timestamp: float):
        self.__overlay_manager.update(timestamp)

    def __update_frame(self, timestamp: float):
        self.__frame_manager.update(timestamp)

    def __redraw(self, timestamp: float):
        image = self.__cook_player()
        # If player is active.
        if image is not None:
            image = self.__cook_overlay(timestamp, image)
            self.__screen_manager.update_image(image)
            self.__screen_manager.redraw()

    def __cook_player(self) -> Optional[Image]:
        if self.__frame_manager.frame is None:
            return None

        sw, sh = self.__screen_manager.screen_size
        frame = np.zeros((sh, sw, 3))

        media = self.__frame_manager.frame
        screen_ratio = sh / sw
        mh, mw = media.shape[:2]
        media_ratio = mh / mw
        if screen_ratio > media_ratio:
            media = cv2.resize(media, (sw, int(sw / mw * mh)))
        else:
            media = cv2.resize(media, (int(sh / mh * mw), sh))
        mh, mw = media.shape[:2]
        ox, oy = (sw - mw) // 2, (sh - mh) // 2
        frame[oy:oy + mh, ox:ox + mw] = media

        return Image.fromarray(frame)

    def __cook_overlay(self, timestamp: float, image: Image) -> Image:
        overlay = self.__overlay_manager.overlay(timestamp)
        if overlay is None:
            return image

        left, top, width, height, radius, color = (
            overlay.left, overlay.top, overlay.width, overlay.height, overlay.radius, overlay.color
        )
        image_draw = ImageDraw.Draw(image)
        image_draw.rounded_rectangle((left, top, left + width, top + height), radius, fill=color)
        return image_draw

    def __sigint_handler(self):
        if self.__exit_update_thread_event is not None:
            self.__exit_update_thread_event.set()

    @staticmethod
    def __configure_logger():
        # Clean up and set only the one file handler.
        logger.remove()
        logger.add(get_log_file(), rotation="1 MB", retention="14 days")
