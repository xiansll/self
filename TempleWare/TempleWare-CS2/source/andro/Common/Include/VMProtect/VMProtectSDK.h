#pragma once
// STUB: VMProtect neutralized for TempleWare integration (no VMProtectSDK*.lib).
// All protection/licensing calls are inline no-ops; string "decrypt" is identity.
#define VMP_WCHAR wchar_t

#ifdef __cplusplus
extern "C" {
#endif

inline void VMProtectBegin(const char*) {}
inline void VMProtectBeginVirtualization(const char*) {}
inline void VMProtectBeginMutation(const char*) {}
inline void VMProtectBeginUltra(const char*) {}
inline void VMProtectBeginVirtualizationLockByKey(const char*) {}
inline void VMProtectBeginUltraLockByKey(const char*) {}
inline void VMProtectEnd(void) {}

inline bool VMProtectIsProtected() { return false; }
inline bool VMProtectIsDebuggerPresent(bool) { return false; }
inline bool VMProtectIsVirtualMachinePresent(void) { return false; }
inline bool VMProtectIsValidImageCRC(void) { return true; }
inline const char* VMProtectDecryptStringA(const char* value) { return value; }
inline const VMP_WCHAR* VMProtectDecryptStringW(const VMP_WCHAR* value) { return value; }
inline bool VMProtectFreeString(const void*) { return true; }

enum VMProtectSerialStateFlags
{
	SERIAL_STATE_SUCCESS				= 0,
	SERIAL_STATE_FLAG_CORRUPTED			= 0x00000001,
	SERIAL_STATE_FLAG_INVALID			= 0x00000002,
	SERIAL_STATE_FLAG_BLACKLISTED		= 0x00000004,
	SERIAL_STATE_FLAG_DATE_EXPIRED		= 0x00000008,
	SERIAL_STATE_FLAG_RUNNING_TIME_OVER	= 0x00000010,
	SERIAL_STATE_FLAG_BAD_HWID			= 0x00000020,
	SERIAL_STATE_FLAG_MAX_BUILD_EXPIRED	= 0x00000040,
};

#pragma pack(push, 1)
typedef struct { unsigned short wYear; unsigned char bMonth; unsigned char bDay; } VMProtectDate;
typedef struct
{
	int nState; VMP_WCHAR wUserName[256]; VMP_WCHAR wEMail[256];
	VMProtectDate dtExpire; VMProtectDate dtMaxBuild; int bRunningTime;
	unsigned char nUserDataLength; unsigned char bUserData[255];
} VMProtectSerialNumberData;
#pragma pack(pop)

inline int  VMProtectSetSerialNumber(const char*) { return SERIAL_STATE_SUCCESS; }
inline int  VMProtectGetSerialNumberState() { return SERIAL_STATE_SUCCESS; }
inline bool VMProtectGetSerialNumberData(VMProtectSerialNumberData*, int) { return false; }
inline int  VMProtectGetCurrentHWID(char*, int) { return 0; }

enum VMProtectActivationFlags
{
	ACTIVATION_OK = 0, ACTIVATION_SMALL_BUFFER, ACTIVATION_NO_CONNECTION,
	ACTIVATION_BAD_REPLY, ACTIVATION_BANNED, ACTIVATION_CORRUPTED,
	ACTIVATION_BAD_CODE, ACTIVATION_ALREADY_USED, ACTIVATION_SERIAL_UNKNOWN,
	ACTIVATION_EXPIRED, ACTIVATION_NOT_AVAILABLE
};

inline int VMProtectActivateLicense(const char*, char*, int) { return ACTIVATION_OK; }
inline int VMProtectDeactivateLicense(const char*) { return ACTIVATION_OK; }
inline int VMProtectGetOfflineActivationString(const char*, char*, int) { return ACTIVATION_OK; }
inline int VMProtectGetOfflineDeactivationString(const char*, char*, int) { return ACTIVATION_OK; }

#ifdef __cplusplus
}
#endif
