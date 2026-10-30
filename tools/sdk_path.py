"""Where these tools find the Open-XY SDK: the checkout OPXY_MD_SDK names, or
../open-xy. Its newest platform folder names the stock OS a build needs."""
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SDK = pathlib.Path(os.environ.get("OPXY_MD_SDK") or ROOT.parent / "open-xy")
SDK_INCLUDE = SDK / "sdk" / "include"


def platform():
    """The SDK's platform folder for the newest stock OS it supports, sdk/platform/opxy-<version>."""
    return sorted((SDK / "sdk" / "platform").glob("opxy-*"), key=lambda p: [int(n) for n in p.name[5:].split(".")])[-1]


def stock_version():
    return platform().name[5:]
