from .button import Button


class AnimationCurve:
    __start: Button
    __end: Button

    def __init__(self, start: Button, end: Button):
        self.__start = start
        self.__end = end

    def interpolated(self, start_timestamp: float, end_timestamp: float, current_timestamp: float) -> Button:
        fraction = max(min((current_timestamp - start_timestamp) / (end_timestamp - start_timestamp), 1), 0)

        start, end = self.__start, self.__end

        return Button(
            self.__interpolate(start.left, end.left, fraction),
            self.__interpolate(start.top, end.top, fraction),
            self.__interpolate(start.width, end.width, fraction),
            self.__interpolate(start.height, end.height, fraction),
            start.radius,
            start.color,
            start.text_color,
            start.text
        )

    @staticmethod
    def __interpolate(start: int, end: int, fraction: float) -> int:
        return start + int((end - start) * fraction)