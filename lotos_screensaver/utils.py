import os
import sys


def expand_path(path: str) -> str:
    if not os.path.isabs(path):
        path = os.path.expanduser(path)

        if not os.path.isabs(path):
            path = os.path.join(os.path.dirname(os.path.realpath(sys.argv[0])), path)

    return os.path.expandvars(os.path.normpath(path))


def get_xid() -> int:
    return int(os.environ["XSCREENSAVER_WINDOW"], 16)
