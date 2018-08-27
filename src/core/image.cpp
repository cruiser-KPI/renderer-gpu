
#include "image.h"

#include "../math/basic.h"
#include "../utils/log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>


#include <fstream>

Image::Image(int mipCount)
    : m_pixels(nullptr), m_output_pixels(nullptr), m_width(0), m_height(0), m_mipCount(mipCount)
{

}

int nextPower2(int value)
{
    return (int)pow(2, ceil(log(value)/log(2)));
}

bool Image::load(const std::string &filename)
{
    stbi_ldr_to_hdr_gamma(2.2f);

    float *data = stbi_loadf(filename.c_str(), &m_width, &m_height, nullptr, 4);
    if (!data){
        LogError("Couln't load image file: '%s'", filename.c_str());
        return false;
    }

    int currWidth = nextPower2(m_width);
    int currHeight = nextPower2(m_height);

    int pixelCount = (2 - 1 / (int) pow(2, m_mipCount -1 ));
    pixelCount = pixelCount * pixelCount * currWidth  * currHeight * 4;
    m_pixels = new float[pixelCount];

    if (m_width != currWidth || m_height != currHeight) {
        stbir_resize_float(data, m_width, m_height, 0, m_pixels, currWidth, currHeight, 0, 4);
        m_width = currWidth;
        m_height = currHeight;
    }
    else
        memcpy(m_pixels, data, m_width * m_height * 4 * sizeof(float));

    // TODO get rid of negative values when upsampling and downsampling
    for (int j = 0; j < m_height; j++)
        for (int i = 0; i < m_width*4; i++)
            if (m_pixels[j*m_width*4 + i] < 0.0f)
                m_pixels[j*m_width*4 + i] = 0.0f;


    int offset = currHeight * currWidth * 4;
    for (int level = 1; level < m_mipCount; level++)
    {
        currWidth /= 2;
        currHeight /= 2;

        if (currWidth == 0 || currHeight == 0){
            m_mipCount = level;
            break;
        }

        stbir_resize_float(m_pixels + offset, 2*currWidth, 2*currHeight, 0,
            m_pixels + offset + currWidth * currHeight * 4, currWidth, currHeight, 0, 4);
        offset += currWidth * currHeight * 4;
    }
    stbi_image_free(data);

    LogInfo("Image '%s' was loaded. Resolution: %dx%d, %d mipmaps",
        filename.c_str(), m_width, m_height, m_mipCount);

    return true;
}

bool Image::load(float *data, int width, int height)
{
    m_pixels = data;
    m_width = width;
    m_height = height;
    return true;
}

bool Image::write(const std::string &filename)
{
    std::ofstream imageFile (filename, std::ios::out | std::ios::binary);
    imageFile.write ((const char*)m_pixels, sizeof(float) * 4 * m_width * m_height);
    imageFile.close();

    m_pixels = nullptr;
    m_width = 0;
    m_height = 0;
    return true;
}

void Image::clear()
{
    if (m_pixels)
        delete [] m_pixels;
    m_width = 0;
    m_height = 0;
}

