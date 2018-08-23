
#include "log.h"
#include <iostream>

void Logger::Clear()
{
    Buf.clear();
    LineOffsets.clear();
}

void Logger::AddLog(int logType, const char *fmt, ...)
{

    int old_size = Buf.size();

    switch (logType) {
    case 0:Buf.appendf("[INFO] ");
        break;
    case 1:Buf.appendf("[WARNING] ");
        break;
    case 2:Buf.appendf("[ERROR] ");
        break;
    }
    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);
    Buf.appendf("\n");
    for (int size = old_size, new_size = Buf.size(); size < new_size; size++)
        if (Buf[size] == '\n')
            LineOffsets.push_back(size);
    ScrollToBottom = true;

    if (logType == 2)
        std::cerr << Buf.begin() + old_size;
    else
        std::cout << Buf.begin() + old_size;

}
void Logger::Draw(const char *title, bool *p_opened)
{
    ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiSetCond_FirstUseEver);
    ImGui::Begin(title, p_opened);
    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0f);
    ImGui::Separator();
    ImGui::BeginChild("scrolling");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
    if (copy) ImGui::LogToClipboard();

    if (Filter.IsActive()) {
        const char *buf_begin = Buf.begin();
        const char *line = buf_begin;
        for (int line_no = 0; line != NULL; line_no++) {
            const char *line_end = (line_no < LineOffsets.Size) ? buf_begin + LineOffsets[line_no] : NULL;
            if (Filter.PassFilter(line, line_end))
                ImGui::TextUnformatted(line, line_end);
            line = line_end && line_end[1] ? line_end + 1 : NULL;
        }
    }
    else {
        ImGui::TextUnformatted(Buf.begin());
    }

    if (ScrollToBottom)
        ImGui::SetScrollHere(1.0f);
    ScrollToBottom = false;
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::End();
}

