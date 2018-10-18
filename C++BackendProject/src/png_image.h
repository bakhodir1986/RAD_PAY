#ifndef PNG_IMAGE_H
#define PNG_IMAGE_H

#include <string>
#include <vector>
#include <png.h>
#include <boost/noncopyable.hpp>
#include "types.h"


class PNG_image_T : private boost::noncopyable{

public:
    int width;
    int  height;
    png_byte color_type;
    png_byte bit_depth;

    PNG_image_T();
    ~PNG_image_T();

    Error_T init_image(int w, int h, png_bytep bg_color);
    Error_T read_file(const std::string & filename);
    Error_T write_file(const std::string & filename);
    Error_T add_top_image(const PNG_image_T &src, int method);

    Error_T fill_qr(int qr_width, const unsigned char *data, int size, png_bytep base_color);
    Error_T set_qr_angle(const PNG_image_T &src, int qr_width, int size);

private:

    void free_buffers();

private:
    std::vector< png_bytep  > row_pointers;
};





#endif
