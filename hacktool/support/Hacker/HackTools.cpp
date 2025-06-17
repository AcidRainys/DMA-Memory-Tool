#include "HackTools.h"



HackTools::HackTools()
{
    if (!mem.FPGAINIT(false, false))
    {
        LOG_ERROR("FPGA��ʼ��ʧ�� \n");
    }
    LOG_CLEAR();

    LOGO();

	LOG_INFO("Welcome to HackerTool");
	LOG_INFO("Version: 1.0.0");
	LOG_INFO("Build Date: %s %s", __DATE__, __TIME__);

}

HackTools::~HackTools()
{
}


bool HackTools::DumpTargetProcess()
{
    auto process = mem.GetCurProcessInformation();
    if (!process.PID || !process.base_address || !process.base_size)
    {
        LOG_ERROR("[DUMP] ����û�л�ȡ�� �����³�ʼ��");
        return false;
    }

    if (process.process_name.find(Hacker.m_curprocess.process_name.c_str()) == std::string::npos)
    {
        LOG_ERROR("[DUMP] ��ǰ��������ҪDump�Ľ��̲�һ��");
        return false;
    }
    LOG_INFO("[DUMP] ��ʼDump");
   
    auto buffer = (BYTE*)malloc(process.base_size);
    if (!buffer)
    {
        LOG_ERROR("[DUMP] ���仺����ʧ�ܣ�������룺%d��", GetLastError());
        return false;
    }
    LOG_INFO("[DUMP] �ѷ���Ļ�����λ��  0x%p", buffer);

    auto scatterHandle = mem.CreateScatterHandle();
    if (!scatterHandle)
    {
        LOG_ERROR("[DUMP] ������ɢ���ʧ��");
        free(buffer);
        return false;
    }

    // ��������ڴ�ҳ����ɢ��ȡ����
    for (ULONG iterator = 0x0; iterator < process.base_size; iterator += 0x1000)
    {
        mem.AddScatterReadRequest(
            scatterHandle,
            process.base_address + iterator,
            buffer + iterator,
            0x1000
        );
    }

    // ִ��������ȡ
    mem.ExecuteReadScatter(scatterHandle);
    mem.CloseScatterHandle(scatterHandle);

    // ��֤DOSͷ
    auto pdos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);
    if (!pdos_header->e_lfanew)
    {
        LOG_ERROR("[DUMP] δ�ܴӻ�������ȡ DOS ͷ��Ϣ");
        free(buffer);
        return false;
    }
    LOG_INFO("[DUMP] Dos ͷ����ַ: %p", pdos_header);
    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        LOG_ERROR("[DUMP] DOS ͷ��ǩ����Ч");
        free(buffer);
        free(buffer);
        return false;
    }

    // ��֤NTͷ
    auto pnt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer + pdos_header->e_lfanew);
    if (!pnt_header)
    {
        LOG_ERROR("[DUMP] �޷��ӻ�������ȡ'Nt'ͷ��");
        free(buffer);
        return false;
    }
    LOG_INFO("[DUMP] Ntͷ��: 0x%p", pnt_header);

    if (pnt_header->Signature != IMAGE_NT_SIGNATURE)
    {
        LOG_ERROR("[DUMP] Ntͷ��ǩ����Ч");
        free(buffer);
        return false;
    }

    // �����ѡͷ
    auto poptional_header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(&pnt_header->OptionalHeader);
    if (!poptional_header)
    {
        LOG_ERROR("[DUMP] �޷��ӻ�������ȡͷ��Ϣ");
        free(buffer);
        return false;
    }
    LOG_INFO("[DUMP] ͷ��Ϣ��0x%p", poptional_header);

    // �������
    int i = 0;
    unsigned int section_offset = poptional_header->SizeOfHeaders;
    for (
        PIMAGE_SECTION_HEADER psection_header = IMAGE_FIRST_SECTION(pnt_header);
        i < pnt_header->FileHeader.NumberOfSections;
        i++, psection_header++)
    {
        psection_header->Misc.VirtualSize = psection_header->SizeOfRawData;
        memcpy(buffer + section_offset, psection_header, sizeof(IMAGE_SECTION_HEADER));
        section_offset += sizeof(IMAGE_SECTION_HEADER);

        // ʹ��Memory���ȡ��������
        if (!mem.Read(
            poptional_header->ImageBase + psection_header->VirtualAddress,
            buffer + psection_header->PointerToRawData,
            psection_header->SizeOfRawData))
        {
            LOG_ERROR("[DUMP] �޷���ȡλ�� 0x%lX ���ķ�������",
                poptional_header->ImageBase + psection_header->VirtualAddress);
            free(buffer);
            return false;
        }
    }

    // ����ת���ļ�
    char FileName[MAX_PATH];
    sprintf_s(FileName, "%s\\%s_dump.exe", mem.GetExePath().c_str(), process.process_name.c_str());

    std::ofstream Dump(FileName, std::ios::binary);
    Dump.write((char*)buffer, process.base_size);
    Dump.close();

    std::string path = FileName;
    LOG_INFO("[DUMP] Dump�ɹ� λ��:" + path + "");

    free(buffer);
    return true;
}

