
#ifndef RENDERER_GPU_LOG_H
#define RENDERER_GPU_LOG_H

#include <imgui/imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

class Logger
{
public:
    void Clear();
    void AddLog(int logType, const char *fmt, ...);
    void Draw(const char *title, bool *p_opened = nullptr);

    static Logger &getInstance()
    {
        static Logger logger;
        return logger;
    }

private:
    Logger() = default;

    ImGuiTextBuffer Buf;
    ImGuiTextFilter Filter;
    ImVector<int> LineOffsets;        // Index to lines offset
    bool ScrollToBottom;
};

inline std::string string_format(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}

#define LogInfo(...) Logger::getInstance().AddLog(0, __VA_ARGS__)
#define LogWarning(...) Logger::getInstance().AddLog(1, __VA_ARGS__)
#define LogError(...) Logger::getInstance().AddLog(2, __VA_ARGS__)

template<typename TK, typename TV>
std::vector<TK> extract_keys(std::map<TK, TV> const& input_map) {
    std::vector<TK> retval;
    for (auto const& element : input_map) {
        retval.push_back(element.first);
    }
    return retval;
}

template<typename TK, typename TV>
std::vector<TV> extract_values(std::map<TK, TV> const& input_map) {
    std::vector<TV> retval;
    for (auto const& element : input_map) {
        retval.push_back(element.second);
    }
    return retval;
}

inline void MakeUniqueString(std::string &str)
{
    // name -> name (1)
    // name (n) -> name (n+1)

    if (str[str.length() - 1] == ')') {
        for (int i = str.length() - 2; i >= 0; i--)
            if (str[i] == '(') {
                int num = -1;
                try {
                    num = std::atoi(str.substr(i+1, str.length() - i -1).c_str());
                }
                catch(...){}
                if (num != -1) {
                    std::string new_braces = "(" + std::to_string(num + 1) + ")";
                    str.replace(i, str.length() - i, new_braces);
                }

                break;
            }
    }
    else {
        str += " (1)";
    }
}

inline std::string GetUniqueName(const std::vector<std::string> &names, const std::string& inputName)
{
    std::string name = inputName;

    if (name.empty())
        name = "Unknown object";

    if(std::find(names.begin(), names.end(), name) != names.end()) {
        MakeUniqueString(name);
    }

    return name;

}

template <typename T>
std::vector<T> difference(const std::vector<T> &first_v, const std::vector<T> &second_v)
{
    std::vector<T> first = first_v;
    std::vector<T> second = second_v;
    std::vector<T> result;

    std::sort(first.begin(), first.end());
    std::sort(second.begin(), second.end());
    std::set_difference(
        first.begin(), first.end(),
        second.begin(), second.end(),
        std::back_inserter(result)
    );
    return result;
}


#endif //RENDERER_GPU_LOG_H
