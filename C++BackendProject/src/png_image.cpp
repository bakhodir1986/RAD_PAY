#include <cstring> // memset
#include <cstdlib>
#include <memory>


#include <new> // std::bad_alloc

#include "png_image.h"


PNG_image_T::PNG_image_T() :  width(0), height(0), color_type(), bit_depth(), row_pointers()
{

}

PNG_image_T::~PNG_image_T()
{
    free_buffers();
}

Error_T PNG_image_T::init_image(int w, int h, png_bytep bg_color)
{
    width = w;
    height = h;
    
    row_pointers.resize(  height  ); 
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(4 * width);
        if( ! row_pointers[y]){ free_buffers(); throw std::bad_alloc();}
        
        if ( width > 0 )
            memset(row_pointers[y], 0xFF, 4 * width);
    }
    return Error_OK ;
}

Error_T PNG_image_T::read_file(const std::string &filename)
{
    free_buffers();

    FILE *fp = fopen(filename.c_str(), "rb");
    if(!fp) {
       // slog.WarningLog("Can't open file: \"%s\"", filename.c_str());
        return Error_internal;
    }
    
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        fclose(fp);
        //slog.ErrorLog("Failed to create png read structure");
        return Error_internal;
    }

    png_infop info = png_create_info_struct(png);
    if(!info)  {
        png_destroy_read_struct(&png, (png_infopp)(NULL), (png_infopp)(NULL));
        fclose(fp);
        //slog.ErrorLog("Failed to create png info structure");
        return Error_internal;
    }

    if(setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, (png_infopp)(NULL));
        fclose(fp);
        //slog.ErrorLog("setjmp called");
        return Error_internal;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    width      = png_get_image_width(png, info);
    height     = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth  = png_get_bit_depth(png, info);

    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if(color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    row_pointers.resize( height ); 
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
        if( ! row_pointers[y]){ 
            free_buffers(); 
            png_destroy_read_struct(&png, &info, (png_infopp)(NULL));
            fclose(fp);
            throw std::bad_alloc();
        }
    }

    png_read_image(png, row_pointers.data());
    
    png_destroy_read_struct(&png, &info, (png_infopp)(NULL));
    fclose(fp);
    return Error_OK;
}

Error_T PNG_image_T::write_file(const std::string &filename)
{
    FILE *fp = fopen(filename.c_str(), "wb");
    if(!fp) {
        //slog.WarningLog("Can't open file: \"%s\"", filename.c_str());
        return Error_internal;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        fclose(fp);
        //slog.ErrorLog("Failed to create png write structure");
        return Error_internal;
    }

    png_infop info = png_create_info_struct(png);
    if(!info)  {
        png_destroy_write_struct(&png, (png_infopp)0);
        fclose(fp);
        //slog.ErrorLog("Failed to create png info structure");
        return Error_internal;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
       // slog.ErrorLog("setjmp called");
        return Error_internal;
    }

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
        png,
        info,
        width, height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
    // Use png_set_filler().
    //png_set_filler(png, 0, PNG_FILLER_AFTER);

    png_write_image(png, row_pointers.data());
    png_write_end(png, NULL);

    png_destroy_write_struct(&png, &info);
        
    fclose(fp);

    return Error_OK;
}

Error_T PNG_image_T::add_top_image(const PNG_image_T &src, int method)
{
    int cor_y = (height - src.height)/ 2;
    int cor_x = (width - src.width) / 2;
    
    for(int y = 0; y <  src.height; ++y) {
        png_bytep row = row_pointers[ y + cor_y];
        memcpy(row + cor_x * 4, src.row_pointers[y], sizeof(png_byte) * 4 * src.width);
    }

    return Error_OK;
}

Error_T PNG_image_T::fill_qr(int qr_width, const unsigned char *data, int size, png_bytep base_color)
{
    int margin = 4;
    
    static const
    int space[8][8] = {
        { 100, 90, 40, 10, 10, 40, 90, 100 },
        {  90, 10,  0,  0,  0,  0, 10,  90 },
        {  40,  0,  0,  0,  0,  0,  0,  40 },
        {  10,  0,  0,  0,  0,  0,  0,  10 },
        {  10,  0,  0,  0,  0,  0,  0,  10 },
        {  40,  0,  0,  0,  0,  0,  0,  40 },
        {  90, 10,  0,  0,  0,  0, 10,  90 },
        { 100, 90, 40, 10, 10, 40, 90, 100 }
    };

    if( row_pointers.empty() )
        init_image((qr_width + margin*2) * size, (qr_width + margin*2) * size, NULL);

    for(int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for(int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);

            int myy = y / size - margin;
            int myx = x / size - margin;
            if (myx >= 0 && myx < qr_width
             && myy >= 0 && myy < qr_width) {

                int a = (*(data + myy * qr_width + myx)  & 1);
                if(a == 1) {
                    int modx = x % size;
                    int mody = y % size;

                    int v = space[modx][mody];
                    px[0] = base_color[0] + (255 - base_color[0]) * v / 100;
                    px[1] = base_color[1] + (255 - base_color[1]) * v / 100;
                    px[2] = base_color[2] + (255 - base_color[2]) * v / 100;
                    px[3] = base_color[3];

                }
            }
        }
    }

    return Error_OK;
}

Error_T PNG_image_T::set_qr_angle(const PNG_image_T &src, int qr_width, int size)
{
    int oy = 0;
    int margin = 4;

    for(int y = margin * size; y < (margin + 7) * size ; y++) {
        png_bytep row = row_pointers[y];
        memcpy(row + margin * size * 4, src.row_pointers[oy], sizeof(png_byte) * 4 * src.width / 2);
        memcpy(row + width * 4 - (margin + 7) * size * 4,
            src.row_pointers[oy] + sizeof(png_byte) * 4 * src.width / 2,
            sizeof(png_byte) * 4 * src.width / 2);
        oy++;
    }
    for(int y = height - (margin + 7) * size; y < height - margin * size ; y++) {
        png_bytep row = row_pointers[y];
        memcpy(row + margin * size * 4, src.row_pointers[oy], sizeof(png_byte) * 4 * src.width / 2);
        oy++;
    }

    return Error_OK;
}

void PNG_image_T::free_buffers()
{
    if( ! row_pointers.empty() ) {
        for(int y = 0; y < height; y++) {
            free(row_pointers[y]);
        }
        row_pointers.clear();
         
        width = 0;
        height = 0;
    }
}
