from abc import ABCMeta, abstractmethod


class Manager(metaclass=ABCMeta):
    __initial_timestamp: float

    def __init__(self, initial_timestamp: float):
        self.__initial_timestamp = initial_timestamp

    def set_initial_timestamp(self, initial_timestamp: float):
        self.__initial_timestamp = initial_timestamp

    @property
    def initial_timestamp(self) -> float:
        return self.__initial_timestamp

    @initial_timestamp.setter
    def initial_timestamp(self, value: float):
        self.__initial_timestamp = value

    def next_timestamp(self, timestamp: float) -> float:
        return self.initial_timestamp + self.duration(timestamp)

    @abstractmethod
    def duration(self, timestamp: float) -> float:
        ...
