#include "pch.h"
#include "Memory.h"
#include <thread>
#include <iostream>
#include <processthreadsapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include "MemoryModule.h"
#include "library/dll.h"

inline HMODULE LoadResourceEx(LPCWSTR lpName, LPCWSTR lpType) {
	HRSRC Res = FindResourceW(nullptr, lpName, lpType);
	if (!Res)
		return nullptr;
	return (HMODULE)LoadResource(nullptr, Res);
}


Memory::Memory()
{
	LOG_INFO("加载动态库中...");
	modules.VMM = LoadLibraryA("vmm.dll");
	modules.FTD3XX = LoadLibraryA("FTD3XX.dll");
	modules.LEECHCORE = LoadLibraryA("leechcore.dll");
	//modules.VMM = LoadResourceEx(MAKEINTRESOURCEW(IDR_VMM2), L"VMM");
    //modules.FTD3XX = LoadResourceEx(MAKEINTRESOURCEW(IDR_FTD3XX2), L"FTD3XX");
    //modules.LEECHCORE = LoadResourceEx(MAKEINTRESOURCEW(IDR_LEECHCORE2), L"LEECHCORE");
	//modules.FTD3XX = (HMODULE)MemoryLoadLibrary((void*)FTD3XX, FTD3XX_size);
    //modules.VMM = (HMODULE)MemoryLoadLibrary((void*)vmm, vmm_size);
    //modules.LEECHCORE = (HMODULE)MemoryLoadLibrary((void*)leechcore, leechcore_size);

	if (!modules.VMM || !modules.FTD3XX || !modules.LEECHCORE)
	{
		LOG_INFO("vmm: %p", modules.VMM);
		LOG_INFO("ftd: %p", modules.FTD3XX);
		LOG_INFO("leech: %p", modules.LEECHCORE);
	}

	this->key = std::make_shared<c_keys>();
}



