
#include "stats.h"
#include <iostream>

std::vector<printCallback> *StatRegisterer::printFuncs;
std::vector<clearCallback> *StatRegisterer::clearFuncs;

static StatRegisterer registerer;

std::vector<std::string> GetStats()
{
    return registerer.getStatsString();
}

void ClearStats()
{
    registerer.clearVariables();
}
