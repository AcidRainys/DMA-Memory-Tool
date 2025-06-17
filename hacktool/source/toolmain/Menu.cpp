#include "Menu.h"
#include "display/ImGuiDx11App.h"
#include "Memory/Memory.h"
#include "Hacker/HackTools.h"
#include <thread>
#include <cctype>


const std::unordered_set<std::string> Menu::systemModules = {
    "ntdll.dll", "kernel32.dll", "kernelbase.dll", "user32.dll",
    "gdi32.dll", "advapi32.dll", "msvcrt.dll", "ws2_32.dll",
    "rpcrt4.dll", "ole32.dll", "combase.dll", "shcore.dll",
    "shell32.dll", "ucrtbase.dll", "win32u.dll", "imm32.dll",
    "bcrypt.dll", "crypt32.dll", "sechost.dll", "oleaut32.dll",
    "WOW64win.dll", "WOW64cpu.dll", "WOW64con.dll", "WOW64base.dll",
    "WOW64.dll", "wintypes.dll", "windows.storage.dll", "msvcp_win.dll",
    "WINTRUST.dll", "WINMM.dll"
};

Menu::Menu() = default;

bool Menu::isProcessAttached() const {
    return m_attachedPid != 0 && m_iSelectedPid == m_attachedPid;
}

