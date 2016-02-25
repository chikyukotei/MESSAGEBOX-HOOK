#include <Windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <tchar.h>
#include "environment.h"

/**
* @IsWindowsNT
* �V�X�e����WindowsNT���ǂ����𒲂ׂ�
* @brief �V�X�e����WindowsNT���ǂ����𒲂ׂ�
* @return BOOL�^ NT�Ȃ�true, �����łȂ����false
* @detail �ڍ�
*/
BOOL IsWindowsNT()
{
	OSVERSIONINFO ver;

	#if 1600<=_MSC_VER	//	Visual C++ 2010�ȏ�
	#pragma warning(push)
	#pragma warning(disable : 4996)	// GetVersionEx���Â��`���Ƃ��Đ錾����܂��������
	#endif

	// check current version of Windows
	DWORD version = GetVersionEx(&ver);

	// parse return
	DWORD majorVersion = (DWORD)(LOBYTE(LOWORD(version)));
	DWORD minorVersion = (DWORD)(HIBYTE(LOWORD(version)));
	return (version < 0x80000000);
}

/**
* @injectDLL
* �Y���v���Z�X�ɔC�ӂ�DLL��ǂݍ��܂���
* @brief �Y���v���Z�X�ɔC�ӂ�DLL��ǂݍ��܂���
* @param (DWORD ProcessID) �v���Z�X��ID
* @return BOOL�^ ���������true, ���s�����false
* @detail 
* CreateRemoteThread Method
*/
BOOL InjectDLL(DWORD ProcessID)
{
	HANDLE Proc, createdThread;
	TCHAR buf[50] = { 0 };
	LPVOID RemoteString, LoadLibAddy;

	if (!ProcessID) {
		MessageBox(NULL, TEXT("Target process not found"), TEXT("Error"), NULL);
		return FALSE;
	}

	Proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, ProcessID);
	if (!Proc) {
		_stprintf_s(buf, TEXT("OpenProcess() failed: %d"), GetLastError());
		MessageBox(NULL, buf, TEXT("Error"), NULL);
		return FALSE;
	}

	LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "LoadLibraryA");
	if (!LoadLibAddy) {
		_stprintf_s(buf, TEXT("GetProcAddress() failed: %d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("Error"), NULL);
		return FALSE;
	}

	// Allocate space in the process for the dll
	RemoteString = (LPVOID)VirtualAllocEx(Proc, NULL, strlen(PSEUDO_DLL), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!RemoteString) {
		_stprintf_s(buf, TEXT("VirtualAllocEx() failed: %d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("Error"), NULL);
		return FALSE;
	}

	// Write the string name of the dll in the memory allocated 
	if (!WriteProcessMemory(Proc, (LPVOID)RemoteString, PSEUDO_DLL, strlen(PSEUDO_DLL), NULL)) {
		_stprintf_s(buf, TEXT("WriteProcessMemory() failed: %d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("Error"), NULL);
		return FALSE;
	}

	// Load the dll
	createdThread = CreateRemoteThread(Proc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddy, (LPVOID)RemoteString, NULL, NULL);
	if (!createdThread) {
		_stprintf_s(buf, TEXT("CreateRemoteThread() failed: %d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("Error"), NULL);
		return FALSE;
	}

	WaitForSingleObject(createdThread, INFINITE);

	// Free the memory that is not being using anymore. 
	if (RemoteString != NULL) VirtualFreeEx(Proc, RemoteString, 0, MEM_RELEASE);
	if (createdThread != NULL) CloseHandle(createdThread);
	if (Proc != NULL) CloseHandle(Proc);

	return TRUE;
}

/**
* @findProcess
* ���s���̃v���Z�X����Y������v���Z�X��ID��Ԃ�
* @brief ���s���̃v���Z�X����Y������v���Z�X��ID��Ԃ�
* @param (TCHAR* exe_name) exe�t�@�C���̖��O
* @return DWORD�^ ��������΃v���Z�X��ID, ���s������-1
* @detail �ڍׂȐ���
*/
DWORD FindProcess(TCHAR* exe_name)
{
	DWORD targetProcId = 0; //target process id

	//search target process
	HANDLE hSnapShot;
	if ((hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE) {
		//CreateToolhelp32Snapshot Error
		OutputDebugString(TEXT("CreateToolhelp32Snapshot"));
		return 0;
	}

	PROCESSENTRY32 pEntry;
	pEntry.dwSize = sizeof(pEntry);
	BOOL result = Process32First(hSnapShot, &pEntry);
	while (result) {
		if (_tcscmp(exe_name, pEntry.szExeFile) == 0) {
			targetProcId = pEntry.th32ProcessID;
			break;
		}
		result = Process32Next(hSnapShot, &pEntry);
	}

	CloseHandle(hSnapShot);

	return targetProcId;
}

/**
* @main
* �����̃��C��
* @brief �����̃��C��
* @param (void)
* @return int 0
* @detail �ڍׂȐ���
*/
int main(void)
{
	if (!IsWindowsNT) {
		OutputDebugString(TEXT("This system is not WindowsNT"));
		return 1;
	}

	DWORD targetProcID = FindProcess(TARGET_EXE);

	// injection
	if (!InjectDLL(targetProcID)) {
		OutputDebugString(TEXT("inject fail"));
	}

	return 0;
}