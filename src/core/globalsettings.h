
#ifndef RENDERER_GPU_GLOBALSETTINGS_H
#define RENDERER_GPU_GLOBALSETTINGS_H


#include <pugixml.hpp>

class GlobalSettings
{
public:
    int worldForwardAxis = 2;


    void load(const pugi::xml_node &node);
    static GlobalSettings& getInstance();

private:
    GlobalSettings();


};


#endif //RENDERER_GPU_GLOBALSETTINGS_H
