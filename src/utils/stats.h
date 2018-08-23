
#ifndef RENDERER_GPU_STATS_H
#define RENDERER_GPU_STATS_H


#include <string>
#include <sstream>
#include <vector>

typedef std::string (*printCallback)();

typedef void (*clearCallback)();

class StatRegisterer
{
public:
    // StatRegisterer Public Methods
    StatRegisterer()
    {

    }
    StatRegisterer(printCallback pfunc, clearCallback cfunc)
    {
        if (!printFuncs)
            printFuncs = new std::vector<printCallback>;
        if (!clearFuncs)
            clearFuncs = new std::vector<clearCallback>;
        printFuncs->push_back(pfunc);
        clearFuncs->push_back(cfunc);
    }

    std::vector<std::string> getStatsString() const
    {
        std::vector<std::string> strs;
        for (auto &func: *printFuncs)
            strs.push_back(func());
        return strs;
    }
    void clearVariables() const
    {
        for (auto &func: *clearFuncs)
            func();
    }

private:
    // StatRegisterer Private Data
    static std::vector<printCallback> *printFuncs;
    static std::vector<clearCallback> *clearFuncs;
};

std::vector<std::string> GetStats();
void ClearStats();


// cleared each time frame is rendered
#define REGISTER_DYNAMIC_STATISTIC(type, name, value, description)                  \
    static type name = value;                                               \
    static StatRegisterer STATS_REG_##name(                                 \
            []()->std::string {std::stringstream ss; ss << description << ": " << name << "\n"; return ss.str();}, \
            []() {name = value;})

// never cleared
#define REGISTER_PERMANENT_STATISTIC(type, name, value, description)                    \
    static type name = value;                                                           \
    static StatRegisterer STATS_REG_##name(                                             \
            []()->std::string {std::stringstream ss; ss << description << ": " << name << "\n"; return ss.str();}, \
            []() {})

#endif //RENDERER_GPU_STATS_H
