import os
from abc import ABCMeta, abstractmethod
from copy import deepcopy
from itertools import cycle
from typing import Any, Dict, Iterator, List, Optional

import cv2
import numpy as np
from loguru import logger

from .manager import Manager


class Media(metaclass=ABCMeta):
    _frame: np.ndarray
    _duration: float

    @property
    def frame(self) -> np.ndarray:
        return self._frame

    @property
    def duration(self) -> float:
        return self._duration

    @property
    @abstractmethod
    def is_video(self) -> bool:
        ...

    @abstractmethod
    def __iter__(self):
        ...


class Image(Media):
    def __init__(self, path: str, duration: float):
        self._frame = cv2.cvtColor(cv2.imread(path), cv2.COLOR_BGR2RGB)
        self._duration = duration

    @property
    def is_video(self) -> bool:
        return False

    def __iter__(self):
        yield self.frame


class Video(Media):
    __video: Optional[Any]

    def __init__(self, path: str):
        self.__video = video = cv2.VideoCapture(path)
        self._duration = 1.0 / video.get(cv2.CAP_PROP_FPS)
        _, self._frame = video.read()

    @property
    def is_video(self) -> bool:
        return True

    def __iter__(self):
        while True:
            result, self.__frame = self.__video.read()
            if not result:
                self.__video.release()
                break

            self.__frame = cv2.cvtColor(self.__frame, cv2.COLOR_BGR2RGB)
            yield self.__frame


class MediaList:
    __configuration: List[Dict[str, Any]]

    def __init__(self, configuration: List[Dict[str, Any]]):
        self.__configuration = configuration

    def __iter__(self):
        configuration = deepcopy(self.__configuration)

        while True:
            # Process each entry in media.
            for media in cycle(configuration):
                path = media["path"]

                # Skip invalid media.
                if not os.path.isfile(path):
                    logger.error(f"No media file found: {path}, skipping")
                    continue

                yield Video(path) if media["type"] == "video" else Image(path, media["time"])


class FrameManager(Manager):
    __configuration: Dict[str, Any]
    __media_iterator: Iterator[Media]
    __media: Media
    __frame_iterator: Iterator[np.ndarray]
    __frame: np.ndarray

    def __init__(self, initial_timestamp: float, configuration: Dict[str, Any]):
        super().__init__(initial_timestamp)
        self.update_configuration(initial_timestamp, configuration)

    def update_configuration(self, timestamp: float, configuration: Dict[str, Any]):
        self.__configuration = deepcopy(configuration)
        self.__reset(timestamp)

    def __reset(self, timestamp: float):
        self.initial_timestamp = timestamp
        self.__media_iterator = iter(MediaList(self.__configuration["media_files"]))
        self.__media = next(self.__media_iterator)
        self.__frame_iterator = iter(self.__media)
        self.__frame = next(self.__frame_iterator)

    @property
    def frame(self) -> Optional[np.ndarray]:
        return self.__frame

    def is_update_required(self, timestamp: float) -> bool:
        return timestamp >= self.initial_timestamp + self.duration(timestamp)

    def duration(self, timestamp: float) -> float:
        return self.__media.duration

    def update(self, timestamp: float):
        next_timestamp = self.next_timestamp(timestamp)

        if timestamp >= next_timestamp:
            try:
                self.__frame = next(self.__frame_iterator)
            except StopIteration:
                self.__media = next(self.__media_iterator)
                self.__frame_iterator = iter(self.__media)
                self.__frame = next(self.__frame_iterator)
                self.initial_timestamp = timestamp

            self.initial_timestamp = self.next_timestamp(timestamp)

    @property
    def is_playing_video(self) -> bool:
        return type(self.__media) is Video
