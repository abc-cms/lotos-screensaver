from time import monotonic
from typing import Dict, List, Sequence, Tuple, Union

from .configuration_manager import ConfigurationManager
from .frame_manager import FrameManager
from .overlay_manager import OverlayManager


class OperationManager:
    __configuration_manager: ConfigurationManager
    __frame_manager: FrameManager
    __overlay_manager: OverlayManager

    def __init__(self, configuration_manager: ConfigurationManager, frame_manager: FrameManager,
                 overlay_manager: OverlayManager):
        self.__configuration_manager = configuration_manager
        self.__frame_manager = frame_manager
        self.__overlay_manager = overlay_manager
        initial_timestamp = monotonic()
        self.__configuration_manager.set_initial_timestamp(initial_timestamp)
        self.__frame_manager.set_initial_timestamp(initial_timestamp)
        self.__overlay_manager.set_initial_timestamp(initial_timestamp)

    def __iter__(self) -> Tuple[float, List[Dict[str, Union[str, Sequence]]]]:
        while True:
            operations: List[Dict[str, Union[str, Sequence]]] = []
            current_timestamp = monotonic()

            # Check if overlay animation is required.
            is_overlay_update_required = self.__overlay_manager.is_update_required(current_timestamp)
            if is_overlay_update_required:
                operations.append({"type": "update_overlay", "parameters": (current_timestamp,)})

            # Check if frame update is required.
            is_frame_update_required = self.__frame_manager.is_update_required(current_timestamp)
            if is_frame_update_required:
                operations.append({"type": "update_frame", "parameters": (current_timestamp,)})

            # Check if configuration update is required.
            is_configuration_update_required = self.__configuration_manager.is_update_required(current_timestamp)
            if is_configuration_update_required:
                operations.append({"type": "update_configuration", "parameters": (current_timestamp,)})

            # Redraw if required.
            if is_overlay_update_required or is_frame_update_required:
                operations.append({"type": "redraw", "parameters": ()})

            # Calculate the next timestamp.
            next_frame_timestamp = self.__frame_manager.next_timestamp(current_timestamp)
            next_overlay_timestamp = self.__overlay_manager.next_timestamp(current_timestamp)
            next_configuration_timestamp = self.__configuration_manager.next_timestamp(current_timestamp)

            if self.__frame_manager.is_playing_video and (self.__frame_manager.duration(current_timestamp) < self.__overlay_manager.duration(current_timestamp)):
                next_timestamp = next_frame_timestamp
            else:
                next_timestamp = min(next_frame_timestamp, next_overlay_timestamp, next_configuration_timestamp)
            yield next_timestamp, operations