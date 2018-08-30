
#include "fileutil.h"
#include <sstream>

std::string readString(const pugi::xml_node &node, std::string def)
{
    std::string val = node.child_value();
    if (val.empty())
        return def;
    return val;
}

int readInt(const pugi::xml_node &node, int def)
{
    const char *str = node.child_value();
    int result = def;
    if (str) {
        std::stringstream s_x(str);
        s_x >> result;
    }
    return result;

}

float readFloat(const pugi::xml_node &node, float def)
{
    const char *str = node.child_value();
    float result = def;
    if (str) {
        std::stringstream s_x(str);
        s_x >> result;
    }
    return result;
}

optix::float3 readVector3(const pugi::xml_node &node, optix::float3 def)
{
    const char *str = node.child_value();
    if (!str)
        return def;

    std::vector<float> v;

    // Build an istream that holds the input string
    std::istringstream iss(str);

    // Iterate over the istream, using >> to grab floats
    // and push_back to store them in the vector
    std::copy(std::istream_iterator<float>(iss),
              std::istream_iterator<float>(),
              std::back_inserter(v));

    if (v.size() != 3)
        return def;

    return optix::make_float3(v[0], v[1], v[2]);

}

optix::Matrix4x4 readTransform(const pugi::xml_node &node)
{
    auto values_node = node.child("values");
    if (values_node){
        std::vector<float> v;

        // Build an istream that holds the input string
        std::istringstream iss(values_node.child_value());

        // Iterate over the istream, using >> to grab floats
        // and push_back to store them in the vector
        std::copy(std::istream_iterator<float>(iss),
                  std::istream_iterator<float>(),
                  std::back_inserter(v));

        if (v.size() != 16)
            return optix::Matrix4x4();

        return optix::Matrix4x4(v.data());
    }
    else {
        optix::float3 scale = readVector3(node.child("scale"), optix::make_float3(1.0f));
        optix::float3 translate = readVector3(node.child("translate"));

        float transformMatrixData[16] =
            {
                scale.x, 0.0f, 0.0f, translate.x,
                0.0f, scale.y, 0.0f, translate.y,
                0.0f, 0.0f, scale.z, translate.z,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        return optix::Matrix4x4(transformMatrixData);
    }
}