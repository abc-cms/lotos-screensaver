from typing import Any, Optional, Tuple

import numpy as np
from PIL import Image
from Xlib import X
from Xlib.display import Display
from loguru import logger


class ScreenManager:
    __xid: int
    __screen_size: Tuple[int, int]
    __display: Display
    __screensaver_window: Any
    __window: Any
    __pixmap: Any
    __gc: Any
    __image: Optional[Image]
    __default_image: Image

    def __init__(self, xid: int):
        self.__xid = xid

        self.__display = display = Display()
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
            event_mask=X.ExposureMask
        )
        window.map()

        self.__image = None
        self.__default_image = Image.fromarray(np.zeros((height, width, 3)))
        self.__pixmap = self.__window.create_pixmap(width, height, self.__display.screen().root_depth)
        self.__gc = self.__window.create_gc(foreground=0, background=0)
        self.redraw()

    def run(self):
        while True:
            event = self.__display.next_event()

            if event.type == X.Expose:
                self.redraw()
            elif event.type == X.DestroyNotify:
                break

        logger.info("Stop X loop")

    def update_image(self, image: Image):
        self.__image = image

    def redraw(self):
        self.__pixmap.put_pil_image(self.__gc, 0, 0, self.__default_image if self.__image is None else self.__image)
        self.__window.copy_area(self.__gc, self.__pixmap, 0, 0, self.__screen_size[0], self.__screen_size[1], 0, 0)
        self.__display.flush()

    @property
    def screen_size(self) -> Tuple[int, int]:
        return self.__screen_size
