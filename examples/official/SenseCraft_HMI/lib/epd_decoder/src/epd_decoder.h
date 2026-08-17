#pragma once

#include <Arduino.h>
#include <FS.h>

struct EpdFrameBuffer
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bitDepth = 0;
    uint8_t *pixels = nullptr;
    size_t dataSize = 0;
};

bool epd_decode_frame(File &file, EpdFrameBuffer &frame);
void epd_free_frame(EpdFrameBuffer &frame);