void Menu::show_menu() {

    
    ImGui::Begin("HackerTool", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BeginChild("child", ImVec2(700, 500));
    if (ImGui::BeginTabBar("TabMain", ImGuiTabBarFlags_None)) {
        
        if (ImGui::BeginTabItem("Process", nullptr, ImGuiTabItemFlags_None)) {
            // 刷新列表
            if (needRefreshProcessList) {
                RefreshProcessList();
                needRefreshProcessList = false;
            }

            ImGui::Spacing();
            ImGui::Text("Select Process:");


            std::string comboLabel = "Process List";
            if (m_iSelectedPid != 0) {
                auto it = std::find_if(m_vFilteredProcesses.begin(), m_vFilteredProcesses.end(),
                    [&](const auto& p) { return p.first == m_iSelectedPid; });
                if (it != m_vFilteredProcesses.end()) {
                    comboLabel = it->second + " (PID: " + std::to_string(it->first) + ")";
                }
            }

            
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##ProcessCombo", comboLabel.c_str())) {
                
                ImGui::PushItemWidth(-1);
                if (ImGui::InputTextWithHint("##ComboSearch", "Search...",
                    m_cProcessSearchBuffer, IM_ARRAYSIZE(m_cProcessSearchBuffer))) {
                    FilterProcesses();
                }
                ImGui::PopItemWidth();

                
                if (ImGui::Button("Refresh")) {
                    needRefreshProcessList = true;
                }

                // 过滤系统进程
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                if (ImGui::Checkbox("Exclude System", &m_bExcludeSystemProcesses)) {
                    FilterProcesses();
                }

                // 进程列表
                ImGui::BeginChild("##ProcessList", ImVec2(0, 200), true);
                for (size_t i = 0; i < m_vFilteredProcesses.size(); i++) {
                    const auto& [pid, name] = m_vFilteredProcesses[i];
                    std::string displayName = name + " (PID: " + std::to_string(pid) + ")";

                    bool isSelected = (m_iSelectedPid == pid);
                    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                        
                        m_iSelectedPid = pid;

                        
                        if (m_attachedPid != 0 && pid != m_attachedPid) {
                            showModuleList = false;
                        }
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndChild();

                ImGui::EndCombo();
            }

            // 显示选中进程信息
            if (m_iSelectedPid != 0) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Selected Process:");
                ImGui::Text("PID:     %d", m_iSelectedPid);

                auto it = std::find_if(m_vAllProcesses.begin(), m_vAllProcesses.end(),
                    [&](const auto& p) { return p.first == m_iSelectedPid; });
                if (it != m_vAllProcesses.end()) {
                    m_cSelectedName = it->second;
                    ImGui::Text("Name: ");
                    ImGui::SameLine();
                    ImGui::TextColored(isProcessAttached() ? ImColor(10, 200, 10) : ImColor(200, 10, 10),
                        " %s", it->second.c_str());
                    ImGui::Text("IsAttached: ");
                    ImGui::SameLine();
                    ImGui::TextColored(isProcessAttached() ? ImColor(10, 200, 10) : ImColor(200, 10, 10),
                        " %s", isProcessAttached() ? "TRUE" : "FALSE");
                }

                
                if (ImGui::Button("Attach to Process")) {
                    if (Hacker.InitiationProcess(m_cSelectedName.c_str())) {
                        m_attachedPid = m_iSelectedPid;
                        showModuleList = false;
                        LOG_INFO("Successfully attached to process: " + std::to_string(m_iSelectedPid));
                    }
                    else {
                        m_attachedPid = 0;
                        LOG_ERROR("Failed to attach to process: " + std::to_string(m_iSelectedPid));
                    }
                }
            }

            ImGui::EndTabItem();
        }

        
        if (ImGui::BeginTabItem("Memory", nullptr, ImGuiTabItemFlags_None)) {
            bool hasSelected = m_cSelectedName.find("NULL") == std::string::npos;

            ImGui::Text("Current Selection: ");
            ImGui::SameLine();
            if (!hasSelected) {
                ImGui::TextColored(ImColor(200, 10, 10), "No process selected");
            }
            else if (!isProcessAttached()) {
                ImGui::TextColored(ImColor(200, 100, 10), "%s (Not attached)", m_cSelectedName.c_str());
            }
            else {
                ImGui::TextColored(ImColor(10, 200, 10), "%s (Attached)", m_cSelectedName.c_str());
            }

            if (hasSelected && isProcessAttached()) {
                ImGui::Text("Base Address: 0x%llx", Hacker.GetBaseAddress());
                ImGui::SameLine(0.0f, 50.0f);
                ImGui::Text("Base Size: %zu KB", Hacker.GetBaseSize() / 1024);

                if (ImGui::Button("Dump EXE")) {
                    std::thread(Hacker.DumpTargetProcess).detach();
                }
                ImGui::SameLine();

                auto moduleList = Hacker.GetModuleList();
                if (ImGui::Button("Get Module List")) {
                    std::thread(Hacker.GetTargetAllModule).detach();
                    showModuleList = true;
                    moduleSelectionStates.clear();
                }

                if (showModuleList && !moduleList.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // 模块搜索,过滤
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                    if (ImGui::InputTextWithHint("##ModuleSearch", "Search modules...",
                        moduleSearchBuffer, IM_ARRAYSIZE(moduleSearchBuffer))) {
                    }

                    ImGui::SameLine();
                    ImGui::Checkbox("Exclude System DLLs", &excludeSystemModules);

                    
                    if (moduleSelectionStates.size() != moduleList.size()) {
                        moduleSelectionStates.resize(moduleList.size(), false);
                    }

                    // 表格
                    if (ImGui::BeginTable("ModuleTable", 4,
                        ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {

                        // 表头
                        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
                        ImGui::TableSetupColumn("Base Address", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 1);
                        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 2);
                        ImGui::TableHeadersRow();

                        // 过滤后的模块
                        std::vector<size_t> filteredIndices;
                        for (size_t i = 0; i < moduleList.size(); i++) {
                            const auto& module = moduleList[i];

                            // 排除系统模块
                            if (excludeSystemModules &&
                                systemModules.find(module.module_name) != systemModules.end()) {
                                continue;
                            }

                            // 搜索过滤
                            if (moduleSearchBuffer[0] != '\0') {
                                std::string modLower = module.module_name;
                                std::transform(modLower.begin(), modLower.end(), modLower.begin(),
                                    [](unsigned char c) { return std::tolower(c); });

                                std::string searchLower = moduleSearchBuffer;
                                std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                                    [](unsigned char c) { return std::tolower(c); });

                                if (modLower.find(searchLower) == std::string::npos) {
                                    continue;
                                }
                            }

                            filteredIndices.push_back(i);
                        }

                        // 模块列表
                        for (size_t idx : filteredIndices) {
                            const auto& module = moduleList[idx];

                            ImGui::TableNextRow();

                            // 选择列
                            ImGui::TableSetColumnIndex(0);
                            bool temp = moduleSelectionStates[idx];
                            ImGui::Checkbox(("##Select" + std::to_string(idx)).c_str(),
                                &temp);
                            moduleSelectionStates[idx] = temp;
                            // 模块名称
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%s", module.module_name.c_str());

                            // 基地址
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("0x%llX", module.base_address);

                            // 大小
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%zu KB", module.base_size / 1024);
                        }

                        ImGui::EndTable();
                    }

                    
                    if (ImGui::Button("Dump Selected Modules")) {
                        std::vector<HackTools::ProcessModuleINFO> selectedModules;
                        for (size_t i = 0; i < moduleList.size(); i++) {
                            if (moduleSelectionStates[i]) {
                                selectedModules.push_back(moduleList[i]);
                            }
                        }

                        if (!selectedModules.empty()) {
                            std::thread(Hacker.DumpTargetModules, selectedModules).detach();
                        }
                        else {
                            LOG_WARN("No modules selected for dumping");
                        }
                    }
                }



            }
            ImGui::EndTabItem();
        }

        
        if (ImGui::BeginTabItem("Setting", nullptr, ImGuiTabItemFlags_None)) {
            
            float samples[165];
            for (int n = 0; n < 165; n++)
                samples[n] = sinf(n * 0.2f + ImGui::GetTime() * 1.5f);
            ImGui::PlotLines("Frame Count", samples, 165);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::End();
}








void Menu::RefreshProcessList() {
    m_vAllProcesses.clear();
    Hacker.GetAllProcess();

    // 获取进程ID列表
    std::vector<DWORD> pidList = Hacker.GetPidList();
    const auto& pidNameMap = Hacker.GetPidFormName();

    // 构建进程列表
    for (DWORD pid : pidList) {
        auto it = pidNameMap.find(pid);
        if (it != pidNameMap.end() && !it->second.empty()) {
            m_vAllProcesses.emplace_back(pid, it->second);
        }
    }

    // 按进程名称排序
    std::sort(m_vAllProcesses.begin(), m_vAllProcesses.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // 应用过滤
    FilterProcesses();
#ifdef _DEBUG
    LOG_INFO("Refreshed process list, found " + std::to_string(m_vAllProcesses.size()) + " processes");
#endif // _DEBUG_


}

void Menu::FilterProcesses() {
    m_vFilteredProcesses.clear();

    // 准备搜索文本（小写）
    std::string searchLower = m_cProcessSearchBuffer;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // 系统进程列表
    static const std::unordered_set<std::string> systemProcesses = {
        "System", "smss.exe", "csrss.exe", "wininit.exe", "services.exe",
        "lsass.exe", "winlogon.exe", "svchost.exe", "dwm.exe", "taskhostw.exe"
    };

    for (const auto& [pid, name] : m_vAllProcesses) {
        // 排除系统进程
        if (m_bExcludeSystemProcesses &&
            systemProcesses.find(name) != systemProcesses.end()) {
            continue;
        }

        // 应用搜索过滤
        if (!searchLower.empty()) {
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                [](unsigned char c) { return std::tolower(c); });

            std::string pidStr = std::to_string(pid);
            if (nameLower.find(searchLower) == std::string::npos &&
                pidStr.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        m_vFilteredProcesses.push_back({ pid, name });
    }
}