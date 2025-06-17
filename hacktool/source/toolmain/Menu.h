#pragma once
#include <utility>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include "imgui.h"

typedef unsigned long DWORD;
class ImGuiTableSortSpecs;

class Menu {
public:
    Menu();
    ~Menu() = default;

    void show_menu();

private:
    void RefreshProcessList();
    void FilterProcesses();
    bool isProcessAttached() const;

    
    char m_cProcessSearchBuffer[256] = "";
    bool m_bExcludeSystemProcesses = true;
    DWORD m_iSelectedPid = 0;
    DWORD m_attachedPid = 0;
    std::string m_cSelectedName = "NULL";
    std::vector<std::pair<DWORD, std::string>> m_vAllProcesses;
    std::vector<std::pair<DWORD, std::string>> m_vFilteredProcesses;
    bool needRefreshProcessList = true;

    


    char moduleSearchBuffer[256] = "";
    bool excludeSystemModules = true;
    std::vector<bool> moduleSelectionStates;
    bool showModuleList = false;
    static const std::unordered_set<std::string> systemModules;



};