Import("env")
from pathlib import Path

env.Replace( MKFSTOOL=env.get("PROJECT_DIR") + '/mklittlefs' )  # PlatformIO now believes it has actually created a SPIFFS

project_dir = Path(env.get("PROJECT_DIR"))
pio_env = env.get("PIOENV")
epaper_cpp = project_dir / ".pio" / "libdeps" / pio_env / "Seeed_GFX" / "Extensions" / "EPaper.cpp"

if epaper_cpp.exists():
    text = epaper_cpp.read_text()
    old = "EPaper::EPaper() : TFT_eSprite(this), _sleep(true), _entemp(true), _temp(16.00), _humi(50.00)"
    new = "EPaper::EPaper() : TFT_eSprite(this), _grayLevel(0), _sleep(true), _entemp(true), _temp(16.00), _humi(50.00)"
    if old in text and new not in text:
        epaper_cpp.write_text(text.replace(old, new, 1))
        print("Patched Seeed_GFX EPaper _grayLevel initialization")
