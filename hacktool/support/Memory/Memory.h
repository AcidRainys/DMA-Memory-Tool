﻿#pragma once
#include "pch.h"
#include "InputManager.h"
#include "Registry.h"
#include "Shellcode.h"
#include "structs.h"
#include <xmmintrin.h>
#include <unordered_map>
#include <any>
#define CONSTEXPR_INLINE constexpr inline 
#define INLINE inline 
#define INLINEex extern inline 



inline HMODULE LoadResourceEx(LPCWSTR lpName, LPCWSTR lpType);

class Memory
{
private:
	struct LibModules
	{
		HMODULE VMM = nullptr;
		HMODULE FTD3XX = nullptr;
		HMODULE LEECHCORE = nullptr;
		LibModules()
		{
			VMM = nullptr;
			FTD3XX = nullptr;
			LEECHCORE = nullptr;
		}
	};
public:
	struct ModelInfo {
		int64_t start;
		int64_t end;
		size_t size;
	};
private:
	static inline LibModules modules { };

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

	static inline CurrentProcessInformation current_process { };

	static inline BOOLEAN DMA_INITIALIZED = FALSE;
	static inline BOOLEAN PROCESS_INITIALIZED = FALSE;
	/**
	*Dumps the systems Current physical memory pages
	*To a file so we can use it in our DMA (:
	*This file it created to %temp% folder
	*@return true if successful, false if not.
	*/
	bool DumpMemoryMap(bool debug = false);

	/**
	* brief Removes basic information related to the FPGA device
	* This is required before any DMA operations can be done.
	* To ensure the optimal safety in game cheating.
	* @return true if successful, false if not.
	*/
	bool SetFPGA();

	//shared pointer
	std::shared_ptr<c_keys> key;
	c_registry registry;
	c_shellcode shellcode;

	/*this->registry_ptr = std::make_shared<c_registry>(*this);
	this->key_ptr = std::make_shared<c_keys>(*this);*/

public:
	/**
	 * brief Constructor takes a wide string of the process.
	 * Expects that all the libraries are in the root dir
	 */
	Memory();
	~Memory();

	void Release();
	/**
	* @brief Gets the registry object
	* @return registry class
	*/
	c_registry GetRegistry() { return registry; }

	/**
	* @brief Gets the key object
	* @return key class
	*/
	c_keys* GetKeyboard() { return key.get(); }

	/**
	* @brief Gets the shellcode object
	* @return shellcode class
	*/
	c_shellcode GetShellcode() { return shellcode; }

	/**
	* brief Initializes the DMA
	* This is required before any DMA operations can be done.
	* @param process_name the name of the process
	* @param memMap if true, will dump the memory map to a file	& make the DMA use it.
	* @return true if successful, false if not.
	*/
	bool Init(std::string process_name, bool memMap = true, bool debug = false);

	bool FPGAINIT(bool memMap = true, bool debug = false);

	/*This part here is things related to the process information such as Base daddy, Size ect.*/

	/**
	* brief Gets the process id of the process
	* @param process_name the name of the process
	* @return the process id of the process
	*/
	DWORD GetPidFromName(std::string process_name);

	std::vector<DWORD>  GetPidList();
	/**
	* brief Gets all the processes id(s) of the process
	* @param process_name the name of the process
	* @returns all the processes id(s) of the process
	*/
	std::vector<int> GetPidListFromName(std::string process_name);

	/**
	* \brief Gets the module list of the process
	* \param process_name the name of the process 
	* \return all the module names of the process 
	*/
	std::vector<std::string> GetModuleList(std::string process_name);

	/**
	* \brief Gets the process information
	* \return the process information
	*/
	VMMDLL_PROCESS_INFORMATION GetProcessInformation();
	VMMDLL_PROCESS_INFORMATION GetProcessInformationFormPid(DWORD pid);
	/**
	* \brief Gets the process peb
	* \return the process peb 
	*/
	PEB GetProcessPeb();

	/**
	* brief Gets the base address of the process
	* @param module_name the name of the module
	* @return the base address of the process
	*/
	size_t GetBaseDaddy(std::string module_name);

	/**
	* brief Gets the base size of the process
	* @param module_name the name of the module
	* @return the base size of the process
	*/
	size_t GetBaseSize(std::string module_name);

	/**
	* brief Gets the export table address of the process
	* @param import the name of the export
	* @param process the name of the process
	* @param module the name of the module that you wanna find the export in
	* @return the export table address of the export
	*/
	uintptr_t GetExportTableAddress(std::string import, std::string process, std::string module);