Memory::~Memory()
{
	////MemoryFreeLibrary(modules.VMM);
   // MemoryFreeLibrary(modules.FTD3XX);
   // MemoryFreeLibrary(modules.LEECHCORE);
	VMMDLL_Close(this->vHandle);
	VMMDLL_CloseAll();
	this->current_process = CurrentProcessInformation();
	DMA_INITIALIZED = false;
	PROCESS_INITIALIZED = false;
}
void Memory::Release()
{
	this->current_process = CurrentProcessInformation();
	DMA_INITIALIZED = false;
	PROCESS_INITIALIZED = false;
}
bool Memory::DumpMemoryMap(bool debug)
{
	LPCSTR args[] = {const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>("")};
	int argc = 3;
	if (debug)
	{
		args[argc++] = const_cast<LPCSTR>("-v");
		args[argc++] = const_cast<LPCSTR>("-printf");
	}

	VMM_HANDLE handle = VMMDLL_Initialize(argc, args);
	if (!handle)
	{
		LOG_ERROR("Failed to open a VMM Handle\n");
		return false;
	}

	PVMMDLL_MAP_PHYSMEM pPhysMemMap = NULL;
	if (!VMMDLL_Map_GetPhysMem(handle, &pPhysMemMap))
	{
		LOG_ERROR("Failed to get physical memory map\n");
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->dwVersion != VMMDLL_MAP_PHYSMEM_VERSION)
	{
		LOG_ERROR("Invalid VMM Map Version\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->cMap == 0)
	{
		LOG_ERROR("Failed to get physical memory map\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}
	//Dump map to file
	std::stringstream sb;
	for (DWORD i = 0; i < pPhysMemMap->cMap; i++)
	{
		sb << std::hex << pPhysMemMap->pMap[i].pa << " " << (pPhysMemMap->pMap[i].pa + pPhysMemMap->pMap[i].cb - 1) << std::endl;
	}

	auto temp_path = std::filesystem::temp_directory_path();
	std::ofstream nFile(temp_path.string() + "\\mmap.txt");
	nFile << sb.str();
	nFile.close();

	VMMDLL_MemFree(pPhysMemMap);
	LOG_INFO("Successfully dumped memory map to file!\n");
	//Little sleep to make sure it's written to file.
	Sleep(3000);
	VMMDLL_Close(handle);
	return true;
}
unsigned char abort2[4] = {0x10, 0x00, 0x10, 0x00};

bool Memory::SetFPGA()
{
	ULONG64 qwID = 0, qwVersionMajor = 0, qwVersionMinor = 0;
	if (!VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &qwID) && VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MAJOR, &qwVersionMajor) && VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MINOR, &qwVersionMinor))
	{
		LOG_ERROR("无法查找 FPGA 设备，正在尝试继续\n\n");
		return false;
	}

	LOG_INFO("[+] VMMDLL_ConfigGet");
	LOG_INFO(" ID = %lli", qwID);
	LOG_INFO(" 版本 = %lli.%lli\n", qwVersionMajor, qwVersionMinor);

	if ((qwVersionMajor >= 4) && ((qwVersionMajor >= 5) || (qwVersionMinor >= 7)))
	{
		HANDLE handle;
		LC_CONFIG config = {.dwVersion = LC_CONFIG_VERSION, .szDevice = "existing"};
		handle = LcCreate(&config);
		if (!handle)
		{
			LOG_ERROR("无法创建FPGA设备\n");
			return false;
		}

		LcCommand(handle, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, reinterpret_cast<PBYTE>(&abort2), NULL, NULL);
		LOG_INFO("注册已自动清除\n");
		LcClose(handle);
	}

	return true;
}

bool Memory::Init(std::string process_name, bool memMap, bool debug)
{
	if (PROCESS_INITIALIZED)
	{
		LOG_INFO("进程已初始化！\n");
		return true;
	}

	current_process.PID = GetPidFromName(process_name);
	if (!current_process.PID)
	{
		LOG_ERROR("[!] 无法从 进程名 获取 进程PID！\n");
		return false;
	}
	current_process.process_name = process_name;
	if (!this->FixCr3())
	{
		LOG_ERROR("[!] CR3修复失败 修复重启中 \n");
	}
	current_process.base_address = GetBaseDaddy(process_name);
	if (!current_process.base_address)
	{
		LOG_ERROR("[!] 无法获取基址！\n");
		return false;
	}

	current_process.base_size = GetBaseSize(process_name);
	if (!current_process.base_size)
	{
		LOG_ERROR("[!] 无法获取基址大小\n");
		return false;
	}
#ifdef _DEBUG
	LOG_INFO("Process information of %s\n", process_name.c_str());
	LOG_INFO("PID: %i\n", current_process.PID);
	LOG_INFO("Base Address: 0x%llx\n", current_process.base_address);
	LOG_INFO("Base Size: 0x%llx\n", current_process.base_size);
#else
	LOG_INFO("进程PID: %i\n", current_process.PID);
#endif // DEBUG
	

	PROCESS_INITIALIZED = TRUE;

	return true;
}

bool Memory::FPGAINIT(bool memMap, bool debug)
{
	if (!DMA_INITIALIZED)
	{
		LOG_INFO("初始化中...\n");
	reinit:
		LPCSTR args[] = { const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>("") };
		DWORD argc = 3;
		if (debug)
		{
			args[argc++] = const_cast<LPCSTR>("-v");
			args[argc++] = const_cast<LPCSTR>("-printf");
		}

		std::string path = "";
		if (memMap)
		{
			auto temp_path = std::filesystem::temp_directory_path();
			path = (temp_path.string() + "\\mmap.txt");
			bool dumped = false;
			if (!std::filesystem::exists(path))
				dumped = this->DumpMemoryMap(debug);
			else
				dumped = true;
			LOG_INFO("dumping memory map to file...\n");
			if (!dumped)
			{
				LOG_ERROR("[!] ERROR: Could not dump memory map!\n");
				LOG_ERROR("Defaulting to no memory map!\n");
			}
			else
			{
				LOG_INFO("Dumped memory map!\n");

				//Add the memory map to the arguments and increase arg count.
				args[argc++] = const_cast<LPSTR>("-memmap");
				args[argc++] = const_cast<LPSTR>(path.c_str());
			}
		}
		this->vHandle = VMMDLL_Initialize(argc, args);
		if (!this->vHandle)
		{
			if (memMap)
			{
				memMap = false;
				LOG_ERROR("[!] 初始化失败，并显示 Memory map？不使用 MMap 试用\n");
				goto reinit;
			}
			LOG_ERROR("[!] 初始化失败 你的DMA连接了吗？\n");
			return false;
		}

		ULONG64 FPGA_ID = 0, DEVICE_ID = 0;

		VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &FPGA_ID);
		VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_DEVICE_ID, &DEVICE_ID);

		LOG_INFO("FPGA ID: %llu\n", FPGA_ID);
		LOG_INFO("设备 ID: %llu\n", DEVICE_ID);
		LOG_INFO("成功!\n");

		if (!this->SetFPGA())
		{
			LOG_ERROR("[!] 无法设置 FPGA！\n");
			VMMDLL_Close(this->vHandle);
			return false;
		}

		DMA_INITIALIZED = TRUE;
	}
	else
		LOG_INFO("DMA 已初始化！\n");
	return true;
}

DWORD Memory::GetPidFromName(std::string process_name)
{
	DWORD pid = 0;
	VMMDLL_PidGetFromName(this->vHandle, (LPSTR)process_name.c_str(), &pid);
	return pid;
}



