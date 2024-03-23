import cv2
import asyncio
from loguru import logger
from Xlib import display, X, Xutil, threaded
import numpy as np
from dataclasses import dataclass
from PIL import Image
from time import sleep

@dataclass
class Button:
    left: int
    top: int
    width: int
    height: int
    radius: int
    color: Tuple[int, int, int]
    text_color: Tuple[int, int, int]
    text: string

class AnimationCurve:
    __start: Button
    __end: Button
    __duration: float
    __steps: int

    def __init__(self, start: Button, end: button, duration: float, steps: int):
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

        for i in range(1, steps + 1):
            left += i * left_delta
            top += i * left_delta
            width += i * width_delta
            height += i * height_delta
            yield step_duration, Button(left, top, width, height, start.radius, start.color, start.text_color, start.text)

class CvPlayer:
    def __init__(self, window_id, configuration: Dict[str, Any]):
        logger.info(f"Initialize Slider using xid={window_id}")
        # Initialize configuration and create player.
        self.__configuration = deepcopy(configuration)):
        self.__path = path
        self.__initial_time = None
        self.__video = None
        self.__delay = None
        self.__window_id = window_id

        self.__display = display.Display()
        self.__window = self.__display.create_resource_object("window", window_id)


        # Child window
        width, height = 500, 500
        self.__child_window = self.__window.create_window(
            50, 50, width, height, 0,
            self.__display.screen().root_depth,
            X.CopyFromParent,
            X.CopyFromParent,
            background_pixel=self.__display.screen().white_pixel,
            colormap=X.CopyFromParent,
        )
        self.__child_window.map()
        self.__bgpm = bgpm = self.__child_window.create_pixmap(320, 320, self.__display.screen().root_depth)
        self.__bggc = bggc = self.__child_window.create_gc(foreground=0, background=0)

        logger.info(f"Initialize ButtonOverlay using xid={window_id}")
        self.__terminate_player = False
        self.__counter = 0

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


    async def draw_frame(self):
        while True:
            self.__counter += 1
            x = self.__x
            await asyncio.sleep(1.0 / 25.0)

    def __play_video(self):
        video  = cv2.VideoCapture(self.__path)
        delay = 1.0 / video.get(cv2.CAP_PROP_FPS)

        while not self.__terminate_player:
            self.__x = x = 320
            pil_data = Image.fromarray(cv2.resize(video.read()[1], (x, x)))
            self.__bgpm.put_pil_image(self.__bggc, 0, 0, pil_data)
            self.__bgpm.fill_rectangle(self.__bggc, self.__counter, self.__counter, 50, 50)
            self.__child_window.copy_area(self.__bggc, self.__bgpm, 0, 0, x, x, 100, 100)
            self.__counter += 1
            self.__display.sync()
            sleep(delay)

    async def __update_overlay(self):
        self.__overlay_position = None
        self.__overlay_size = None

        self.__overlay_animation_duration = 1.0
        self.__overlay_switch_duration = 5.0

        animate: bool = False
        sleep_duration: float

        try:
            while True:
                sleep_duration = 0.1 if animate else self.__overlay_switch_duration
                await asyncio.sleep(sleep_duration)
        finally:
            ...

    def __draw_overlay(self, frame: np.ndarray):
        ...

    async def play(self):
        try:
            logger.info("Start XLIB loop")
            await asyncio.gather(asyncio.to_thread(self.__loop), asyncio.to_thread(self.__play_video))

        finally:
            self.__child_window.unmap()
            self.__child_window.destroy()
            self.__window.destroy()
            self.__window.unmap()

    def __loop(self):
        while True:
            logger.info("!!!!!!!!!!!!!!!!!!!!!!!!")
            e = self.__display.next_event()

    async def show_blank_screen(self, duration: int):
        # Just show black screen.
        await self.show_image(get_black_screen_file(), duration)

    @staticmethod
    def __is_active_period(now: time, start_time: time, end_time: time) -> bool:
        return (start_time <= end_time) == (start_time <= now < end_time)