	/**
	* brief Gets the import table address of the process
	* @param import the name of the import
	* @param process the name of the process
	* @param module the name of the module that you wanna find the import in
	* @return the import table address of the import
	*/
	uintptr_t GetImportTableAddress(std::string import, std::string process, std::string module);

	/**
	 * \brief This fixes the CR3 fuckery that EAC does.
	 * It fixes it by iterating over all DTB's that exist within your system and looks for specific ones
	 * that nolonger have a PID assigned to them, aka their pid is 0
	 * it then puts it in a vector to later try each possible DTB to find the DTB of the process.
	 * NOTE: Using FixCR3 requires you to have symsrv.dll, dbghelp.dll and info.db
	 */
	bool FixCr3();

	/**
	 * \brief Dumps the process memory at address (requires to be a valid PE Header) to the path
	 * \param address the address to the PE Header(BaseAddress)
	 * \param path the path where you wanna save dump to
	 */
	bool DumpMemory(uintptr_t address, std::string path);

	/*This part is where all memory operations are done, such as read, write.*/

	/**
	 * \brief Scans the process for the signature.
	 * \param signature the signature example "48 ? ? ?"
	 * \param range_start Region to start scan from 
	 * \param range_end Region up to where it should scan
	 * \param PID (OPTIONAL) where to read to?
	 * \return address of signature
	 */
	uint64_t FindSignature(const char* signature, uint64_t range_start, uint64_t range_end, int PID = 0);

	uintptr_t FindSignature(uintptr_t m_Address, uintptr_t m_Size, const char* m_Signature, int occurrenceIndex);

	uintptr_t FindSignature(uintptr_t m_Address, uintptr_t m_Size, const char* m_Signature);

	bool ConvertHexToByteArray(const std::string& HexString, std::vector<uint8_t>& ByteArray);

	uint64_t FindSignature(const uintptr_t range_start, const uintptr_t range_end, const std::string& pattern, bool extract_address, bool extract_offset, int offset_position);

	uintptr_t FindSignature(uintptr_t m_Address, uintptr_t m_Size, const char* m_Signature, uint8_t* outWildcardValue);

	/**
	 * \brief Writes memory to the process 
	 * \param address The address to write to
	 * \param buffer The buffer to write
	 * \param size The size of the buffer
	 * \return 
	 */
	bool Write(uintptr_t address, void* buffer, size_t size) const;
	bool Write(uintptr_t address, void* buffer, size_t size, int pid) const;

	bool Read(void* address, void* buffer) const;

	bool Read(void* address, void* buffer, size_t size) const;

	/**
	 * \brief Writes memory to the process using a template
	 * \param address to write to
	 * \param value the value you'll write to the address
	 */
	template <typename T>
	void Write(void* address, T value)
	{
		Write(address, &value, sizeof(T));
	}

	template <typename T>
	void Write(uintptr_t address, T value)
	{
		Write(address, &value, sizeof(T));
	}

	/**
	* brief Reads memory from the process
	* @param address The address to read from
	* @param buffer The buffer to read to
	* @param size The size of the buffer
	* @return true if successful, false if not.
	*/
	bool Read(uintptr_t address, void* buffer, size_t size) const;
	bool Read(uintptr_t address, void* buffer, size_t size, int pid) const;