std::vector<DWORD> Memory::GetPidList()
{
	std::vector<DWORD> pidList;
	SIZE_T cPids = 0;
	if (!VMMDLL_PidList(this->vHandle, NULL, &cPids) || (cPids == 0)) {
		pidList.clear();
		return pidList;
	}

	// 2. 分配足够的内存来存储PID列表
	pidList.resize(cPids);

	// 3. 实际获取PID列表
	SIZE_T cReturnedPids = cPids; // 传入缓冲区大小
	if (!VMMDLL_PidList(this->vHandle, pidList.data(), &cReturnedPids)) {
		pidList.clear();
		return pidList;
	}

	
	if (cReturnedPids > cPids) {
		pidList.clear();
		return pidList;
	}
	return pidList;
}

std::vector<int> Memory::GetPidListFromName(std::string name)
{
	PVMMDLL_PROCESS_INFORMATION process_info = NULL;
	DWORD total_processes = 0;
	std::vector<int> list = { };

	if (!VMMDLL_ProcessGetInformationAll(this->vHandle, &process_info, &total_processes))
	{
		LOG_ERROR("[!] Failed to get process list\n");
		return list;
	}

	for (size_t i = 0; i < total_processes; i++)
	{
		auto process = process_info[i];
		if (strstr(process.szNameLong, name.c_str()))
			list.push_back(process.dwPID);
	}

	return list;
}

std::vector<std::string> Memory::GetModuleList(std::string process_name)
{
	std::vector<std::string> list = { };
	PVMMDLL_MAP_MODULE module_info = NULL;
	if (!VMMDLL_Map_GetModuleU(this->vHandle, current_process.PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		LOG_ERROR("[!] Failed to get module list\n");
		return list;
	}

	for (size_t i = 0; i < module_info->cMap; i++)
	{
		auto module = module_info->pMap[i];
		list.push_back(module.uszText);
	}

	return list;
}

VMMDLL_PROCESS_INFORMATION Memory::GetProcessInformation()
{
	VMMDLL_PROCESS_INFORMATION info = { };
	SIZE_T process_information = sizeof(VMMDLL_PROCESS_INFORMATION);
	ZeroMemory(&info, sizeof(VMMDLL_PROCESS_INFORMATION));
	info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
	info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

	if (!VMMDLL_ProcessGetInformation(this->vHandle, current_process.PID, &info, &process_information))
	{
		LOG_ERROR("[!] Failed to find process information\n");
		return { };
	}

	LOG_INFO("[+] Found process information\n");
	return info;
}
VMMDLL_PROCESS_INFORMATION Memory::GetProcessInformationFormPid(DWORD pid)
{
	VMMDLL_PROCESS_INFORMATION info = { };
	SIZE_T process_information = sizeof(VMMDLL_PROCESS_INFORMATION);
	ZeroMemory(&info, sizeof(VMMDLL_PROCESS_INFORMATION));
	info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
	info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

	if (!VMMDLL_ProcessGetInformation(this->vHandle, pid, &info, &process_information))
	{
		LOG_ERROR("[!] Failed to find process information\n");
		return { };
	}

	// LOG_INFO("[+] Found process information\n");
	return info;
}


PEB Memory::GetProcessPeb()
{
	auto info = GetProcessInformation();
	if (info.win.vaPEB)
	{
		LOG_ERROR("[+] Found process PEB ptr at 0x%llx\n", (int64_t)(info.win.vaPEB));
		return Read<PEB>(info.win.vaPEB);
	}
	LOG_ERROR("[!] Failed to find the processes PEB\n");
	return { };
}

size_t Memory::GetBaseDaddy(std::string module_name)
{
	std::wstring str(module_name.begin(), module_name.end());

	PVMMDLL_MAP_MODULEENTRY module_info;
	if (!VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		LOG_ERROR("[!] Couldn't find Base Address for %s\n", module_name.c_str());
		return 0;
	}
#ifdef DEBUG
	LOG_INFO("[+] Found Base Address for %s at 0x%llx\n", module_name.c_str(), (int64_t)(module_info->vaBase));
#endif // DEBUG

	
	return module_info->vaBase;
}

size_t Memory::GetBaseSize(std::string module_name)
{
	std::wstring str(module_name.begin(), module_name.end());

	PVMMDLL_MAP_MODULEENTRY module_info;
	auto bResult = VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL);
	if (bResult)
	{
#ifdef DEBUG
		LOG_INFO("[+] Found Base Size for %s at 0x%llx\n", module_name.c_str(), (int64_t)(module_info->cbImageSize));
#endif // DEBUG
		
		return module_info->cbImageSize;
	}
	return 0;
}

