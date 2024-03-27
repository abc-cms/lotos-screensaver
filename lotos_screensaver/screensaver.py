import signal
from threading import Event, Thread
from time import monotonic
from typing import Optional
from PIL.Image import Image, fromarray

import cv2
import numpy as np
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
        self.__overlay_manager = OverlayManager(initial_time, screen_size)

        self.__update_thread = None
        self.__exit_update_thread_event = None

        signal.signal(signal.SIGINT, self.__sigint_handler)

    def run(self):
        self.__exit_update_thread_event = Event()
        self.__update_thread = Thread(target=self.__run_update_loop)
        self.__update_thread.start()
        self.__run_screen_loop()

    def __run_screen_loop(self):
        self.__screen_manager.run()

    def __run_update_loop(self):
        index = 0
        for timestamp, operations in OperationManager(self.__configuration_manager, self.__frame_manager,
                                                      self.__overlay_manager):
            current_timestamp = monotonic()
            index += 1
            for operation in operations:
                operation_type, parameters = operation["type"], operation["parameters"]
                if operation_type == "update_configuration":
                    ...
                    #self.__update_configuration(current_timestamp)
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
        image = self.__cook_overlay(timestamp, image)
        image = fromarray(image)
        self.__screen_manager.update_image(image)
        self.__screen_manager.redraw()

    def __cook_player(self) -> np.ndarray:
        sw, sh = self.__screen_manager.screen_size
        frame = np.zeros((sh, sw, 3), dtype=np.uint8)

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

        return frame

    def __cook_overlay(self, timestamp: float, image: np.ndarray) -> np.ndarray:
        overlay = self.__overlay_manager.overlay(timestamp)
        frame = self.__overlay_manager.frame(timestamp)

        left, top, width, height, radius, color = (
            overlay.left, overlay.top, overlay.width, overlay.height, overlay.radius, overlay.color
        )

        #image[top:top + height, left:left + width] = frame
        mask = frame.copy()
        mask[np.where(np.any(mask != [0, 0, 0], axis=-1))] = np.asarray((255, 255, 255), dtype=np.uint8)
        image[top:top + height, left:left + width] = np.bitwise_or(np.bitwise_and(
            image[top:top + height, left:left + width],
            np.bitwise_not(mask),
        ), frame)

        return image

        #image_draw = Draw(image)
        #image_draw.rounded_rectangle((left, top, left + width, top + height), radius, fill=color)
        #font = truetype("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 36)
        #size = font.getsize_multiline(overlay.text)
        #image_draw.text((left + int((width - size[0]) / 2), top + int((height - size[1]) / 2)), overlay.text, overlay.text_color, font, align="center")
        #return image

    def __sigint_handler(self):
        if self.__exit_update_thread_event is not None:
            self.__exit_update_thread_event.set()

    @staticmethod
    def __configure_logger():
        # Clean up and set only the one file handler.
        logger.remove()
        logger.add(get_log_file(), rotation="1 MB", retention="14 days")