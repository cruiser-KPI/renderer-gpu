
#include "globalsettings.h"

#include "../utils/fileutil.h"

GlobalSettings &GlobalSettings::getInstance()
{
    static GlobalSettings gs;
    return gs;
}


GlobalSettings::GlobalSettings()
{

}

void GlobalSettings::load(const pugi::xml_node &node)
{
    worldForwardAxis = readInt(node.child("forward_axis"), 2);
}
