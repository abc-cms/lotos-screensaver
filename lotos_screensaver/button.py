from dataclasses import dataclass
from typing import Tuple


@dataclass
class Button:
    left: int
    top: int
    width: int
    height: int
    radius: int
    color: Tuple[int, int, int]
    text_color: Tuple[int, int, int]
    text: str
