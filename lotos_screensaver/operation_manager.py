from time import monotonic
from typing import Dict, List, Optional, Sequence, Tuple, Union

from .configuration_manager import ConfigurationManager
from .frame_manager import FrameManager
from .overlay_manager import OverlayManager


class OperationManager:
    __configuration_manager: ConfigurationManager
    __frame_manager: Optional[FrameManager]
    __overlay_manager: Optional[OverlayManager]

    def __init__(self, configuration_manager: ConfigurationManager, frame_manager: Optional[FrameManager] = None,
                 overlay_manager: Optional[OverlayManager] = None):
        self.__configuration_manager = configuration_manager
        self.__frame_manager = frame_manager
        self.__overlay_manager = overlay_manager
        initial_timestamp = monotonic()
        self.__configuration_manager.set_initial_timestamp(initial_timestamp)
        if frame_manager is not None:
            self.__frame_manager.set_initial_timestamp(initial_timestamp)
        if overlay_manager is not None:
            self.__overlay_manager.set_initial_timestamp(initial_timestamp)

    def __iter__(self) -> Tuple[float, List[Dict[str, Union[str, Sequence]]]]:
        while True:
            operations: List[Dict[str, Union[str, Sequence]]] = []
            current_timestamp = monotonic()

            # Check if overlay animation is required.
            is_overlay_update_required = \
                self.__overlay_manager is not None and self.__overlay_manager.is_update_required(current_timestamp)
            if is_overlay_update_required:
                operations.append({"type": "update_overlay", "parameters": (current_timestamp,)})

            # Check if frame update is required.
            is_frame_update_required = self.__frame_manager is not None and self.__frame_manager.is_update_required(
                current_timestamp)
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
            timestamps = []
            next_frame_timestamp = 0
            if self.__frame_manager is not None:
                next_frame_timestamp = self.__frame_manager.next_timestamp(current_timestamp)
                timestamps.append(next_frame_timestamp)
            if self.__overlay_manager is not None:
                timestamps.append(self.__overlay_manager.next_timestamp(current_timestamp))
            timestamps.append(self.__configuration_manager.next_timestamp(current_timestamp))

            if self.__frame_manager is not None and self.__overlay_manager is not None:
                if self.__frame_manager.is_playing_video and \
                        (self.__frame_manager.duration(current_timestamp) <
                         self.__overlay_manager.duration(current_timestamp)):
                    next_timestamp = next_frame_timestamp
                else:
                    next_timestamp = min(timestamps)
            else:
                next_timestamp = min(timestamps)
            yield next_timestamp, operations
