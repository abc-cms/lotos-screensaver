from copy import deepcopy
from datetime import datetime
from random import randrange
from typing import Any, Dict, Optional, Tuple

from lotos_screensaver import Activity, AnimationCurve, Button
from .manager import Manager


class OverlayManager(Manager):
    SWITCH_DURATION: float = 5  # Seconds.
    ANIMATION_DURATION: float = 1  # Seconds.
    ANIMATION_STEPS: int = 15
    ANIMATION_STEP_DURATION: float = ANIMATION_DURATION / ANIMATION_STEPS

    __BUTTON_HEIGHT: int = 100
    __BUTTON_RADIUS: int = 15
    __BUTTON_COLOR: Tuple[int, int, int] = 0, 255, 0
    __BUTTON_TEXT: str = "Press to enter\nmenu"
    __BUTTON_TEXT_COLOR: Tuple[int, int, int] = 255, 255, 255
    __BUTTON_FONT: str = ""
    __BUTTON_SIDE_MARGIN: int = 50
    __BUTTON_SIDE_RANDOM_MARGIN: int = 215
    __BUTTON_BOTTOM_MARGIN: int = 50
    __BUTTON_BOTTOM_RANDOM_MARGIN: int = 215

    __screen_size: Tuple[int, int]
    __animation_interval: Tuple[float, float]
    __configuration: Dict[str, Any]
    __activity: Activity
    __current_button: Button
    __next_button: Button

    def __init__(self, initial_timestamp: float, screen_size: Tuple[int, int], configuration: Dict[str, Any]):
        super().__init__(initial_timestamp)

        self.__screen_size = screen_size
        self.update_configuration(initial_timestamp, configuration)

    def update_configuration(self, timestamp: float, configuration: Dict[str, Any]):
        self.__configuration = deepcopy(configuration)
        self.__activity = Activity(
            (
                (
                    self.__configuration["screensaver_settings"]["start_time"],
                    self.__configuration["screensaver_settings"]["end_time"]
                ),
            )
        )
        self.__reset(timestamp)

    def __reset(self, timestamp: float):
        self.initial_timestamp = timestamp

        self.__animation_interval = (
            timestamp + self.SWITCH_DURATION,
            timestamp + self.SWITCH_DURATION + self.ANIMATION_DURATION
        )

        self.__current_button = self.__build_new_button()
        self.__next_button = self.__build_new_button()

    def is_animating(self, timestamp: float) -> bool:
        return self.__animation_interval[0] <= timestamp < self.__animation_interval[1]

    def duration(self, timestamp: float) -> float:
        now_time = datetime.now()
        if self.__activity.is_active(now_time):
            return self.ANIMATION_STEP_DURATION if self.is_animating(timestamp) else self.SWITCH_DURATION

        return self.__activity.get_duration_to_next_activity_period(now_time)

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

    def overlay(self, timestamp: float) -> Optional[Button]:
        if self.__activity.is_active(datetime.now()):
            return AnimationCurve(self.__current_button, self.__next_button).interpolated(
                *self.__animation_interval, timestamp
            )

        return None

    def update(self, timestamp: float):
        if self.__activity.is_active(datetime.now()):
            if self.__animation_interval is None:
                self.initial_timestamp = timestamp
                self.__animation_interval = (
                    timestamp + self.SWITCH_DURATION,
                    timestamp + self.SWITCH_DURATION + self.ANIMATION_DURATION
                )

            if timestamp >= self.__animation_interval[1]:
                self.initial_timestamp = self.__animation_interval[1]

                self.__animation_interval = (
                    self.__animation_interval[1] + self.SWITCH_DURATION,
                    self.__animation_interval[1] + self.SWITCH_DURATION + self.ANIMATION_DURATION
                )

                self.__current_button, self.__next_button = self.__next_button, self.__build_new_button()
            elif self.is_animating(timestamp):
                self.initial_timestamp = self.__animation_interval[0] + (timestamp - self.__animation_interval[0]) \
                                         // self.ANIMATION_STEP_DURATION * self.ANIMATION_STEP_DURATION
        else:
            self.initial_timestamp = timestamp
            self.__animation_interval = None