	/**
	* brief Reads memory from the process using a template
	* @param address The address to read from
	* @return the value read from the process
	*/
	template <typename T>
	T Read(void* address)
	{
		T buffer { };
		memset(&buffer, 0, sizeof(T));
		Read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T));

		return buffer;
	}

	template <typename T>
	T Read(uint64_t address)
	{
		return Read<T>(reinterpret_cast<void*>(address));
	}

	/**
	* brief Reads memory from the process using a template and pid
	* @param address The address to read from
	* @param pid The process id of the process
	* @return the value read from the process
	*/
	template <typename T>
	T Read(void* address, int pid)
	{
		T buffer { };
		memset(&buffer, 0, sizeof(T));
		Read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T), pid);

		return buffer;
	}

	template <typename T>
	T Read(uint64_t address, int pid)
	{
		return Read<T>(reinterpret_cast<void*>(address), pid);
	}

	/**
	* brief Reads a chain of offsets from the address
	* @param address The address to read from
	* @param a vector of offset values to read through
	* @return the value read from the chain
	*/
	int64_t ReadChain(uint64_t base, const std::vector<uint32_t>& offsets)
	{
		int64_t result = Read<int64_t>(base + offsets.at(0));
		for (int i = 1; i < offsets.size(); i++) result = Read<int64_t>(result + offsets.at(i));
		return result;
	}

	template <typename T>
	T ReadChain(uint64_t base, const std::vector<uint64_t>& offsets)
	{
		T result = Read<T>(base + offsets.at(0));
		for (int i = 1; i < offsets.size(); i++) result = Read<T>(result + offsets.at(i));
		return result;
	}

	/**
	 * \brief Create a scatter handle, this is used for scatter read/write requests
	 * \return Scatter handle
	 */
	VMMDLL_SCATTER_HANDLE CreateScatterHandle() const;
	VMMDLL_SCATTER_HANDLE CreateScatterHandle(int pid) const;

	/**
	 * \brief Closes the scatter handle
	 * \param handle
	 */
	void CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle);

	/**
	 * \brief Adds a scatter read/write request to the handle
	 * \param handle the handle
	 * \param address the address to read/write to 
	 * \param buffer the buffer to read/write to
	 * \param size the size of buffer
	 */
	void AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);

	template <typename T>
	void AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, T* buffer)
	{
		AddScatterReadRequest(handle, address, reinterpret_cast<void*>(buffer), sizeof(T));
	}
		
	void AddScatterWriteRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);
		
	bool Get_process_base_address();
	
	/**
	 * \brief Executes all prepared scatter requests, note if you created a scatter handle with a pid
	 * you'll need to specify the pid in the execute function. so we can clear the scatters from the handle.
	 * \param handle 
	 * \param pid 
	 */
	void ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle, int pid = 0);
	void ExecuteWriteScatter(VMMDLL_SCATTER_HANDLE handle, int pid = 0);

	DWORD64 TraceAddress(DWORD64 BaseAddress, std::vector<DWORD> Offsets);

	/*the FPGA handle*/
	VMM_HANDLE vHandle;

	std::string GetExePath()
	{
		char szFilePath[MAX_PATH + 1] = { 0 };
		GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
		(strrchr(szFilePath, '\\'))[0] = 0;
		std::string path = szFilePath;
		return path;
	}

	CurrentProcessInformation GetCurProcessInformation();
private:
	std::unordered_map<uint64_t, int64_t> memoryCache;
	std::unordered_map<uint64_t, __m128> memoryCacheM128;

public:
	int64_t CachedRead(uint64_t address) {
		if (memoryCache.find(address) != memoryCache.end()) {
			return memoryCache[address];
		}
		int64_t value = Read<int64_t>(address);
		memoryCache[address] = value;
		return value;
	}

	bool CachedRead(uint64_t address, void* buffer, size_t size) {
		DWORD read_size = 0;

		//               ݣ ֱ ӷ  ػ   ֵ
		if (size == sizeof(int64_t) && memoryCache.find(address) != memoryCache.end()) {
			*static_cast<int64_t*>(buffer) = memoryCache.at(address);
			return true;
		}
		//          û У       ڴ  ȡ
		if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &read_size, VMMDLL_FLAG_NOCACHE)) {
			//std::cout << "[!] Failed to read Memory at 0x" << std::hex << address << std::endl;
			return false;
		}
		read_size = Read<DWORD>(address);
		//   ȡ ɹ  󣬻     
		if (size == sizeof(int64_t)) {
			memoryCache[address] = *static_cast<int64_t*>(buffer);  //      int64_t    ͵     
		}


		return (read_size == size);
	}



	template <typename T>
	T CachedRead(uint64_t address) {
		if constexpr (std::is_same<T, int64_t>::value) {
			if (memoryCache.find(address) != memoryCache.end()) {
				return memoryCache[address];
			}
		}
		else if constexpr (std::is_same<T, __m128>::value) {
			if (memoryCacheM128.find(address) != memoryCacheM128.end()) {
				return memoryCacheM128[address];
			}
		}
		T value = Read<T>(address);
		if constexpr (std::is_same<T, int64_t>::value) {
			memoryCache[address] = value;
		}
		else if constexpr (std::is_same<T, __m128>::value) {
			memoryCacheM128[address] = value;
		}
		return value;
	}
	inline void ClearCache() {
		memoryCache.clear();
		memoryCacheM128.clear();
	}
};

inline Memory mem;