uintptr_t Memory::GetExportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_EAT eat_map = NULL;
	PVMMDLL_MAP_EATENTRY export_entry = NULL;
	bool result = VMMDLL_Map_GetEATU(vHandle, GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &eat_map);
	if (!result)
	{
		LOG_ERROR("[!] Failed to get Export Table\n");
		return 0;
	}

	if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
	{
		VMMDLL_MemFree(eat_map);
		eat_map = NULL;
		LOG_ERROR("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < eat_map->cMap; i++)
	{
		export_entry = eat_map->pMap + i;
		if (strcmp(export_entry->uszFunction, import.c_str()) == 0)
		{
			addr = export_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(eat_map);
	eat_map = NULL;

	return addr;
}

uintptr_t Memory::GetImportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_IAT iat_map = NULL;
	PVMMDLL_MAP_IATENTRY import_entry = NULL;
	bool result = VMMDLL_Map_GetIATU(vHandle, GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &iat_map);
	if (!result)
	{
		LOG_ERROR("[!] Failed to get Import Table\n");
		return 0;
	}

	if (iat_map->dwVersion != VMMDLL_MAP_IAT_VERSION)
	{
		VMMDLL_MemFree(iat_map);
		iat_map = NULL;
		LOG_ERROR("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < iat_map->cMap; i++)
	{
		import_entry = iat_map->pMap + i;
		if (strcmp(import_entry->uszFunction, import.c_str()) == 0)
		{
			addr = import_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(iat_map);
	iat_map = NULL;

	return addr;
}

uint64_t cbSize = 0x80000;
//callback for VfsFileListU
VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo)
{
	if (strcmp(uszName, "dtb.txt") == 0)
		cbSize = cb;
}
struct Info
{
	uint32_t index;
	uint32_t process_id;
	uint64_t dtb;
	uint64_t kernelAddr;
	std::string name;
};
bool Memory::FixCr3()
{
	PVMMDLL_MAP_MODULEENTRY module_entry = NULL;
	bool result = VMMDLL_Map_GetModuleFromNameU(this->vHandle, current_process.PID, const_cast<LPSTR>(current_process.process_name.c_str()), &module_entry, NULL);
	if (result)
		return true; //Doesn't need to be patched lol

	if (!VMMDLL_InitializePlugins(this->vHandle))
	{
		LOG_ERROR("[-] Failed VMMDLL_InitializePlugins call\n");
		return false;
	}

	//have to sleep a little or we try reading the file before the plugin initializes fully
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	while (true)
	{
		BYTE bytes[4] = {0};
		DWORD i = 0;
		auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\progress_percent.txt"), bytes, 3, &i, 0);
		if (nt == VMMDLL_STATUS_SUCCESS && atoi(reinterpret_cast<LPSTR>(bytes)) == 100)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	VMMDLL_VFS_FILELIST2 VfsFileList;
	VfsFileList.dwVersion = VMMDLL_VFS_FILELIST_VERSION;
	VfsFileList.h = 0;
	VfsFileList.pfnAddDirectory = 0;
	VfsFileList.pfnAddFile = cbAddFile; //dumb af callback who made this system

	result = VMMDLL_VfsListU(this->vHandle, const_cast<LPSTR>("\\misc\\procinfo\\"), &VfsFileList);
	if (!result)
		return false;

	//read the data from the txt and parse it
	const size_t buffer_size = cbSize;
	std::unique_ptr<BYTE[]> bytes(new BYTE[buffer_size]);
	DWORD j = 0;
	auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\dtb.txt"), bytes.get(), static_cast<DWORD>(buffer_size - 1), &j, 0);
	if (nt != VMMDLL_STATUS_SUCCESS)
		return false;

	std::vector<uint64_t> possible_dtbs = { };
	std::string lines(reinterpret_cast<char*>(bytes.get()));
	std::istringstream iss(lines);
	std::string line = "";

	while (std::getline(iss, line))
	{
		Info info = { };

		std::istringstream info_ss(line);
		if (info_ss >> std::hex >> info.index >> std::dec >> info.process_id >> std::hex >> info.dtb >> info.kernelAddr >> info.name)
		{
			if (info.process_id == 0) //parts that lack a name or have a NULL pid are suspects
				possible_dtbs.push_back(info.dtb);
			if (current_process.process_name.find(info.name) != std::string::npos)
				possible_dtbs.push_back(info.dtb);
		}
	}

	//loop over possible dtbs and set the config to use it til we find the correct one
	for (size_t i = 0; i < possible_dtbs.size(); i++)
	{
		auto dtb = possible_dtbs[i];
		static ULONG64 pml4[512];
		DWORD readsize;
		
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | current_process.PID, dtb);
		result = VMMDLL_Map_GetModuleFromNameU(this->vHandle, current_process.PID, const_cast<LPSTR>(current_process.process_name.c_str()), &module_entry, NULL);
		if (result)
		{
			VMMDLL_MemReadEx(this->vHandle, -1, dtb, reinterpret_cast<PBYTE>(pml4), sizeof(pml4), (PDWORD)&readsize, VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_NOPAGING | VMMDLL_FLAG_ZEROPAD_ON_FAIL | VMMDLL_FLAG_NOPAGING_IO);
			VMMDLL_MemReadEx((VMM_HANDLE)-666, 333, (ULONG64)pml4, 0, 0, 0, 0);
			VMMDLL_MemWrite(this->vHandle, -1, dtb, reinterpret_cast<PBYTE>(pml4), sizeof(pml4));
			VMMDLL_MemWrite((VMM_HANDLE)-666, 333, (ULONG64)pml4,0,0);
			//VMMDLL_VmMemWrite
			VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | current_process.PID, 666);
			
#ifdef DEBUG
			LOG_INFO("[+] Patched DTB\n");
#endif // DEBUG
			return true;
		}
	}

	LOG_ERROR("[-] Failed to patch module\n");
	return false;
}

bool Memory::DumpMemory(uintptr_t address, std::string path)
{
	LOG_INFO("[!] Memory dumping currently does not rebuild the IAT table, imports will be missing from the dump.\n");
	IMAGE_DOS_HEADER dos { };
	Read(address, &dos, sizeof(IMAGE_DOS_HEADER));

	//Check if memory has a PE 
	if (dos.e_magic != 0x5A4D) //Check if it starts with MZ
	{
		LOG_ERROR("[-] Invalid PE Header\n");
		return false;
	}

	IMAGE_NT_HEADERS64 nt;
	Read(address + dos.e_lfanew, &nt, sizeof(IMAGE_NT_HEADERS64));

	//Sanity check
	if (nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		LOG_ERROR("[-] Failed signature check\n");
		return false;
	}
	//Shouldn't change ever. so const 
	const size_t target_size = nt.OptionalHeader.SizeOfImage;
	//Crashes if we don't make it a ptr :(
	auto target = std::unique_ptr<uint8_t[]>(new uint8_t[target_size]);

	//Read whole modules memory
	Read(address, target.get(), target_size);
	auto nt_header = (PIMAGE_NT_HEADERS64)(target.get() + dos.e_lfanew);
	auto sections = (PIMAGE_SECTION_HEADER)(target.get() + dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader);

	for (size_t i = 0; i < nt.FileHeader.NumberOfSections; i++, sections++)
	{
		//Rewrite the file offsets to the virtual addresses
		LOG_ERROR("[!] Rewriting file offsets at 0x%llx size 0x%llx\n", (int64_t)(sections->VirtualAddress), (int64_t)(sections->Misc.VirtualSize));
		sections->PointerToRawData = sections->VirtualAddress;
		sections->SizeOfRawData = sections->Misc.VirtualSize;
	}

	auto debug = (PIMAGE_DEBUG_DIRECTORY)(target.get() + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress);
	debug->PointerToRawData = debug->AddressOfRawData;

	//Find all modules used by this process
	//auto descriptor = Read<IMAGE_IMPORT_DESCRIPTOR>(address + ntHeader->OptionalHeader.DataDirectory[1].VirtualAddress);

	//int descriptor_count = 0;
	//int thunk_count = 0;

	/*std::vector<ModuleData> modulelist;
	while (descriptor.Name) {
		auto first_thunk = Read<IMAGE_THUNK_DATA>(moduleAddr + descriptor.FirstThunk);
		auto original_first_thunk = Read<IMAGE_THUNK_DATA>(moduleAddr + descriptor.OriginalFirstThunk);
		thunk_count = 0;

		char ModuleName[256];
		ReadMemory(moduleAddr + descriptor.Name, (void*)&ModuleName, 256);

		std::string DllName = ModuleName;

		ModuleData tmpModuleData;

		//if(std::find(modulelist.begin(), modulelist.end(), tmpModuleData) == modulelist.end())
		//	modulelist.push_back(tmpModuleData);
		while (original_first_thunk.u1.AddressOfData) {
			char name[256];
			ReadMemory(moduleAddr + original_first_thunk.u1.AddressOfData + 0x2, (void*)&name, 256);

			std::string str_name = name;
			auto thunk_offset{ thunk_count * sizeof(uintptr_t) };

			//if (str_name.length() > 0)
			//	imports[str_name] = moduleAddr + descriptor.FirstThunk + thunk_offset;

			++thunk_count;
			first_thunk = Read<IMAGE_THUNK_DATA>(moduleAddr + descriptor.FirstThunk + sizeof(IMAGE_THUNK_DATA) * thunk_count);
			original_first_thunk = Read<IMAGE_THUNK_DATA>(moduleAddr + descriptor.OriginalFirstThunk + sizeof(IMAGE_THUNK_DATA) * thunk_count);
		}

		++descriptor_count;
		descriptor = Read<IMAGE_IMPORT_DESCRIPTOR>(moduleAddr + ntHeader->OptionalHeader.DataDirectory[1].VirtualAddress + sizeof(IMAGE_IMPORT_DESCRIPTOR) * descriptor_count);
	}*/

	//Rebuild import table

	//LOG("[!] Creating new import section\n");

	//Create New Import Section

	//Build new import Table

	//Dump file
	const auto dumped_file = CreateFileW(std::wstring(path.begin(), path.end()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_COMPRESSED, NULL);
	if (dumped_file == INVALID_HANDLE_VALUE)
	{
		LOG_ERROR("[!] Failed creating file: %i\n", GetLastError());
		return false;
	}

	if (!WriteFile(dumped_file, target.get(), static_cast<DWORD>(target_size), NULL, NULL))
	{
		LOG_ERROR("[!] Failed writing file: %i\n", GetLastError());
		CloseHandle(dumped_file);
		return false;
	}

	LOG_INFO("[+] Successfully dumped memory at %s\n", path.c_str());
	CloseHandle(dumped_file);
	return true;
}
static const char* hexdigits =
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\001\002\003\004\005\006\007\010\011\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

static uint8_t GetByte(const char* hex)
{
	return static_cast<uint8_t>((hexdigits[hex[0]] << 4) | (hexdigits[hex[1]]));
}
uint64_t Memory::FindSignature(const char* signature, uint64_t range_start, uint64_t range_end, int PID)
{
	if (!signature || signature[0] == '\0' || range_start >= range_end)
		return 0;

	if (PID == 0)
		PID = current_process.PID;

	std::vector<uint8_t> buffer(range_end - range_start);
	if (!VMMDLL_MemReadEx(this->vHandle, PID, range_start, buffer.data(), static_cast<DWORD>(buffer.size()), 0, VMMDLL_FLAG_NOCACHE))
		return 0;

	const char* pat = signature;
	uint64_t first_match = 0;
	for (uint64_t i = range_start; i < range_end; i++)
	{
		if (*pat == '?' || buffer[i - range_start] == GetByte(pat))
		{
			if (!first_match)
				first_match = i;

			if (!pat[2])
				break;

			pat += (*pat == '?') ? 2 : 3;
		}
		else
		{
			pat = signature;
			first_match = 0;
		}
	}

	return first_match;
}



#define BYTE_InRange(x, a, b)	(x >= a && x <= b) 
#define BYTE_GetBits(x)			(BYTE_InRange((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (BYTE_InRange(x,'0','9') ? x - '0' : 0))
#define BYTE_Get(x)				(BYTE_GetBits(x[0]) << 4 | BYTE_GetBits(x[1]))

__forceinline bool Signature_IsMatch(uint8_t* m_Address, uint8_t* m_Pattern, uint8_t* m_Mask)
{
	size_t m_Index = 0;
	while (m_Address[m_Index] == m_Pattern[m_Index] || m_Mask[m_Index] == (uint8_t)('?'))
	{
		if (!m_Mask[++m_Index])
			return true;
	}

	return false;
}

uintptr_t Memory::FindSignature(uintptr_t m_Address, uintptr_t m_Size, const char* m_Signature, int occurrenceIndex = 0)
{
	size_t m_SignatureLength = strlen(m_Signature);

	uint8_t* m_PatternAlloc = new uint8_t[m_SignatureLength >> 1];
	uint8_t* m_MaskAlloc = new uint8_t[m_SignatureLength >> 1];
	uint8_t* m_Pattern = m_PatternAlloc;
	uint8_t* m_Mask = m_MaskAlloc;

	// Run-Time IDA Sig to Sig & Mask
	size_t m_PatternLength = 0;
	while (*m_Signature)
	{
		if (*m_Signature == ' ') m_Signature++;
		if (!*m_Signature) break;

		if (*(uint8_t*)(m_Signature) == (uint8_t)('\?'))
		{
			*m_Pattern++ = 0;
			*m_Mask++ = '?';
			m_Signature += ((*(uint16_t*)(m_Signature) == (uint16_t)('\?\?')) ? 2 : 1);
		}
		else
		{
			*m_Pattern++ = BYTE_Get(m_Signature);
			*m_Mask++ = 'x';
			m_Signature += 2;
		}

		++m_PatternLength;
	}

	// Find Address
	*m_Mask = 0;

	int m_OccurrenceIndex = 0;
	uintptr_t m_ReturnValue = 0;
	for (uintptr_t i = 0; m_Size > i; ++i)
	{
		if (Signature_IsMatch(reinterpret_cast<uint8_t*>(m_Address + i), m_PatternAlloc, m_MaskAlloc))
		{
			if(occurrenceIndex == m_OccurrenceIndex)
			{
				m_ReturnValue = (m_Address + i);
				break;
			}
            m_OccurrenceIndex++;
		}
	}

	delete[] m_PatternAlloc;
	delete[] m_MaskAlloc;
	return m_ReturnValue;
}
uintptr_t Memory::FindSignature(uintptr_t m_Address, uintptr_t m_Size, const char* m_Signature)
{
	size_t m_SignatureLength = strlen(m_Signature);

	uint8_t* m_PatternAlloc = new uint8_t[m_SignatureLength >> 1];
	uint8_t* m_MaskAlloc = new uint8_t[m_SignatureLength >> 1];
	uint8_t* m_Pattern = m_PatternAlloc;
	uint8_t* m_Mask = m_MaskAlloc;

	// Run-Time IDA Sig to Sig & Mask
	size_t m_PatternLength = 0;
	while (*m_Signature)
	{
		if (*m_Signature == ' ') m_Signature++;
		if (!*m_Signature) break;

		if (*(uint8_t*)(m_Signature) == (uint8_t)('\?'))
		{
			*m_Pattern++ = 0;
			*m_Mask++ = '?';
			m_Signature += ((*(uint16_t*)(m_Signature) == (uint16_t)('\?\?')) ? 2 : 1);
		}
		else
		{
			*m_Pattern++ = BYTE_Get(m_Signature);
			*m_Mask++ = 'x';
			m_Signature += 2;
		}

		++m_PatternLength;
	}

	// Find Address
	*m_Mask = 0;

	uintptr_t m_ReturnValue = 0;
	for (uintptr_t i = 0; m_Size > i; ++i)
	{
		if (Signature_IsMatch(reinterpret_cast<uint8_t*>(m_Address + i), m_PatternAlloc, m_MaskAlloc))
		{
			m_ReturnValue = (m_Address + i);
			break;
		}
	}

	delete[] m_PatternAlloc;
	delete[] m_MaskAlloc;
	return m_ReturnValue;
}

bool Memory::ConvertHexToByteArray(const std::string& HexString, std::vector<uint8_t>& ByteArray)
{
	ByteArray.clear();
	size_t len = HexString.length();
	for (size_t i = 0; i < len; i += 3) {
		std::string ByteString = HexString.substr(i, 2);
		if (ByteString == "??" || ByteString == "?? ")
			ByteArray.push_back(0);

		else
		{
			char* End;
			long byte = std::strtol(ByteString.c_str(), &End, 16);

			if (*End != 0)
				return false;

			ByteArray.push_back(static_cast<uint8_t>(byte));
		}
	}

	return true;
}

uint64_t Memory::FindSignature(
	const uintptr_t range_start, const uintptr_t range_end,
	const std::string& pattern, bool extract_address,
	bool extract_offset, int offset_position)
{
	if (range_start >= range_end || pattern.empty())
		return 0;

	// 将模式转换为字节数组
	std::vector<uint8_t> pattern_bytes;
	if (!ConvertHexToByteArray(pattern, pattern_bytes))
		return 0;

	size_t pattern_size = pattern_bytes.size();
	if (pattern_size == 0)
		return 0;

	const size_t block_size = 0x1000; // 每次读取的内存块大小
	uintptr_t current_address = range_start;

	// 创建 Scatter Handle
	VMMDLL_SCATTER_HANDLE scatter_handle = CreateScatterHandle();
	if (!scatter_handle)
		return 0;

	while (current_address < range_end)
	{
		size_t read_size = (std::min)(block_size, range_end - current_address);
		std::vector<uint8_t> buffer(read_size);

		// 添加读取请求到 Scatter Handle
		AddScatterReadRequest(scatter_handle, current_address, buffer.data(), read_size);

		// 执行 Scatter Read
		ExecuteReadScatter(scatter_handle, 0);

		// 在读取的块中进行模式匹配
		for (size_t offset = 0; offset < read_size - pattern_size + 1; ++offset)
		{
			bool match = true;
			for (size_t i = 0; i < pattern_size; ++i)
			{
				if (pattern_bytes[i] != 0x00 && buffer[offset + i] != pattern_bytes[i])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				uintptr_t match_address = current_address + offset;

				// 提取地址或偏移
				if (extract_address)
				{
					int32_t relative_offset = 0;
					if (!Read(match_address + offset_position, &relative_offset, sizeof(relative_offset)))
					{
						CloseScatterHandle(scatter_handle);
						return 0;
					}

					CloseScatterHandle(scatter_handle);
					return match_address + 7 + relative_offset;
				}

				if (extract_offset)
				{
					uintptr_t offset_address = match_address + offset_position;
					int32_t relative_offset = 0;
					if (!Read(offset_address, &relative_offset, sizeof(relative_offset)))
					{
						CloseScatterHandle(scatter_handle);
						return 0;
					}

					CloseScatterHandle(scatter_handle);
					return offset_address + sizeof(int32_t) + relative_offset;
				}

				CloseScatterHandle(scatter_handle);
				return match_address;
			}
		}

		// 更新地址范围
		current_address += block_size - pattern_size + 1; // 避免漏掉跨块的匹配
	}

	CloseScatterHandle(scatter_handle);
	return 0;
}

uintptr_t Memory::FindSignature(
	uintptr_t m_Address,
	uintptr_t m_Size,
	const char* m_Signature,
	uint8_t* outWildcardValue // 新增输出参数
) {
	size_t m_SignatureLength = strlen(m_Signature);
	uint8_t* m_Pattern = new uint8_t[m_SignatureLength >> 1];
	uint8_t* m_Mask = new uint8_t[m_SignatureLength >> 1];

	size_t m_PatternLength = 0;
	size_t wildcardCount = 0;
	size_t secondWildcardPos = 0;  // 记录第二个通配符位置

	// 解析签名并记录通配符位置
	while (*m_Signature) {
		if (*m_Signature == ' ') {
			m_Signature++;
			continue;
		}

		if (m_Signature[0] == '?') {
			if (m_Signature[1] == '?') {
				m_Pattern[m_PatternLength] = 0;
				m_Mask[m_PatternLength] = '?';
				m_Signature += 2;

				// 记录第二个通配符位置
				if (++wildcardCount == 2) {
					secondWildcardPos = m_PatternLength;
				}
			}
		}
		else {
			m_Pattern[m_PatternLength] = BYTE_Get(m_Signature);
			m_Mask[m_PatternLength] = 'x';
			m_Signature += 2;
		}
		m_PatternLength++;
	}

	// 搜索内存
	uintptr_t foundAddr = 0;
	for (uintptr_t i = 0; i < m_Size; ++i) {
		uint8_t* currentAddr = this->Read<uint8_t*>(m_Address + i);
		if (Signature_IsMatch(currentAddr, m_Pattern, m_Mask)) {
			foundAddr = m_Address + i;

			// 获取第二个通配符处的值
			if (outWildcardValue && secondWildcardPos < m_PatternLength) {
				*outWildcardValue = *(currentAddr + secondWildcardPos);
			}
			break;
		}
	}

	delete[] m_Pattern;
	delete[] m_Mask;
	return foundAddr;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size) const
{
	if (!VMMDLL_MemWrite(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size)))
	{
		LOG_ERROR("[!] Failed to write Memory at 0x%llx\n", address);
		return false;
	}
	return true;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size, int pid) const
{
	if (!VMMDLL_MemWrite(this->vHandle, pid, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size)))
	{
		LOG_ERROR("[!] Failed to write Memory at 0x%llx\n", address);
		return false;
	}
	return true;
}
bool Memory::Read(void* address, void* buffer) const
{
	DWORD read_size = 0;
	size_t size = sizeof(buffer);
	if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, reinterpret_cast<uintptr_t>(address), static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[!] Failed to read Memory at 0x%p\n", address);
		return false;
	}

	return (read_size == size);
}
bool Memory::Read(void* address, void* buffer, size_t size) const
{
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, reinterpret_cast<uintptr_t>(address), static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[!] Failed to read Memory at 0x%llx\n", (int64_t)(address));
		return false;
	}

	return (read_size == size);
}
bool Memory::Read(uintptr_t address, void* buffer, size_t size) const
{
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[!] Failed to read Memory at 0x%llx\n", address);
		return false;
	}

	return (read_size == size);
}
bool Memory::Read(uintptr_t address, void* buffer, size_t size, int pid) const
{
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, pid, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[!] Failed to read Memory at 0x%llx\n", address);
		return false;
	}
	return (read_size == size);
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle() const
{
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, current_process.PID, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		LOG_ERROR("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle(int pid) const
{
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, pid, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		LOG_ERROR("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

void Memory::CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle)
{
	VMMDLL_Scatter_CloseHandle(handle);
}

void Memory::AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	if (!VMMDLL_Scatter_PrepareEx(handle, address, static_cast<DWORD>(size), static_cast<PBYTE>(buffer), NULL))
	{
		LOG_ERROR("[!] Failed to prepare scatter read at 0x%llx\n", address);
	}
}

void Memory::AddScatterWriteRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	if (!VMMDLL_Scatter_PrepareWrite(handle, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size)))
	{
		LOG_ERROR("[!] Failed to prepare scatter write at 0x%llx\n", address);
	}
}


