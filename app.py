from pathlib import Path

from sapling_pet.app import run


if __name__ == "__main__":
    assets = Path(__file__).parent / "assets"
    run(assets / "stages")
