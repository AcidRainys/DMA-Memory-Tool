#pragma once
#include "Memory/Memory.h"
#include "Memory/logger.h"

class Memory;
class HackTools {
private:
	struct CurrentProcessInformation
	{
		int PID = 0;
		size_t base_address = 0;
		size_t base_size = 0;
		std::string process_name = "";
		CurrentProcessInformation()
		{
			PID = 0;
			base_address = 0;
			base_size = 0;
			process_name = "";
		}
	};
	

public:
	struct ProcessModuleINFO
	{
		size_t base_address = 0;
		size_t base_size = 0;
		int64_t base_endress = 0;
		std::string module_name = "";
	};

    HackTools();
    ~HackTools();

    static bool DumpTargetProcess();
    static bool DumpTargetModules(std::vector<ProcessModuleINFO> list);
	static bool GetTargetAllModule();

    bool InitiationProcess(const char* process_name);

	bool GetAllProcess();

	

	std::vector<DWORD> GetPidList() const { return m_pidlist; }
	std::unordered_map<DWORD, std::string> GetPidFormName() const { return m_pidforname; }
	int64_t GetBaseAddress() const { return m_curprocess.base_address; }
	int64_t GetBaseSize() const { return m_curprocess.base_size; }
	int64_t GetPid() const { return m_curprocess.PID; }

	std::vector<ProcessModuleINFO> GetModuleList() const { return m_modulelist; }
private:


    void LOGO();

	CurrentProcessInformation m_curprocess;

	std::vector<ProcessModuleINFO> m_modulelist;

	std::vector<DWORD> m_pidlist;
	std::unordered_map<DWORD, std::string> m_pidforname;
};

extern HackTools Hacker;