void Memory::ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	if (pid == 0)
		pid = current_process.PID;

	if (!VMMDLL_Scatter_ExecuteRead(handle))
	{
		LOG_ERROR("[-] Failed to Execute Scatter Read\n");
	}
	//Clear after using it
	if (!VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[-] Failed to clear Scatter\n");
	}
}

void Memory::ExecuteWriteScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	if (pid == 0)
		pid = current_process.PID;

	if (!VMMDLL_Scatter_Execute(handle))
	{
		LOG_ERROR("[-] Failed to Execute Scatter Read\n");
	}
	//Clear after using it
	if (!VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE))
	{
		LOG_ERROR("[-] Failed to clear Scatter\n");
	}
}
DWORD64 Memory::TraceAddress(DWORD64 BaseAddress, std::vector<DWORD> Offsets)
{
	DWORD64 Address = 0;

	if (Offsets.size() == 0)
		return BaseAddress;

	if (!Read(BaseAddress, &Address, sizeof(DWORD64)))
		return 0;

	for (int i = 0; i < Offsets.size() - 1; i++)
	{
		if (!Read(Address + Offsets[i], &Address,sizeof(DWORD64)))
			return 0;

	}
	return Address == 0 ? 0 : Address + Offsets[Offsets.size() - 1];
}

Memory::CurrentProcessInformation Memory::GetCurProcessInformation()
{
	return this->current_process;
}

