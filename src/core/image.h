
#ifndef RENDERER_GPU_IMAGE_H
#define RENDERER_GPU_IMAGE_H

#include <string>

class Image
{
public:
    Image(int mipCount = 1);

    bool load(const std::string &filename);
    bool load(float *data, int width, int height);
    bool write(const std::string &filename);
    void clear();

    float *pixelData() const { return m_pixels; }
    int width() const { return m_width; }
    int height() const { return m_height; };
    int mipCount() const { return m_mipCount; }
private:

    float *m_pixels;
    unsigned char *m_output_pixels;
    int m_width;
    int m_height;

    int m_mipCount;
};


#endif //RENDERER_GPU_IMAGE_H
