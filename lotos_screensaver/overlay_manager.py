from copy import deepcopy
from datetime import datetime
from random import randrange
from typing import Any, Dict, Optional, Tuple
import numpy as np
from PIL.ImageDraw import Draw
from PIL.ImageFont import truetype
from PIL.Image import Image, fromarray
from lotos_screensaver import Activity, AnimationCurve, Button
from .manager import Manager


class OverlayManager(Manager):
    SWITCH_DURATION: float = 5  # Seconds.
    ANIMATION_DURATION: float = 1  # Seconds.
    ANIMATION_STEPS: int = 30
    ANIMATION_STEP_DURATION: float = ANIMATION_DURATION / ANIMATION_STEPS

    __BUTTON_HEIGHT: int = 100
    __BUTTON_RADIUS: int = 15
    __BUTTON_COLOR: Tuple[int, int, int] = 0, 255, 0
    __BUTTON_TEXT: str = "Press to enter\nmenu"
    __BUTTON_TEXT_COLOR: Tuple[int, int, int] = 255, 255, 255
    __BUTTON_TEXT_FONT: str = "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf"
    __BUTTON_TEXT_SIZE: int = 30
    __BUTTON_SIDE_MARGIN: int = 50
    __BUTTON_SIDE_RANDOM_MARGIN: int = 215
    __BUTTON_BOTTOM_MARGIN: int = 50
    __BUTTON_BOTTOM_RANDOM_MARGIN: int = 215

    __screen_size: Tuple[int, int]
    __animation_interval: Tuple[float, float]
    __activity: Activity
    __current_button: Button
    __next_button: Button
    __is_animated: bool

    def __init__(self, initial_timestamp: float, screen_size: Tuple[int, int]):
        super().__init__(initial_timestamp)

        self.__screen_size = screen_size
        self.update_configuration(initial_timestamp, None)

        # Initialize and cache constant drawings.
        # Left and right borders.
        minimal_button_frame = np.zeros((self.__BUTTON_HEIGHT, 2 * self.__BUTTON_RADIUS, 3), dtype=np.uint8)
        image = fromarray(minimal_button_frame)
        image_draw = Draw(image)
        image_draw.rounded_rectangle((0, 0, 2 * self.__BUTTON_RADIUS, self.__BUTTON_HEIGHT), self.__BUTTON_RADIUS, fill=self.__BUTTON_COLOR)
        minimal_button_frame = np.array(image)
        self.__left_border_cached = minimal_button_frame[:, :self.__BUTTON_RADIUS].copy()
        self.__right_border_cached = minimal_button_frame[:, self.__BUTTON_RADIUS:].copy()

        # Text.
        font = truetype(self.__BUTTON_TEXT_FONT, self.__BUTTON_TEXT_SIZE)
        text_size = font.getsize_multiline(self.__BUTTON_TEXT)
        text_frame = np.full((text_size[1], text_size[0], 3), self.__BUTTON_COLOR, dtype=np.uint8)
        image = fromarray(text_frame)
        image_draw = Draw(image)
        image_draw.text((0, 0), self.__BUTTON_TEXT, self.__BUTTON_TEXT_COLOR, font, align="center")
        self.__text_cached = np.array(image)

    def update_configuration(self, timestamp: float, configuration: Optional[Dict[str, Any]]):
        self.__reset(timestamp)

    def __reset(self, timestamp: float):
        self.initial_timestamp = timestamp

        self.__animation_interval = (
            timestamp + self.SWITCH_DURATION,
            timestamp + self.SWITCH_DURATION + self.ANIMATION_DURATION
        )

        self.__current_button = self.__build_new_button()
        self.__next_button = self.__build_new_button()

        self.__is_animating = False

    def is_animating(self, timestamp: float) -> bool:
        return self.__animation_interval[0] <= timestamp < self.__animation_interval[1]

    def is_update_required(self, timestamp) -> bool:
        return timestamp >= self.initial_timestamp + self.duration(timestamp)

    def duration(self, timestamp: float) -> float:
        return self.ANIMATION_STEP_DURATION if self.is_animating(timestamp) else self.SWITCH_DURATION

    def __build_new_button(self) -> Button:
        x_offset = randrange(self.__BUTTON_SIDE_RANDOM_MARGIN)
        y_offset = randrange(self.__BUTTON_BOTTOM_RANDOM_MARGIN)

        return Button(
            left=self.__BUTTON_SIDE_MARGIN + x_offset,
            top=self.__screen_size[1] - self.__BUTTON_BOTTOM_MARGIN - self.__BUTTON_HEIGHT - y_offset,
            width=self.__screen_size[0] - 2 * (self.__BUTTON_SIDE_MARGIN + x_offset),
            height=self.__BUTTON_HEIGHT,
            radius=self.__BUTTON_RADIUS,
            color=self.__BUTTON_COLOR,
            text_color=self.__BUTTON_TEXT_COLOR,
            text=self.__BUTTON_TEXT
        )

    def overlay(self, timestamp: float) -> Button:
        return AnimationCurve(self.__current_button, self.__next_button).interpolated(
            *self.__animation_interval, timestamp
        )

    def frame(self, timestamp: float) -> np.ndarray:
        overlay = self.overlay(timestamp)

        left, top, width, height, radius, color = (
            overlay.left, overlay.top, overlay.width, overlay.height, overlay.radius, overlay.color
        )

        frame = np.full((height, width, 3), self.__BUTTON_COLOR, dtype=np.uint8)
        frame[:, 0:self.__BUTTON_RADIUS] = self.__left_border_cached
        frame[:, width - self.__BUTTON_RADIUS:] = self.__right_border_cached
        text_shape = self.__text_cached.shape[:2]
        ox, oy = (width - text_shape[1]) // 2, (height - text_shape[0]) // 2
        frame[oy:oy + text_shape[0], ox:ox + text_shape[1]] = self.__text_cached

        return frame

    def update(self, timestamp: float):
        if timestamp >= self.__animation_interval[1]:
            self.__is_animating = False
            self.initial_timestamp = self.__animation_interval[1]

            sss = self.__animation_interval

            self.__animation_interval = (
                self.__animation_interval[1] + self.SWITCH_DURATION,
                self.__animation_interval[1] + self.SWITCH_DURATION + self.ANIMATION_DURATION
            )

            self.__current_button = self.__next_button
            self.__next_button = self.__build_new_button()

        elif self.__animation_interval[0] <= timestamp < self.__animation_interval[1]:
            self.__is_animating = True
            self.initial_timestamp = self.__animation_interval[0] + ((timestamp - self.__animation_interval[0])
                // self.ANIMATION_STEP_DURATION) * self.ANIMATION_STEP_DURATION