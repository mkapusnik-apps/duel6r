"""Pixel anchors on the actual full client; normalized regions are diagnostic only.

The float32 transform is Menu.cpp::initializePresentation, not a new game layout.
Multi-anchor checks follow MenuTransferFrame / SharedArenaImageAssertions. They
recognize a route boundary, never certify content, animation or visual acceptance.
"""
from dataclasses import dataclass
import math
import struct

from .common import require

GEOMETRIES = ((850, 700), (1280, 720), (1280, 900), (1920, 1080))


def f32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


@dataclass(frozen=True)
class Canvas:
    width: int
    height: int
    network: bool = False

    def __post_init__(self):
        require((self.width, self.height) in GEOMETRIES, "unsupported capture geometry")

    @property
    def scale(self):
        return min(f32(1.35), f32(self.width / 850), f32(self.height / 700))

    @property
    def origin(self):
        if self.network:
            # NetworkMenu truncates the scaled extent BEFORE centering. Main
            # Menu truncates AFTER centering; at 720p their origins differ 1px.
            return ((self.width - int(f32(850 * self.scale))) // 2,
                    (self.height - int(f32(700 * self.scale))) // 2)
        return (int(f32(self.width - f32(850 * self.scale)) * .5),
                int(f32(self.height - f32(700 * self.scale)) * .5))

    def point(self, x, y):
        """Logical bottom-left menu coordinates -> native top-left pixels."""
        require(0 <= x < 850 and 0 <= y < 700, "menu point outside canvas")
        tx, ty = self.origin
        return tx + int(f32(x * self.scale)), self.height - ty - int(f32(y * self.scale))

    def box(self, box):
        x, y, w, h = box
        require(w > 0 and h > 0 and x >= 0 and y >= 0 and x + w <= 850 and y + h <= 700,
                "anchor outside menu canvas")
        tx, ty = self.origin
        return (tx + math.ceil(f32(x * self.scale)),
                self.height - ty - int(f32((y + h) * self.scale)),
                tx + int(f32((x + w) * self.scale)),
                self.height - ty - math.ceil(f32(y * self.scale)))


# Empty margins inside the three actual header strips / white Persons list.
# NET-01 overlays the unchanged menu; exclude its otherwise misleading anchors.
MAIN = [
    {"box": [180, 560, 130, 12], "rgb": [0, 0, 200], "minimum": .85},
    {"box": [540, 560, 90, 12], "rgb": [0, 0, 200], "minimum": .85},
    {"box": [795, 560, 35, 12], "rgb": [0, 0, 200], "minimum": .85},
    {"box": [285, 330, 12, 70], "rgb": [255, 255, 255], "minimum": .80},
    {"box": [510, 438, 70, 8], "rgb": [0, 0, 200], "maximum": .10},
]
ENTRY = MAIN[:3] + [
    {"box": [510, 438, 70, 8], "rgb": [0, 0, 200], "minimum": .85, "network": True},
]
STATES = {"main-menu": MAIN, "network-entry": ENTRY}


def recognize(rgb, canvas, anchors):
    require(len(rgb) == canvas.width * canvas.height * 3, "incomplete full-client RGB frame")
    require(isinstance(anchors, list) and 1 <= len(anchors) <= 20, "invalid anchor set")
    observations = []
    for anchor in anchors:
        space = Canvas(canvas.width, canvas.height, anchor.get("network", False))
        left, top, right, bottom = space.box(anchor["box"])
        color, tolerance = anchor["rgb"], anchor.get("tolerance", 12)
        require(len(color) == 3 and all(isinstance(c, int) and 0 <= c <= 255 for c in color)
                and 0 <= tolerance <= 32, "invalid anchor color")
        minimum, maximum = anchor.get("minimum", 0), anchor.get("maximum", 1)
        require(0 <= minimum <= maximum <= 1, "invalid anchor threshold")
        count = matched = 0
        for y in range(top, bottom):
            for x in range(left, right):
                offset = (y * canvas.width + x) * 3
                matched += all(abs(rgb[offset + c] - color[c]) <= tolerance for c in range(3))
                count += 1
        ratio = matched / count if count else 0
        observations.append({"fraction": ratio, "passed": count > 0 and minimum <= ratio <= maximum})
    return all(item["passed"] for item in observations), observations