bool HackTools::DumpTargetModules(std::vector<ProcessModuleINFO> list)
{
    auto process = mem.GetCurProcessInformation();
    if (!process.PID || !process.base_address || !process.base_size)
    {
        LOG_ERROR("[DUMP] ����û�л�ȡ�� �����³�ʼ��");
        return false;
    }
    for (const auto& module : list)
    {
        auto buffer = (BYTE*)malloc(module.base_size);

        if (!buffer)
        {
            LOG_ERROR("[DUMP] ���仺����ʧ�ܣ�������룺%d��", GetLastError());
            continue;
        }

        LOG_INFO("[DUMP] �ѷ���Ļ�����λ��  0x%p", buffer);


        for (ULONG iterator = 0x0; iterator < module.base_size; iterator += 0x1000) {
            size_t read_size = ((iterator + 0x1000) > module.base_size) ? (module.base_size - iterator) : 0x1000;
            if (!mem.Read(module.base_address + iterator, buffer + iterator, read_size)) {
                LOG_ERROR("[DUMP] �޷���ȡλ�� 0x%lX ���Ļ�������������룺%d��", module.base_address + iterator, GetLastError());
                free(buffer);
                return false;
            }
        }

        auto pdos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);

        if (!pdos_header->e_lfanew)
        {
            LOG_ERROR("[DUMP] δ�ܴӻ�������ȡ DOS ͷ��Ϣ");
            free(buffer);
            continue;
        }

        LOG_INFO("[DUMP] Dos ͷ����ַ: %p", pdos_header);

        if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
        {
            LOG_ERROR("[DUMP] DOS ͷ��ǩ����Ч");
            free(buffer);
            continue;
        }

        auto pnt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer + pdos_header->e_lfanew);

        if (!pnt_header)
        {
            LOG_ERROR("[DUMP] �޷��ӻ�������ȡNTͷ����ַ");
            free(buffer);
            continue;
        }

        LOG_INFO("[DUMP] NT ͷ��ַ��0x%p", pnt_header);
        if (pnt_header->Signature != IMAGE_NT_SIGNATURE)
        {
            LOG_ERROR("[DUMP] ������Ч�� NT ͷ��ǩ��");
            free(buffer);
            continue;
        }

        auto poptional_header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(&pnt_header->OptionalHeader);

        if (!poptional_header)
        {
            LOG_ERROR("[DUMP] �޷��ӻ�������ȡ��ͷ��ַ");
            free(buffer);
            continue;
        }

        LOG_INFO("[DUMP] ��ͷ��ַ��0x%p", poptional_header);
        int i = 0;
        unsigned int section_offset = poptional_header->SizeOfHeaders;

        for (
            PIMAGE_SECTION_HEADER psection_header = IMAGE_FIRST_SECTION(pnt_header);
            i < pnt_header->FileHeader.NumberOfSections;
            i++, psection_header++
            )
        {
            size_t section_size = (std::max)(psection_header->Misc.VirtualSize, psection_header->SizeOfRawData);

            memcpy(buffer + section_offset, psection_header, sizeof(IMAGE_SECTION_HEADER));
            section_offset += sizeof(IMAGE_SECTION_HEADER);

            if (!mem.Read(
                poptional_header->ImageBase + psection_header->VirtualAddress,
                buffer + psection_header->PointerToRawData,
                section_size
            ))
            {
                LOG_ERROR("[DUMP] δ�ܶ�ȡ�ò��ֵĻ����� :" + std::to_string((int)psection_header->Name) + "");
                free(buffer);
                continue;
            }
        }

        char FileName[MAX_PATH];
        sprintf_s(FileName, "%s\\%s_dump.dll", mem.GetExePath().c_str(), module.module_name.c_str());


        std::ofstream Dump(FileName, std::ios::binary);
        Dump.write((char*)buffer, module.base_size);
        Dump.close();

        std::string path = FileName;
        LOG_INFO("[DUMP] Dump�ɹ� λ��:" + path + "");
        free(buffer);
    }
    return true;
}


