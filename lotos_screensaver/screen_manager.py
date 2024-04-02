from typing import Optional, Tuple

import numpy as np
from PIL.Image import Image, fromarray
from Xlib import X, threaded
from Xlib.display import Display
from Xlib.xobject.drawable import Pixmap, Window
from Xlib.xobject.fontable import GC
from loguru import logger
from threading import Event, Thread
from concurrent.futures import ThreadPoolExecutor


class ScreenManager:
    __xid: int
    __screen_size: Tuple[int, int]
    __display: Display
    __screensaver_window: Window
    __window: Window
    __pixmap: Pixmap
    __gc: GC
    __image: Optional[Image]
    __default_image: Image
    __draw_thread: Thread
    __draw_event: Event
    __draw_flag: bool
    __exit_flag: bool
    __draw_executor: ThreadPoolExecutor

    def __init__(self, xid: int):
        self.__xid = xid

        self.__draw_executor = ThreadPoolExecutor()

        self.__display = display = Display()

        self.__draw_event = Event()
        self.__draw_flag = self.__exit_flag = False
        self.__draw_thread = Thread(target=self.__internal_draw)
        self.__draw_thread.start()

        self.__screensaver_window = self.__display.create_resource_object("window", xid)

        # Determine screen size, create child window and map it.
        screen = display.screen()
        geometry = self.__screensaver_window.get_geometry()
        self.__screen_size = width, height = geometry.width, geometry.height

        self.__window = window = self.__screensaver_window.create_window(
            0, 0, width, height, 0, screen.root_depth,
            X.CopyFromParent, X.CopyFromParent,
            background_pixel=screen.black_pixel,
            colormap=X.CopyFromParent,
            event_mask=X.ExposureMask | X.StructureNotifyMask
        )
        window.map()

        self.__image = None
        self.__default_image = fromarray(np.zeros((height, width, 3), dtype=np.uint8))
        self.__pixmap = self.__window.create_pixmap(width, height, self.__display.screen().root_depth)
        self.__gc = self.__window.create_gc(foreground=0, background=0)
        self.redraw()

    def run(self):
        try:
            while True:
                event = self.__display.next_event()

                if event.type == X.Expose:
                    if event.count == 0:
                        self.redraw()
                elif event.type == X.DestroyNotify:
                    break
        finally:
            self.__close()
            logger.info("Stop X loop")

    def update_image(self, image: Optional[Image]):
        self.__image = image

    def redraw(self):
        self.__draw_executor.submit(self.__internal_draw)

    def __internal_draw(self):
        self.__pixmap.put_pil_image(self.__gc, 0, 0, self.__default_image if self.__image is None else self.__image)
        self.__window.copy_area(self.__gc, self.__pixmap, 0, 0, self.__screen_size[0], self.__screen_size[1], 0, 0)
        self.__display.flush()

    def close(self):
        self.__exit_flag = True
        if self.__window is not None:
            self.__window.destroy()
            self.__window = None
            self.__display.flush()

    @property
    def screen_size(self) -> Tuple[int, int]:
        return self.__screen_size