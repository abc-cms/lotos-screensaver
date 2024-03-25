import cv2
import asyncio
from loguru import logger
from Xlib import X, Xutil, threaded
from Xlib.display import Display
import numpy as np
from dataclasses import dataclass
from PIL import Image, ImageDraw
from time import sleep, time
from typing import Any, Dict, Tuple
from copy import deepcopy
from datetime import datetime
import os
from random import randrange

@dataclass
class Button:
    left: int
    top: int
    width: int
    height: int
    radius: int
    color: Tuple[int, int, int]
    text_color: Tuple[int, int, int]
    text: str

class AnimationCurve:
    __start: Button
    __end: Button
    __duration: float
    __steps: int

    def __init__(self, start: Button, end: Button, duration: float, steps: int):
        assert duration > 0
        assert steps > 0

        self.__start = start
        self.__end = end
        self.__duration = duration
        self.__steps = steps

    def __iter__(self):
        start = self.__start
        end = self.__end
        steps = self.__steps
        step_duration = self.__duration / steps

        left_delta = (end.left - start.left) / steps
        top_delta = (end.top - start.top) / steps
        width_delta = (end.width - start.width) / steps
        height_delta = (end.height - start.height) / steps

        left, top, width, height = start.left, start.top, start.width, start.height

        for i in range(1, steps + 1):
            yield step_duration, Button(
                int(left + i * left_delta),
                int(top + i * top_delta),
                int(width + i * width_delta),
                int(height + i * height_delta),
                start.radius,
                start.color,
                start.text_color,
                start.text
            )

