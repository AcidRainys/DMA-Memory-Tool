# DMA Memory Tool

> DMA（直接内存访问）工具
利用直接内存访问技术来获取目标机器进程的内存数据。


## 功能特性
- 💾 进程枚举以及内存区域转储 
- 📊 可视化的设计以及优雅的LOG
## 适用场景
- 🔍 可以在您没有驱动又由于目标进程有保护导致无法Dump内存的情况下使用
- 🛡️ 安全性 无感知

![项目截图](image/2025-06-18 015436.png) <!-- 替换为你的实际图片路径 -->

![项目截图](image/2025-06-18 015448.png) <!-- 替换为你的实际图片路径 -->


## 技术栈
```bash
C++20 or later versions
```

##快速开始
编译要求

```bash
Visual Studio 2022
Windows 11 SDK (10.0.22000.0)
```

##安装步骤
克隆仓库：

```bash
git clone https://github.com/yourusername/DMA-Memory-Tool.git
```

项目结构
```info
DMA-Memory-Tool/
├──hacktool
    ├── source/                  # 用户界面
    │   ├── toolmain             # 主窗口逻辑
    │   └── display/             # Dx11 资源文件
    ├── support/                 # 支持文件
    │   ├── ImGui
    │   ├── Memory
    │
    └──  # Visual Studio 解决方案
├── README.md                    # 本文件
└── Process.sln
```

学分与致谢
本项目部分实现参考/使用了以下资源：

DMALibrary: https://github.com/Metick/DMALibrary
MemProcFS: https://github.com/ufrisk/MemProcFS
Dear ImGui: https://github.com/ocornut/imgui
DumpMemoryFunction: https://github.com/idkfrancis/DMA-ProcessDumper

许可协议
本项目采用 Apache 2.0 许可证 - 详情见 LICENSE 文件