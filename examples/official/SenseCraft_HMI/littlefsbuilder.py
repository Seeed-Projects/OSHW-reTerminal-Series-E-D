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

# Normalizes the JD79676 header reference for case-sensitive build hosts.
# 统一 JD79676 头文件引用，供大小写敏感的构建环境使用。
tft_espi_cpp = project_dir / ".pio" / "libdeps" / pio_env / "Seeed_GFX" / "TFT_eSPI.cpp"
jd79676_header = tft_espi_cpp.parent / "TFT_Drivers" / "JD79676_init.h"

if tft_espi_cpp.exists() and jd79676_header.exists():
    text = tft_espi_cpp.read_text()
    old = 'TFT_Drivers/JD79676_Init.h'
    new = 'TFT_Drivers/JD79676_init.h'
    if old in text:
        tft_espi_cpp.write_text(text.replace(old, new))
        print("Patched Seeed_GFX JD79676 header reference")
