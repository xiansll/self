#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace M {
	template <typename T = std::uint8_t>
	T* abs(T* relative_address, int pre_offset = 0x0, int post_offset = 0x0)
	{
		relative_address += pre_offset;
		relative_address += sizeof(std::int32_t) + *reinterpret_cast<std::int32_t*>(relative_address);
		relative_address += post_offset;
		return relative_address;
	}
	std::uint8_t* FindPattern(const char* module_name, const std::string& byte_sequence);

	inline std::uint8_t* ResolveRelativeAddress(std::uint8_t* nAddressBytes, std::uint32_t nRVAOffset, std::uint32_t nRIPOffset)
	{
		std::uint32_t nRVA = *reinterpret_cast<std::uint32_t*>(nAddressBytes + nRVAOffset);
		std::uint64_t nRIP = reinterpret_cast<std::uint64_t>(nAddressBytes) + nRIPOffset;

		return reinterpret_cast<std::uint8_t*>(nRVA + nRIP);
	}

	inline std::uint8_t* ToAbsolute(std::uint8_t* at)
	{
		const int relative = *reinterpret_cast<int*>(at);
		return at + relative + sizeof(int);
	}

	template <typename T = std::uint8_t>
	T* GetAbsoluteAddress(T* relative_address, int pre_offset = 0x0, int post_offset = 0x0)
	{
		relative_address += pre_offset;
		relative_address += sizeof(std::int32_t) + *reinterpret_cast<std::int32_t*>(relative_address);
		relative_address += post_offset;
		return relative_address;
	}

	template <typename T>
	inline T GetVFunc(void* thisptr, std::size_t index)
	{
		return (*reinterpret_cast<T**>(thisptr))[index];
	}

	uint8_t* scan_absolute(const char* module_name, const char* pattern);

	uint8_t* scan_absolute(const char* module_name, const char* pattern, int pre_offset);

	uint8_t* scan_absolute(const char* module_name, const char* pattern, int pre_offset, int post_offset);

	uintptr_t patternScan(const std::string& module, const std::string& pattern);

	uint8_t* scan(const char* module_name, const char* pattern);
}