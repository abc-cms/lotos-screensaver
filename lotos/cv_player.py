import cv2
import asyncio
from loguru import logger
from Xlib import display, X, Xutil
import numpy as np

from PIL import Image

class CvPlayer:
    def __init__(self, path: str, window_id):
        self.__path = path
        self.__initial_time = None
        self.__video = None
        self.__delay = None
        self.__window_id = window_id

        self.__display = display.Display()
        self.__window = self.__display.create_resource_object("window", window_id)

        # Child window
        #width, height = 75, 75
        #self.__window = parent_window.create_window(
        #    150, 150, width, height, 0,
        #    self.__display.screen().root_depth,
        #    X.CopyFromParent,
        #    X.CopyFromParent,
        #    background_pixel=self.__display.screen().white_pixel,
        #    colormap=X.CopyFromParent,
        #)

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

        logger.info(f"Initialize ButtonOverlay using xid={window_id}")

        self.__counter = 0

    async def draw_frame(self):
        while True:
            self.__counter += 1
            x = self.__x
            pil_data = Image.fromarray(cv2.resize(self.__video.read()[1], (x, x)))
            #self.__bgpm = bgpm = self.__child_window.create_pixmap(x, x, self.__display.screen().root_depth)
            #self.__bggc = bggc = self.__child_window.create_gc(foreground=0, background=0)
            self.__bgpm.put_pil_image(self.__bggc, 0, 0, pil_data)
            self.__bgpm.fill_rectangle(self.__bggc, self.__counter, self.__counter, 50, 50)
            self.__child_window.copy_area(self.__bggc, self.__bgpm, 0, 0, x, x, 100, 100)
            #loop = asyncio.get_event_loop()
            #loop.call_at(self.__initial_time + self.__counter / 25, self.draw_frame)
            self.__display.sync()
            await asyncio.sleep(1.0 / 25.0)

    async def play(self):
        loop = asyncio.get_event_loop()
        self.__video  = cv2.VideoCapture(self.__path)
        fps = self.__video.get(cv2.CAP_PROP_FPS)
        self.__delay = 1 / fps
        self.__initial_time = loop.time()
        #asyncio.call_at(self.__start_show)
        self.__child_window.map()
        self.__x = x = 320
        self.__bgpm = bgpm = self.__child_window.create_pixmap(x, x, self.__display.screen().root_depth)
        self.__bggc = bggc = self.__child_window.create_gc(foreground=0, background=0)
        #loop.call_at(self.__initial_time, self.draw_frame)
        pil_data = Image.fromarray(cv2.resize(self.__video.read()[1], (x, x)))
        self.__bgpm.put_pil_image(self.__bggc, 0, 0, pil_data)
        self.__child_window.copy_area(self.__bggc, self.__bgpm, 0, 0, x, x, 100, 100)


        try:
            logger.info("Start XLIB loop")
            await asyncio.gather(asyncio.to_thread(self.__loop), self.draw_frame())

        #except Exception as e:
        #    logger.info(e)

        finally:
            self.__child_window.unmap()
            self.__child_window.destroy()
            self.__window.destroy()
            self.__window.unmap()

    def __loop(self):
        while True:
            logger.info("!!!!!!!!!!!!!!!!!!!!!!!!")
            e = self.__display.next_event()