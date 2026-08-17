#include "epd_decoder.h"

#include <cstring>

#include "ArduinoLog.h"
#include "esp_heap_caps.h"
#include "miniz.h"

namespace
{
tinfl_decompressor *g_epdTinflContext = nullptr;

tinfl_decompressor *get_epd_tinfl_context()
{
    if (g_epdTinflContext == nullptr)
    {
        // g_epdTinflContext = static_cast<tinfl_decompressor *>(
        //     heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        // if (g_epdTinflContext == nullptr)
        // {
            g_epdTinflContext = static_cast<tinfl_decompressor *>(
                heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        // }
        if (g_epdTinflContext == nullptr)
        {
            Log.errorln("[epd_decoder] Failed to allocate %u bytes for tinfl state",
                        static_cast<unsigned>(sizeof(tinfl_decompressor)));
        }
    }
    return g_epdTinflContext;
}

size_t decompress_epd_payload(uint8_t *dst,
                              size_t dst_capacity,
                              const uint8_t *src,
                              size_t src_size)
{
    tinfl_decompressor *ctx = get_epd_tinfl_context();
    if (ctx == nullptr)
    {
        return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    }

    tinfl_init(ctx);

    size_t in_size = src_size;
    size_t out_size = dst_capacity;
    const int flags = TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;

    tinfl_status status = tinfl_decompress(ctx,
                                           reinterpret_cast<const mz_uint8 *>(src),
                                           &in_size,
                                           reinterpret_cast<mz_uint8 *>(dst),
                                           reinterpret_cast<mz_uint8 *>(dst),
                                           &out_size,
                                           flags);
    if (status != TINFL_STATUS_DONE)
    {
        Log.errorln("[epd_decoder] tinfl_decompress failed with status %d (consumed=%u produced=%u)",
                    static_cast<int>(status),
                    static_cast<unsigned>(in_size),
                    static_cast<unsigned>(out_size));
        return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    }
    return out_size;
}
} // namespace

bool epd_decode_frame(File &file, EpdFrameBuffer &frame)
{
    frame = {};

    uint8_t header[16];
    if (file.read(header, sizeof(header)) != sizeof(header))
    {
        Log.errorln("[epd_decoder] Failed to read EPD frame header");
        return false;
    }

    if (memcmp(header, "EPD0", 4) != 0)
    {
        Log.errorln("[epd_decoder] Invalid EPD frame magic");
        return false;
    }

    uint8_t version = header[4];
    (void)version;
    uint8_t bitDepth = header[5];
    uint32_t width = (static_cast<uint32_t>(header[8]) << 24) |
                     (static_cast<uint32_t>(header[9]) << 16) |
                     (static_cast<uint32_t>(header[10]) << 8) |
                     static_cast<uint32_t>(header[11]);
    uint32_t height = (static_cast<uint32_t>(header[12]) << 24) |
                      (static_cast<uint32_t>(header[13]) << 16) |
                      (static_cast<uint32_t>(header[14]) << 8) |
                      static_cast<uint32_t>(header[15]);

    if (width == 0 || height == 0)
    {
        Log.errorln("[epd_decoder] Invalid EPD frame dimensions");
        return false;
    }

    if (!(bitDepth == 1 || bitDepth == 4))
    {
        Log.errorln("[epd_decoder] Unsupported EPD frame bit depth: %u", static_cast<unsigned>(bitDepth));
        return false;
    }

    size_t rowBytes = ((static_cast<size_t>(width) * bitDepth) + 7) / 8;
    size_t expectedSize = rowBytes * static_cast<size_t>(height);
    if (expectedSize == 0 || expectedSize > (8 * 1024 * 1024))
    {
        Log.errorln("[epd_decoder] EPD frame payload size invalid");
        return false;
    }
    Log.traceln("[epd_decoder] Header parsed: %lux%lu bitDepth=%u rowBytes=%u expected=%u bytes",
                static_cast<unsigned long>(width),
                static_cast<unsigned long>(height),
                static_cast<unsigned>(bitDepth),
                static_cast<unsigned>(rowBytes),
                static_cast<unsigned>(expectedSize));

    size_t remaining = file.size() - file.position();
    if (remaining == 0)
    {
        Log.errorln("[epd_decoder] EPD frame contains no payload");
        return false;
    }
    Log.traceln("[epd_decoder] Compressed payload size: %u bytes", static_cast<unsigned>(remaining));

    uint8_t *compressed = static_cast<uint8_t *>(heap_caps_malloc(remaining, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
    if (compressed == nullptr)
    {
        Log.errorln("[epd_decoder] Failed to allocate %u bytes for compressed EPD frame",
                    static_cast<unsigned>(remaining));
        return false;
    }

    if (file.read(compressed, remaining) != static_cast<int>(remaining))
    {
        Log.errorln("[epd_decoder] Failed to read EPD frame payload");
        heap_caps_free(compressed);
        return false;
    }

    uint8_t *pixelBuffer = static_cast<uint8_t *>(heap_caps_malloc(expectedSize, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
    if (pixelBuffer == nullptr)
    {
        Log.errorln("[epd_decoder] Failed to allocate %u bytes for EPD frame pixels",
                    static_cast<unsigned>(expectedSize));
        heap_caps_free(compressed);
        return false;
    }

    size_t written = decompress_epd_payload(pixelBuffer, expectedSize, compressed, remaining);
    heap_caps_free(compressed);

    if (written == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || written != expectedSize)
    {
        if (written == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        {
            Log.errorln("[epd_decoder] Decompression of EPD frame failed (tinfl error already logged)");
        }
        else
        {
            Log.errorln("[epd_decoder] Decompression wrote %u bytes, expected %u",
                        static_cast<unsigned>(written),
                        static_cast<unsigned>(expectedSize));
        }
        heap_caps_free(pixelBuffer);
        return false;
    }

    frame.width = width;
    frame.height = height;
    frame.bitDepth = bitDepth;
    frame.pixels = pixelBuffer;
    frame.dataSize = expectedSize;
    return true;
}

void epd_free_frame(EpdFrameBuffer &frame)
{
    if (frame.pixels != nullptr)
    {
        heap_caps_free(frame.pixels);
        frame.pixels = nullptr;
    }
    frame.dataSize = 0;
    frame.width = 0;
    frame.height = 0;
    frame.bitDepth = 0;
}