class CvPlayer:
    __configuration: Dict[str, Any]
    __media: Any
    __terminate_player: bool

    __display: Display
    __screen_size: Tuple[int, int]
    __screensaver_window: Any
    __window: Any
    __window_id: int
    __pixmap: Any
    __gc: Any

    __overlay: Any

    def __init__(self, window_id: int, configuration: Dict[str, Any]):
        logger.info(f"Initialize Player using xid={window_id}")
        self.__is_playing_video = False
        self.__overlay = None

        # Initialize configuration and create player.
        self.__configuration = deepcopy(configuration)
        self.__media = None
        self.__window_id = window_id

        # Initialize display, attach to the screensaver window and create a child one
        # that will be used to draw media.
        display = self.__display = Display()
        self.__screensaver_window = self.__display.create_resource_object("window", window_id)
        # Determine screen size, create child window and map it.
        screen = display.screen()
        geometry = self.__screensaver_window.get_geometry()
        width, height = self.__screen_size = geometry.width, geometry.height
        window = self.__window = self.__screensaver_window.create_window(
            0, 0, width, height, 0, screen.root_depth,
            X.CopyFromParent, X.CopyFromParent,
            background_pixel=screen.black_pixel,
            colormap=X.CopyFromParent,
            event_mask=X.ExposureMask
        )
        window.map()
        self.__pixmap = self.__window.create_pixmap(width, height, self.__display.screen().root_depth)
        self.__gc = self.__window.create_gc(foreground=0, background=0)

        # Set termination flag to false initially.
        self.__terminate_player = False

    def terminate(self):
        logger.info("Terminate player")
        if self.__window is not None:
            self.__window.unmap()

    def update_configuration(self, configuration: Dict[str, Any]):
        logger.info("Update Slider configuration")
        # Updated configuration if there are any changes.
        if self.__configuration != configuration:
            self.__configuration = deepcopy(configuration)
            logger.info("Slider configuration updated")

    async def run(self):
        try:
            await asyncio.gather(
                asyncio.to_thread(self.__x_loop),
                self.__update_overlay(),
                self.__slide()
            )
        finally:
            self.terminate()

    async def __slide(self):
        while True:
            configuration = deepcopy(self.__configuration)

            # Process each entry in media.
            for media in configuration["media_files"]:
                # Get new configuration and start slide show from the beginning if there are any changes.
                if configuration != self.__configuration:
                    logger.info("Configuration updated, restart sliding")
                    break

                # Check if we should show black screen or slide show.
                if self.__is_active_period(datetime.now().time(),
                                           configuration["screensaver_settings"]["start_time"],
                                           configuration["screensaver_settings"]["end_time"]):
                    path = media["path"]
                    # Skip invalid media.
                    if not os.path.isfile(path):
                        logger.error(f"No media file found: {path}, skipping")
                        continue

                    if media["type"] == "video":
                        # Play video.
                        await self.__play(path)
                    else:
                        # Show image.
                        await self.__show(path, media["time"])
                else:
                    # Show black screen (break every 30 seconds to check if configuration has been changed
                    # or to check inactivity time).
                    await self.__show_blank_screen(30)


    async def __play(self, path: str):
        try:
            video  = cv2.VideoCapture(path)

            if video.isOpened():
                self.__is_playing_video = True
                delay = 1.0 / video.get(cv2.CAP_PROP_FPS)
                current_frame_time = asyncio.get_running_loop().time()

                while True:
                    next_frame_time = current_frame_time = current_frame_time + delay
                    result, frame = video.read()
                    if not result:
                        break

                    self.__media = frame
                    self.__update_window()

                    actual_delay = next_frame_time - asyncio.get_running_loop().time()
                    if actual_delay > 0:
                        await asyncio.sleep(actual_delay)
            else:
                logger.error(f"Unable to process video file {path}")
        finally:
            self.__is_playing_video = False
            video.release()

    def __update_window(self):
        self.__draw()
        #self.__window.clear_area(0, 0, self.__screen_size[0], self.__screen_size[1])

    async def __show(self, path: str, duration: float):
        self.__media = cv2.cvtColor(cv2.imread(path), cv2.COLOR_BGR2RGB)
        self.__update_window()
        await asyncio.sleep(duration)

    def __draw(self):
        frame = np.zeros((self.__screen_size[1], self.__screen_size[0], 3), dtype=np.uint8)

        pixmap = gc = None

        try:
            sw, sh = self.__screen_size

            if self.__media is not None:
                #raise RuntimeError(f"Media {self.__media}")
                media = self.__media
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

            image = self.__postprocess_media(frame)
            # Create window pixmap.
            pixmap = self.__pixmap
            gc = self.__gc
            pixmap.put_pil_image(gc, 0, 0, image)
            self.__window.copy_area(gc, pixmap, 0, 0, self.__screen_size[0], self.__screen_size[1], 0, 0)
            self.__display.flush()
        finally:
            #if pixmap is not None:
            #    pixmap.free()
            #if gc is not None:
            #    gc.free()
            ...

    def __build_new_button(self) -> Button:
        x_offset = randrange(215)
        y_offset = randrange(215)

        return Button(
            left=50 + x_offset,
            top=self.__screen_size[1] - 50 - 150,
            width=self.__screen_size[0] - 100 - 2 * x_offset,
            height=150,
            radius=15,
            color=(0, 255, 0),
            text_color=(255, 255, 255),
            text="Press button\nto enter menu"
        )

    async def __update_overlay(self):
        self.__overlay_animation_duration = 1.0
        self.__overlay_switch_duration = 5.0

        overlay_frames_iterator = None

        current_frame_time = asyncio.get_running_loop().time()

        while True:
            if overlay_frames_iterator is None:
                if self.__overlay is None:
                    self.__overlay = self.__build_new_button()
                delay = self.__overlay_switch_duration
                overlay_frames_iterator = iter(AnimationCurve(self.__overlay, self.__build_new_button(), 1.0, 20))
            else:
                try:
                    delay, self.__overlay = next(overlay_frames_iterator)
                except StopIteration:
                    overlay_frames_iterator = None
                    continue
            if not self.__is_playing_video:
                self.__update_window()

            next_frame_time = current_frame_time = current_frame_time + delay
            actual_delay = next_frame_time - asyncio.get_running_loop().time()

            #if actual_delay < 0 or delay < 0.02:
            #    raise RuntimeError(f"{delay}, {actual_delay}!!!!!!!!!!!!")
            if actual_delay > 0:
                await asyncio.sleep(actual_delay)

    def __postprocess_media(self, frame: np.ndarray) -> np.ndarray:
        overlay: Button = self.__overlay
        image = Image.fromarray(frame)

        if overlay is not None:
            left, top, width, height, radius, color = overlay.left, overlay.top, overlay.width, overlay.height, overlay.radius, overlay.color
            image_draw = ImageDraw.Draw(image)
            image_draw.rounded_rectangle((left, top, left + width, top + height), radius, fill=color)

        return image

        # Draw intenal area.
        #cv2.rectangle(frame, (left, top + radius), (left + width, top + height - 2 * radius), color, -1)
        #cv2.rectangle(frame, (left + radius, top), (left + width - 2 * radius, top + height), color, -1)
        # Draw four rounded corners.
        #cv2.ellipse(frame, (left + radius, top + radius), (radius, radius), 0, 0, 90, overlay.color, -1)
        #cv2.ellipse(frame, (left + width - radius, top + radius), (radius, radius), 90, 0, 90, overlay.color, -1)
        #cv2.ellipse(frame, (left + width - radius, top + height - radius), (radius, radius), 180, 0, 90, overlay.color, -1)
        #cv2.ellipse(frame, (left + radius, top + height - radius), (radius, radius), 270, 0, 90, overlay.color, -1)
        # Draw text.

    async def play(self):
        logger.info("Start X loop")
        await asyncio.gather(asyncio.to_thread(self.__loop), asyncio.to_thread(self.__play_video))

    def __x_loop(self):
        while not self.__terminate_player:
            event = self.__display.next_event()

            if event.type == X.Expose:
                self.__draw()
            elif event.type == X.DestroyNotify:
                raise RuntimeError("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
                break

        logger.info("Stop X loop")

    async def show_blank_screen(self, duration: int):
        # Just show black screen.
        await self.show_image(get_black_screen_file(), duration)

    @staticmethod
    def __is_active_period(now: time, start_time: time, end_time: time) -> bool:
        return (start_time <= end_time) == (start_time <= now < end_time)


    def __start_sliding(self):
        initial_time = asyncio.get_running_loop().time()

        configuration_updater = ConfigurationUpdater(initial_time, update_period)
        player = Player()

        while True:
            configuration = deepcopy(self.__configuration)

            # Process each entry in media.
            for media in configuration["media_files"]:
                # Get new configuration and start slide show from the beginning if there are any changes.
                if configuration != self.__configuration:
                    logger.info("Configuration updated, restart sliding")
                    break


    async def __slide_new(self):
        loop = asyncio.get_running_loop()

        for timestamp, operation in self.__start_sliding():
            if operation == "draw":
                self.__draw_new()

            current_time = loop.time()
            delay = timestamp - current_time
            if delay > 0:
                await asyncio.sleep(delay)



        while True:
            configuration = deepcopy(self.__configuration)

            # Process each entry in media.
            for media in configuration["media_files"]:
                # Get new configuration and start slide show from the beginning if there are any changes.
                if configuration != self.__configuration:
                    logger.info("Configuration updated, restart sliding")
                    break

                # Check if we should show black screen or slide show.
                if self.__is_active_period(datetime.now().time(),
                                           configuration["screensaver_settings"]["start_time"],
                                           configuration["screensaver_settings"]["end_time"]):
                    path = media["path"]
                    # Skip invalid media.
                    if not os.path.isfile(path):
                        logger.error(f"No media file found: {path}, skipping")
                        continue

                    if media["type"] == "video":
                        # Play video.
                        await self.__play(path)
                    else:
                        # Show image.
                        await self.__show(path, media["time"])
                else:
                    # Show black screen (break every 30 seconds to check if configuration has been changed
                    # or to check inactivity time).
                    await self.__show_blank_screen(30)