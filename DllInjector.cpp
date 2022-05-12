#include "DllInjector.h"

#include <windows.h>
#include <tchar.h>
#include <malloc.h>
#include <iostream>

using namespace std;

#pragma warning(disable : 4996)

void RunInjector()
{
	char filename[] = "RobotsDll.dll";
	char fullFilename[1024];

	GetFullPathName(filename, MAX_PATH, fullFilename, nullptr);

	LPCSTR DllPath = fullFilename;
	HWND hWnd = FindWindowA(0, "Robots");

	DWORD pID;
	GetWindowThreadProcessId(hWnd, &pID);
	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);

	if (!handle)
		cout << "Dll Handle Failed " << endl;

	// Allocate memory for the dllpath in the target process, length of the path string + null terminator
	LPVOID pDllPath = VirtualAllocEx(handle, 0, strlen(DllPath) + 1, MEM_COMMIT, PAGE_READWRITE);

	if (!pDllPath)
	{
		cout << "Failed to allocate memory for dll " << endl;
		return; 
	}

	// Write the path to the address of the memory we just allocated in the target process
	WriteProcessMemory(handle, pDllPath, (LPVOID)DllPath, strlen(DllPath) + 1, 0);

	// Create a Remote Thread in the target process which calls LoadLibraryA as our dllpath as an argument -> program loads our dll
	HANDLE hLoadThread = CreateRemoteThread(handle, 0, 0,
		(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA"), pDllPath, 0, 0);

	WaitForSingleObject(hLoadThread, INFINITE); // Wait for the execution of our loader thread to finish
	cout << "Dll path allocated at: " << hex << pDllPath << endl;
	VirtualFreeEx(handle, pDllPath, strlen(DllPath) + 1, MEM_RELEASE); // Free the memory allocated for our dll path
}
