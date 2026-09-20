"""Split the generated 4x3 chroma-key sprite sheet into aligned runtime frames."""

from __future__ import annotations

from pathlib import Path
from collections import deque

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "source" / "sprite-sheet-transparent.png"
OUTPUT = ROOT / "assets" / "actions"
FRAME_SIZE = 96
PADDING_X = 3
PADDING_TOP = 3
PADDING_BOTTOM = 3
ROWS = ("type", "sleep", "cute")


def harden_alpha(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = []
    for red, green, blue, alpha in rgba.get_flattened_data():
        # The character is deliberately pixel art: hard alpha avoids colored
        # chroma fringes after Windows layered-window composition.
        pixels.append((red, green, blue, 255 if alpha >= 96 else 0))
    rgba.putdata(pixels)
    return rgba


def keep_largest_component(image: Image.Image) -> Image.Image:
    """Remove crop-boundary debris while keeping the complete connected sprite."""
    alpha = image.getchannel("A")
    width, height = alpha.size
    source = alpha.load()
    visited: set[tuple[int, int]] = set()
    largest: list[tuple[int, int]] = []
    for y in range(height):
        for x in range(width):
            if source[x, y] == 0 or (x, y) in visited:
                continue
            component: list[tuple[int, int]] = []
            queue = deque([(x, y)])
            visited.add((x, y))
            while queue:
                point = queue.popleft()
                component.append(point)
                px, py = point
                for adjacent in ((px - 1, py), (px + 1, py), (px, py - 1), (px, py + 1)):
                    ax, ay = adjacent
                    if (
                        0 <= ax < width
                        and 0 <= ay < height
                        and source[ax, ay] != 0
                        and adjacent not in visited
                    ):
                        visited.add(adjacent)
                        queue.append(adjacent)
            if len(component) > len(largest):
                largest = component

    cleaned = Image.new("RGBA", image.size, (0, 0, 0, 0))
    source_pixels = image.load()
    target_pixels = cleaned.load()
    for x, y in largest:
        target_pixels[x, y] = source_pixels[x, y]
    return cleaned


def main() -> None:
    sheet = harden_alpha(Image.open(SOURCE))
    width, height = sheet.size
    cells: list[tuple[str, int, Image.Image, tuple[int, int, int, int]]] = []

    for row, prefix in enumerate(ROWS):
        top = round(row * height / 3)
        bottom = round((row + 1) * height / 3)
        for column in range(4):
            left = round(column * width / 4)
            right = round((column + 1) * width / 4)
            cell = sheet.crop((left, top, right, bottom))
            if prefix != "sleep":
                cell = keep_largest_component(cell)
            bbox = cell.getchannel("A").getbbox()
            if bbox is None:
                raise RuntimeError(f"empty frame: {prefix}_{column}")
            cells.append((prefix, column, cell, bbox))

    max_width = max(box[2] - box[0] for _, _, _, box in cells)
    max_height = max(box[3] - box[1] for _, _, _, box in cells)
    scale = min(
        (FRAME_SIZE - 2 * PADDING_X) / max_width,
        (FRAME_SIZE - PADDING_TOP - PADDING_BOTTOM) / max_height,
    )

    OUTPUT.mkdir(parents=True, exist_ok=True)
    for prefix, column, cell, bbox in cells:
        sprite = cell.crop(bbox)
        target = (
            max(1, round(sprite.width * scale)),
            max(1, round(sprite.height * scale)),
        )
        sprite = sprite.resize(target, Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", (FRAME_SIZE, FRAME_SIZE), (0, 0, 0, 0))
        x = (FRAME_SIZE - sprite.width) // 2
        y = FRAME_SIZE - PADDING_BOTTOM - sprite.height
        canvas.alpha_composite(sprite, (x, y))
        canvas.save(OUTPUT / f"{prefix}_{column}.png", optimize=True)

    print(
        f"wrote {len(cells)} frames; source max={max_width}x{max_height}; "
        f"scale={scale:.4f}; frame={FRAME_SIZE}x{FRAME_SIZE}"
    )


if __name__ == "__main__":
    main()
