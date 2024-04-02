import os
import signal
from datetime import datetime
from threading import Event, Thread
from time import monotonic, sleep
from typing import Optional

import cv2
import numpy as np
from PIL.Image import fromarray
from loguru import logger

from lotos_screensaver import Activity, ConfigurationManager, FrameManager, OperationManager, OverlayManager, \
    ScreenManager
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
        self.__configure_logger()

        self.__screen_manager = ScreenManager(get_xid())
        initial_time = monotonic()
        self.__configuration_manager = ConfigurationManager(initial_time)

        self.__update_thread = None
        self.__exit_update_thread_event = None

        signal.signal(signal.SIGTERM, self.__sigint_handler)

    def run(self):
        self.__exit_update_thread_event = Event()
        self.__update_thread = Thread(target=self.__run_update_loop)
        self.__update_thread.start()
        self.__run_screen_loop()
        self.__update_thread.join()

        #if self.__configuration_manager.has_external_changes:
        #    pid = os.fork()
        #    if pid == 0:
        #        sleep(5)  # Wait for some time parent process to be finished.
        #        os.system("xscreensaver-command -restart")

    def __run_screen_loop(self):
        self.__screen_manager.run()

    def __run_update_loop(self):
        run_loop = True

        while run_loop:
            initial_time = monotonic()
            configuration = self.__configuration_manager.configuration
            self.__frame_manager = FrameManager(initial_time, self.__configuration_manager.configuration)
            self.__overlay_manager = OverlayManager(initial_time, self.__screen_manager.screen_size)

            activity = Activity(
                (
                    (
                        configuration["screensaver_settings"]["start_time"],
                        configuration["screensaver_settings"]["end_time"]
                    ),
                )
            )

            is_active = activity.is_active(datetime.now())

            if is_active:
                operation_manager = OperationManager(self.__configuration_manager, self.__frame_manager,
                                                     self.__overlay_manager)
            else:
                operation_manager = OperationManager(self.__configuration_manager)
                self.__screen_manager.update_image(None)
                self.__screen_manager.redraw()

            for timestamp, operations in operation_manager:
                if self.__exit_update_thread_event.is_set():
                    run_loop = False
                    break

                if activity.is_active(datetime.now()) != is_active:
                    break

                current_timestamp = monotonic()
                for operation in operations:
                    need_restart = True
                    operation_type, parameters = operation["type"], operation["parameters"]

                    if operation_type == "update_configuration":
                        self.__configuration_manager.update()
                        if self.__configuration_manager.has_internal_changes:
                            break
                    #elif operation_type == "update_overlay":
                    #    self.__overlay_manager.update(*parameters)
                    elif operation_type == "update_frame":
                        self.__frame_manager.update(*parameters)
                    elif operation_type == "redraw":
                        self.__overlay_manager.update(current_timestamp)
                        self.__redraw(current_timestamp)
                else:
                    need_restart = False

                if need_restart:
                    break

                delay = timestamp - monotonic()
                if delay > 0:
                    self.__exit_update_thread_event.wait(timeout=delay)

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

        mask = frame.copy()
        mask[np.where(np.any(mask != [0, 0, 0], axis=-1))] = np.asarray((255, 255, 255), dtype=np.uint8)
        image[top:top + height, left:left + width] = np.bitwise_or(np.bitwise_and(
            image[top:top + height, left:left + width],
            np.bitwise_not(mask),
        ), frame)

        return image

    def __sigint_handler(self, signum, frame):
        if self.__exit_update_thread_event is not None:
            self.__exit_update_thread_event.set()
        self.__screen_manager.close()

    @staticmethod
    def __configure_logger():
        # Clean up and set only the one file handler.
        logger.remove()
        logger.add(get_log_file(), rotation="1 MB", retention="14 days")