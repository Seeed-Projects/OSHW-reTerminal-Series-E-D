#ifndef UTILS_IMAGE_PARSER_H
#define UTILS_IMAGE_PARSER_H

#include <Arduino.h>
#include <FS.h>

using fs::File;

namespace image_parser
{
bool draw_from_stream(File &file, int16_t x, int16_t y, bool with_color, bool overwrite);
}

#endif // UTILS_IMAGE_PARSER_H
