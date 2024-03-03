import asyncio
from threading import Event
from typing import Optional

from loguru import logger
from vlc import EventType, Instance, MediaPlayer

from .configuration import get_black_screen_file


class Player:
    __instance: Optional[Instance]
    __player: Optional[MediaPlayer]
    __end_reached_event: Optional[Event]

    def __init__(self, window_id: int):
        logger.info("Initialize Player")
        # Create VLC instance and player.
        self.__instance = Instance(["--quiet"])
        self.__player = self.__instance.media_player_new()
        # Set window to draw images and video out.
        self.__player.set_xwindow(window_id)
        # Initially should be empty.
        self.__end_reached_event = None

    def terminate(self):
        logger.info("Terminate player")
        # Set and clean up event.
        if self.__end_reached_event is not None:
            self.__end_reached_event.set()
            self.__end_reached_event = None
            logger.info("Delete ended event")
        # Stop and clean up VLC player.
        if self.__player is not None:
            if self.__player.is_playing():
                self.__player.stop()
            self.__player.release()
            self.__player = None
            logger.info("Delete VLC player")
        # Stop and clean up VLC instance.
        if self.__instance is not None:
            self.__instance.release()
            self.__instance = None
            logger.info("Delete VLC instance")

    @staticmethod
    def __play_video_sync(player: MediaPlayer, ended_event: Event):
        # Start playing and wait until finished.
        player.play()
        ended_event.wait()
        logger.info("Video ended")

    @staticmethod
    def __end_reached_callback(event, end_reached_event):
        # Just set end reached event.
        end_reached_event.set()

    async def play_video(self, path: str):
        if self.__instance is None or self.__player is None:
            return

        instance = self.__instance
        player = self.__player

        media = None
        events = None

        try:
            logger.info("Play video")
            # Create media instance and set it to the player.
            media = instance.media_new(path)
            media.get_mrl()
            player.set_media(media)
            # Initialize end reached event.
            self.__end_reached_event = end_reached_event = Event()
            # Register end of video callback.
            events = player.event_manager()
            events.event_attach(EventType.MediaPlayerEndReached, Player.__end_reached_callback, end_reached_event)
            # Run player in separate blocking thread and wait for it.
            # We do stuff in a such way because play() method is non-blocking. So we put call of this method
            # into a separate thread, which is waiting for end reached event, that is set in register callback.
            # And callback is called asynchronously by the VLC player.
            await asyncio.to_thread(self.__play_video_sync, player, end_reached_event)

        finally:
            logger.info("Finalize playing video")
            # Detach callback, set event and clean it up.
            if events is not None:
                events.event_detach(EventType.MediaPlayerEndReached)
            self.__end_reached_event.set()
            self.__end_reached_event = None
            # Stop playing and free media.
            if player.is_playing():
                player.stop()
            if media is not None:
                media.release()

    async def show_image(self, path: str, duration: int):
        media = None

        try:
            logger.info("Show image")
            # Create media instance and set it to the player.
            media = self.__instance.media_new(path)
            media.get_mrl()
            self.__player.set_media(media)
            self.__player.play()
            # Wait for some time.
            await asyncio.sleep(duration)
        finally:
            logger.info("Finalize image show")
            # Clean up media.
            if media is not None:
                media.release()

    async def show_blank_screen(self, duration: int):
        # Just show black screen.
        await self.show_image(get_black_screen_file(), duration)
