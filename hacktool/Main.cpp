// Process.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <Windows.h>
#include <iostream>
#include "source/display/ImGuiDx11App.h"
#include "source/toolmain/Menu.h"



int main() {
    ImGuiDx11App app;
    Menu menu;
    
    if (!app.Initialize(L"Hacker Tools", 1280, 800)) {
        return 1;
    }

    
    app.SetRenderCallback([&menu] {

        menu.show_menu();

        });

    
    app.Run();

    return 0;
}
