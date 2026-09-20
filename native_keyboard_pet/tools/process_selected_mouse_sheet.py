"""Turn the user-selected 2x2 magenta mouse sheet into four runtime frames."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "source" / "mouse-sheet-selected-chroma.png"
OUTPUT = ROOT / "assets" / "mouse-edition"
FRAME_SIZE = 96
PADDING_X = 3
PADDING_TOP = 3
PADDING_BOTTOM = 3


def remove_magenta(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    cleaned = []
    for red, green, blue, _ in rgba.get_flattened_data():
        # The selected sheet uses a slightly varying hot-magenta background.
        # Orange flame, pink cheeks and cyan click sparks do not satisfy this
        # hue test, so only the chroma field is removed.
        is_magenta = (
            red > 135
            and blue > 115
            and green < min(red, blue) * 0.68
            and abs(red - blue) < 105
        )
        cleaned.append((red, green, blue, 0 if is_magenta else 255))
    rgba.putdata(cleaned)
    return rgba


def main() -> None:
    sheet = remove_magenta(Image.open(SOURCE))
    width, height = sheet.size
    cells: list[tuple[int, Image.Image, tuple[int, int, int, int]]] = []
    for row in range(2):
        top = round(row * height / 2)
        bottom = round((row + 1) * height / 2)
        for column in range(2):
            left = round(column * width / 2)
            right = round((column + 1) * width / 2)
            cell = sheet.crop((left, top, right, bottom))
            bbox = cell.getchannel("A").getbbox()
            if bbox is None:
                raise RuntimeError(f"empty mouse frame: {row * 2 + column}")
            cells.append((row * 2 + column, cell, bbox))

    max_width = max(box[2] - box[0] for _, _, box in cells)
    max_height = max(box[3] - box[1] for _, _, box in cells)
    scale = min(
        (FRAME_SIZE - 2 * PADDING_X) / max_width,
        (FRAME_SIZE - PADDING_TOP - PADDING_BOTTOM) / max_height,
    )

    OUTPUT.mkdir(parents=True, exist_ok=True)
    for index, cell, bbox in cells:
        sprite = cell.crop(bbox)
        target = (
            max(1, round(sprite.width * scale)),
            max(1, round(sprite.height * scale)),
        )
        sprite = sprite.resize(target, Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", (FRAME_SIZE, FRAME_SIZE), (0, 0, 0, 0))
        canvas.alpha_composite(
            sprite,
            ((FRAME_SIZE - sprite.width) // 2,
             FRAME_SIZE - PADDING_BOTTOM - sprite.height),
        )
        canvas.save(OUTPUT / f"mouse_{index}.png", optimize=True)

    print(
        f"wrote {len(cells)} selected mouse frames; "
        f"source max={max_width}x{max_height}; "
        f"scale={scale:.4f}; frame={FRAME_SIZE}x{FRAME_SIZE}"
    )


if __name__ == "__main__":
    main()