bool HackTools::GetTargetAllModule()
{
    auto process = mem.GetCurProcessInformation();
    if (!process.PID || !process.base_address || !process.base_size)
    {
        LOG_ERROR("[Memory] ����û�л�ȡ�� �����³�ʼ��");
        return false;
    }

    if (process.process_name.find(Hacker.m_curprocess.process_name.c_str()) == std::string::npos)
    {
        LOG_ERROR("[Memory] ��ǰ��������ҪDump�Ľ��̲�һ��");
        return false;
    }
    LOG_INFO("[Memory] ��ʼ��ȡĿ��ģ���б�");


    const auto& module_ret = mem.GetModuleList(process.process_name.c_str());
    size_t module_ret_size = module_ret.size();
    std::vector<ProcessModuleINFO> modules;
    for (size_t i = 0; i < module_ret_size; i++)
    {
        const auto& str = module_ret[i];
        size_t base_address = mem.GetBaseDaddy(str);
        size_t base_size = mem.GetBaseSize(str);
        int64_t base_endress = base_address + base_size;
        if (base_address == 0 || base_size == 0 || str.find(".exe") != std::string::npos || str.find(".DRV") != std::string::npos) continue;
        modules.push_back({ base_address, base_size, base_endress, str });
    }
    Hacker.m_modulelist = modules;
    return true;
}

bool HackTools::InitiationProcess(const char* process_name)
{
    if (!m_curprocess.process_name.empty() && m_curprocess.process_name == process_name) {
        LOG_DEBUG("Process already initialized: " + std::string(process_name));
        return true;  
    }

    
    if (!m_curprocess.process_name.empty()) {
        LOG_DEBUG("Releasing previous process: " + m_curprocess.process_name);
        mem.Release();
    }

    
    if (!mem.Init(process_name, false, false)) {
        LOG_ERROR("Failed to initialize process: " + std::string(process_name));
        m_curprocess = {};  
        return false;
    }

    
    const auto process_info = mem.GetCurProcessInformation();
    m_curprocess.base_address = process_info.base_address;
    m_curprocess.base_size = process_info.base_size;
    m_curprocess.PID = process_info.PID;
    m_curprocess.process_name = process_info.process_name;

    LOG_INFO("Successfully initialized process: " + std::string(process_name) +
        " (PID: " + std::to_string(m_curprocess.PID) + ")");
    return true;
}

bool HackTools::GetAllProcess()
{
    this->m_pidlist = mem.GetPidList();
    if (this->m_pidlist.empty())
        return false;

    for (const auto& pid : this->m_pidlist)
    {
        auto INFORMATION = mem.GetProcessInformationFormPid(pid);
        std::string name = INFORMATION.szNameLong;
#ifdef _DEBUG
        LOG_INFO("Pid: " + std::to_string(INFORMATION.dwPID) + "" + " Name: " + name + " ");
#endif // DEBUG
        this->m_pidforname[pid] = name;
    }

    return true;
}

void HackTools::LOGO()
{
    printf("\n");
    printf("\033[1;36m"); 
    printf("  _______  ______   _______  _______ \n");
    printf(" /  _____||   _  \\ |   ____||   ____|\n");
    printf("|  |  __  |  |_)  ||  |__   |  |__   \n");
    printf("\033[1;33m"); 
    printf("|  | |_ | |      / |   __|  |   __|  \n");
    printf("|  |__| | |  |\\  \\ |  |____ |  |____ \n");
    printf("\033[1;32m"); 
    printf(" \\______| |__| \\__\\|_______||_______|\n");
    printf("\033[0m"); 
    printf("\n");
}


HackTools Hacker;