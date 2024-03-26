from datetime import datetime, time, timedelta
from itertools import dropwhile
from typing import Tuple


class Activity:
    __periods: Tuple[Tuple[time, time], ...]

    def __init__(self, periods: Tuple[Tuple[time, time], ...]):
        self.__periods = periods

    def is_active(self, timestamp: datetime) -> bool:
        return any(start <= timestamp.time() < end for start, end in self.__periods)

    def get_duration_to_next_activity_period(self, timestamp: datetime) -> float:
        next_time = tuple(dropwhile(lambda x: timestamp.time() < x[0], self.__periods))
        if next_time:
            next_timestamp = datetime.combine(timestamp.date(), next_time[0][0])
        else:
            next_timestamp = datetime.combine(timestamp.date() + timedelta(days=1), self.__periods[0][0])
        return (next_timestamp - timestamp).total_seconds()
