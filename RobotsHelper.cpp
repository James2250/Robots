#include "DllInjector.h"

#include <Windows.h>
#include <iostream>
#include <TlHelp32.h> 
#include <tchar.h>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <fstream>

using namespace std;
using namespace chrono;

enum VERSION
{
	EU,
	US
};

VERSION GAMEVERSION;

unsigned int RodneyAddress, BallAddress, CamAddress, PauseAddress, GearAddress, TargetFPSAddress, CurrentFPSAddress, LevelAddress;

#define PosXOffset			0xD0
#define PosYOffset			0xD4
#define PosZOffset			0xD8

HANDLE hProc;

DWORD FaceXOffset = 0x00B0;
DWORD FaceZOffset = 0x00B8;
DWORD SpinOffset = 0x00E4;

float SavedX = 278.0168762f;
float SavedY = -7.163738728f;
float SavedZ = 7.143208027f;

float SavedBallX = 0.0f;
float SavedBallY = 0.0f;
float SavedBallZ = 0.0f;

float SavedSpinAngle = 0.0f;

float SpinAngle = 0.0f;
float Speed = 1.0f;

HANDLE handle;

DWORD clientdllBaseAddress;
DWORD ClocalPlayer;
DWORD ClocalPlayer2;

bool LoadKeyUp = true;
bool SaveKeyUp = true;
bool SpeedUpKey = true;
bool SpeedDownKey = true;
bool CoordsKeyUp = true;
bool GravityKeyUp = true;
bool HealthKeyUp = true;
bool SetValuesKey = true;
bool SetGearLimitKey = true;
bool FPSCheck = true;
bool FPSCap = true;

bool CustomFPS = false;
int NewFPS = 30.0f;

bool IsPaused = false;
bool InsideBall = false;

float BallX = 0.0f;
float BallY = 0.0f;
float BallZ = 0.0f;
float FrozenYBallPos = 0.0f;

float CamX = 0.0f;
float CamY = 0.0f;
float CamZ = 0.0f;

bool GearLimitRemoved = false;
bool DllLoaded = false;

template<class T>
void GetValueFromAddress(T& var, unsigned int address)
{
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + address), &var, sizeof(T), 0);

}

template<class T>
void GetValueFromAddress(T& var, unsigned int address, const std::vector<int>& offsets)
{
	std::uint32_t StartAddress = *(uint32_t*)(clientdllBaseAddress + address);
	uint32_t NextAdd = StartAddress;

	for (int i = 0; i < offsets.size() - 1; i++)
	{
		ReadProcessMemory(handle, (LPVOID)(NextAdd + offsets[i]), &NextAdd, sizeof(T), 0);
	}

	ReadProcessMemory(handle, (LPVOID)(NextAdd + offsets[offsets.size() - 1]), &var, sizeof(T), 0);
}

template<class T>
void SetValueAtAddress(T var, unsigned int address)
{
	WriteProcessMemory(handle, (LPVOID)(clientdllBaseAddress + address), &var, sizeof(T), 0);
}

template<class T>
void SetValueAtAddress(T var, unsigned int address, const std::vector<int>& offsets)
{
	std::uint32_t StartAddress;
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + address), &StartAddress, sizeof(uint32_t), 0);
	uint32_t NextAdd = StartAddress;

	for (int i = 0; i < offsets.size() - 1; i++)
	{
		ReadProcessMemory(handle, (LPVOID)(NextAdd + offsets[i]), &NextAdd, sizeof(T), 0);
	}

	WriteProcessMemory(handle, (LPVOID)(NextAdd + offsets[offsets.size() - 1]), &var, sizeof(T), 0);
}

void ReadRodneyFloat(float& x, DWORD Offset)
{
	std::uint32_t FinalAddress;
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + RodneyAddress), &FinalAddress, sizeof(float), NULL);
	ReadProcessMemory(handle, (LPVOID)(FinalAddress + Offset), &x, sizeof(float), NULL);
}

void WriteRodneyFloat(float x, DWORD Offset)
{
	std::uint32_t FinalAddress;
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + RodneyAddress), &FinalAddress, sizeof(float), NULL);
	WriteProcessMemory(handle, (LPVOID)(FinalAddress + Offset), &x, sizeof(float), NULL);
}

std::string GetLastErrorAsString() {
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}
	else {
		return std::system_category().message(errorMessageID);
	}
}

DWORD_PTR dwGetModuleBaseAddress(DWORD pID, TCHAR* szModuleName)
{
	DWORD_PTR dwModuleBaseAddress = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pID);

	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 ModuleEntry32;
		ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(hSnapshot, &ModuleEntry32))
		{
			do
			{
				if (strcmp(ModuleEntry32.szModule, szModuleName) == 0)
				{
					dwModuleBaseAddress = (DWORD_PTR)ModuleEntry32.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnapshot, &ModuleEntry32));
		}
		CloseHandle(hSnapshot);
	}
	return dwModuleBaseAddress;
}

void WriteBallXZ(float x, float z)
{
	DWORD add1, add2, add3;

	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + BallAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0x104), &add1, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add1 + 0x4C), &add2, sizeof(DWORD), NULL);

	WriteProcessMemory(handle, (LPVOID)(add2 + 0x58), &x, sizeof(float), NULL);
	WriteProcessMemory(handle, (LPVOID)(add2 + 0x60), &z, sizeof(float), NULL);
}

void WriteBallY(float y)
{
	SetValueAtAddress(y, BallAddress, { 0x8,0x114,0x40,0x5C });
}

void ReadCamValues()
{
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + CamAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0xD0), &CamX, sizeof(float), 0);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0xD4), &CamY, sizeof(float), 0);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0xD8), &CamZ, sizeof(float), 0);
}

void ReadBallValues()
{
	DWORD add1, add2, add3;
	InsideBall = true;

	//Ball Values
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + BallAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0x104), &add1, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add1 + 0x4C), &add2, sizeof(DWORD), NULL);

	ReadProcessMemory(handle, (LPVOID)(add2 + 0x58), &BallX, sizeof(float), NULL);
	ReadProcessMemory(handle, (LPVOID)(add2 + 0x60), &BallZ, sizeof(float), NULL);

	if (BallX == 0 && BallZ == 0)  // bad hack is bad
	{
		InsideBall = false;
		return;
	}

	InsideBall = true;

	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0x8), &add1, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add1 + 0x114), &add2, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add2 + 0x40), &add3, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add3 + 0x5C), &BallY, sizeof(float), NULL);
}

void ResetBallVel()
{
	WriteBallY(FrozenYBallPos);

	DWORD add1, add2;
	float Zero = 0.0f;

	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + BallAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + 0x104), &add1, sizeof(DWORD), NULL);
	ReadProcessMemory(handle, (LPVOID)(add1 + 0x4C), &add2, sizeof(DWORD), NULL);
	WriteProcessMemory(handle, (LPVOID)(add2 + 0x79), &Zero, sizeof(float), NULL);
	WriteProcessMemory(handle, (LPVOID)(add2 + 0x7C), &Zero, sizeof(float), NULL);
	WriteProcessMemory(handle, (LPVOID)(add2 + 0x80), &Zero, sizeof(float), NULL);
}

void GetPlayerValues(float& xPos, float& yPos, float& zPos, float& xFace, float& zFace)
{
	ReadBallValues();
	ReadCamValues();

	const float GearLimit = 9999.0f;

	if (GearLimitRemoved)
	{
		SetValueAtAddress(GearLimit, GearAddress, { 0x18 });
	}

	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + RodneyAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + FaceXOffset), &xFace, sizeof(float), 0);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + FaceZOffset), &zFace, sizeof(float), 0);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + PosXOffset), &xPos, sizeof(float), NULL);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + PosYOffset), &yPos, sizeof(float), NULL);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + PosZOffset), &zPos, sizeof(float), NULL);
	ReadProcessMemory(handle, (LPVOID)(ClocalPlayer + SpinOffset), &SpinAngle, sizeof(float), 0);

	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + PauseAddress), &IsPaused, sizeof(bool), 0);
}

string GetActiveWindowTitle()
{
	char wnd_title[256];
	HWND hwnd = GetForegroundWindow(); // get handle of currently active window
	GetWindowText(hwnd, wnd_title, sizeof(wnd_title));
	return wnd_title;
}

int GetRobotsVersion()
{
	char RandomByte = 0;
	ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + 0x2174E4), &RandomByte, sizeof(char), 0);  // random byte to check version

	if ((int)RandomByte == 85)  // US
	{
		GAMEVERSION = VERSION::US;

		RodneyAddress = 0x3B25AC;
		BallAddress = 0x003B25B0;
		CamAddress = 0x003B2C08;
		PauseAddress = 0x3B25A8;
		GearAddress = 0x003B30EC;
		TargetFPSAddress = 0x220020;
		CurrentFPSAddress = 0x220024;
		LevelAddress = 0x003B324C;

		return 1;
	}

	else if ((int)RandomByte == 73)  // EU
	{
		GAMEVERSION = VERSION::EU;

		RodneyAddress = 0x003BA7C0;
		BallAddress = 0x003BA7C4;
		CamAddress = 0x003BAED0;
		PauseAddress = 0x3BB478;
		GearAddress = 0x003BB3AC;
		TargetFPSAddress = 0x3AD258;
		CurrentFPSAddress = 0x3AD25C;
		LevelAddress = 0x003BB514;

		return 2;
	}

	return -1;
}

std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
std::chrono::system_clock::time_point b = std::chrono::system_clock::now();

void Run();

void CheckDll()
{
	std::ifstream file("RobotsDll.dll");

	if (file)
	{
		RunInjector();
		DllLoaded = true; 
	}
}

int main()
{
	HWND hWnd = FindWindowA(0, "Robots");
	cout << " -- Robots helper tool -- " << endl << endl;
	cout << "Credit to blopsblops for finding some of the memory addresses used" << endl;
	cout << "T to save position, Y to load position, G to freeze y position / disable yVel " << endl;
	cout << "Arrow keys to move position directly " << endl;
	cout << "Numpad 9 to remove gear limit " << endl;
	cout << "Numpad 7 / 4 to move up / down on y position" << endl;
	cout << "Numpad 6 to enter X/Y/Z " << endl;
	cout << "Numpad 5 to print out your position " << endl;
	cout << "Numpad 1 / 2 to increase / decrease move speed" << endl;
	cout << "Ctrl + H to give full health " << endl;
	cout << "F to set FPS cap, M to see current FPS" << endl;
	cout << "F1 to toggle info panel (using .dll).  F2 to change info text color " << endl << endl;

	//CheckDll();

	if (hWnd == 0)
	{
		std::cout << "Window not found.  Make sure the game is running and you are inside a stage" << std::endl;
		cin.get();
		return 0;
	}

	else
	{
		std::cout << "Window found" << endl;

		DWORD pID;
		GetWindowThreadProcessId(hWnd, &pID);

		TCHAR name[] = "Robots.exe";

		clientdllBaseAddress = 0;
		clientdllBaseAddress = dwGetModuleBaseAddress(pID, name);

		handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);

		if (clientdllBaseAddress == 0)
		{
			cout << "Base address not found, try restarting / running as admin" << endl;
			cin.get();
			return 0;
		}

		else
			cout << "Base address found " << endl;

		int ver = GetRobotsVersion();

		if (ver == -1)
		{
			while (true)
			{
				string v;

				cout << "Version not identified, enter US or EU " << endl;
				cin >> v;

				if (v == "US")
				{
					ver = 1;
					break;
				}

				if (v == "EU")
				{
					ver = 2;
					break;
				}
			}
		}

		if (ver == 1)
		{
			cout << "US Version " << endl << endl;
		}

		else if (ver == 2)
		{
			cout << "EU Version " << endl << endl;
		}

		Run();
	}

	return 0;
}

void Run()
{
	float FrozenYPos = 0.0f;
	bool FreezeYPos = false;

	float spinAngle = 0.0f;

	float xPos, yPos, zPos;
	float xFace, zFace;

	const double maxFPS = 60.0;
	const double maxPeriod = 1.0 / maxFPS;
	double lastTime = 0.0;

	bool OnWindow = true;

	while (true)
	{
		string WindowTitle = GetActiveWindowTitle();

		if (WindowTitle.find("Robots") != std::string::npos) // should find a better way one day
			OnWindow = true;

		else
			OnWindow = false;

		auto  ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
		double time = std::chrono::duration<double>(ms).count();
		double deltaTime = time - lastTime;

		if (deltaTime >= maxPeriod)
		{
			lastTime = time;

			GetPlayerValues(xPos, yPos, zPos, xFace, zFace);

			if (!IsPaused && OnWindow)
			{
				if (!DllLoaded)
				{
					ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + RodneyAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

					if (ClocalPlayer)  // don't do on menu
					{
						CheckDll();
					}
				}

				if (GetAsyncKeyState(VK_UP))
				{
					if (!InsideBall)
					{
						float FinalX = xPos + (xFace * 0.1f * Speed);
						float FinalZ = zPos + (zFace * 0.1f * Speed);

						WriteRodneyFloat(FinalX, PosXOffset);
						WriteRodneyFloat(FinalZ, PosZOffset);
					}

					else
					{
						float ForwardCamX = xPos - CamX;
						float ForwardCamY = yPos - CamY;
						float ForwardCamZ = zPos - CamZ;

						float Length = sqrt((ForwardCamX * ForwardCamX) + (ForwardCamY * ForwardCamY) + (ForwardCamZ * ForwardCamZ));
						ForwardCamX /= Length;
						ForwardCamZ /= Length; //Cam vector + normalizing 

						WriteBallXZ(xPos + (ForwardCamX * 0.1f * Speed), zPos + (ForwardCamZ * 0.1f * Speed));
					}
				}

				if (GetAsyncKeyState(VK_DOWN))
				{
					if (!InsideBall)
					{
						float FinalX = xPos - (xFace * 0.1f * Speed);
						float FinalZ = zPos - (zFace * 0.1f * Speed);

						WriteRodneyFloat(FinalX, PosXOffset);
						WriteRodneyFloat(FinalZ, PosZOffset);
					}

					else
					{
						float ForwardCamX = CamX - xPos;
						float ForwardCamZ = CamZ - zPos;

						float Length = sqrt((ForwardCamX * ForwardCamX) + (ForwardCamZ * ForwardCamZ));
						ForwardCamX /= Length;
						ForwardCamZ /= Length;  //Cam vector + normalizing 

						WriteBallXZ(xPos + (ForwardCamX * 0.1f * Speed), zPos + (ForwardCamZ * 0.1f * Speed));
					}
				}

				if (GetAsyncKeyState(VK_LEFT))
				{
					if (!InsideBall)
						WriteRodneyFloat(SpinAngle - 0.1f, SpinOffset);

					else
					{
						float ForwardCamX = CamX - xPos;
						float ForwardCamZ = CamZ - zPos;

						float Length = sqrt((ForwardCamX * ForwardCamX) + (ForwardCamZ * ForwardCamZ));
						ForwardCamX /= Length;
						ForwardCamZ /= Length;  //Cam vector + normalizing 

						WriteBallXZ(xPos + (ForwardCamZ * 0.1f * Speed), zPos + (-ForwardCamX * 0.1f));
					}
				}

				if (GetAsyncKeyState(VK_RIGHT))
				{
					if (!InsideBall)
						WriteRodneyFloat(SpinAngle + 0.1f, SpinOffset);

					else
					{
						float ForwardCamX = xPos - CamX;
						float ForwardCamZ = zPos - CamZ;

						float Length = sqrt((ForwardCamX * ForwardCamX) + (ForwardCamZ * ForwardCamZ));
						ForwardCamX /= Length;
						ForwardCamZ /= Length;  //Cam vector + normalizing 

						WriteBallXZ(xPos + (ForwardCamZ * 0.1f), zPos + (-ForwardCamX * 0.1f));
					}
				}
			}

			if (!GetAsyncKeyState(VK_NUMPAD1))
				SpeedUpKey = true;

			if (GetAsyncKeyState(VK_NUMPAD1))
			{
				if (SpeedUpKey)
				{
					cout << "Speed Up " << endl;
					Speed += 0.1f;
				}

				SpeedUpKey = false;
			}

			if (!GetAsyncKeyState(VK_NUMPAD2))
				SpeedDownKey = true;

			if (GetAsyncKeyState(VK_NUMPAD2))
			{
				if (SpeedDownKey)
				{
					cout << "Speed Down " << endl;
					Speed -= 0.1f;
				}

				SpeedDownKey = false;
			}

			if (!GetAsyncKeyState(VK_NUMPAD5))
				CoordsKeyUp = true;

			if (GetAsyncKeyState(VK_NUMPAD5))
			{
				if (CoordsKeyUp)
				{
					if (!InsideBall)
					{
						cout << "X: " << xPos << endl;
						cout << "Y: " << yPos << endl;
						cout << "Z: " << zPos << endl << endl;
					}

					else
					{
						cout << "X: " << BallX << endl;
						cout << "Y: " << BallY << endl;
						cout << "Z: " << BallZ << endl << endl;

					}

					CoordsKeyUp = false;
				}
			}

			if (!GetAsyncKeyState(VK_NUMPAD6))
				SetValuesKey = true;

			if (GetAsyncKeyState(VK_NUMPAD6))
			{
				if (SetValuesKey)
				{
					char Values[3] = { 'X', 'Y', 'Z' };
					float NewPositions[3];
					float* OldPositions[3] = { &xPos,&yPos,&zPos };

					FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

					for (int i = 0; i < 3; i++)
					{
						cout << "Enter " << Values[i] << endl;
						string temp;
						getline(cin, temp);

						if (temp.empty())
						{
							cout << "Using current " << Values[i] << endl;
							NewPositions[i] = *OldPositions[i];
						}

						else
						{
							NewPositions[i] = atoi(temp.c_str());
						}
					}

					if (!InsideBall)
					{
						WriteRodneyFloat(NewPositions[0], PosXOffset);
						WriteRodneyFloat(NewPositions[1], PosYOffset);
						WriteRodneyFloat(NewPositions[2], PosZOffset);
					}

					else
					{
						WriteBallXZ(NewPositions[0], NewPositions[2]);
						WriteBallY(NewPositions[1]);
					}

					cout << "Loaded values " << endl;
				}

				SetValuesKey = false;
			}


			if (GetAsyncKeyState(VK_NUMPAD7))
			{
				if (!InsideBall)
				{
					float FinalY = yPos + 0.1f * Speed;
					FrozenYPos += 0.1f * Speed;
					WriteRodneyFloat(FinalY, PosYOffset);
				}

				else
				{
					float FinalY = BallY + 0.1f * Speed;
					FrozenYBallPos += 0.1f * Speed;
					WriteBallY(FinalY);
				}
			}

			if (!GetAsyncKeyState(VK_NUMPAD9))
				SetGearLimitKey = true;

			if (GetAsyncKeyState(VK_NUMPAD9))
			{
				if (SetGearLimitKey)
				{
					GearLimitRemoved = !GearLimitRemoved;

					if (GearLimitRemoved)
						cout << "Gear Limit Removed " << endl;

					else
					{
						cout << "Gear Limit Set " << endl;
						int GearLimit = 200.0f;
						SetValueAtAddress(GearLimit, GearAddress, { 0x18 });
					}
				}
				SetGearLimitKey = false;
			}

			if (GetAsyncKeyState(VK_NUMPAD4))
			{
				if (!InsideBall)
				{
					float FinalY = yPos - 0.1f * Speed;
					FrozenYPos -= 0.1f * Speed;

					WriteRodneyFloat(FinalY, PosYOffset);
				}

				else
				{
					float FinalY = BallY + 0.1f * Speed;
					FrozenYBallPos -= 0.1f * Speed;

					WriteBallY(FinalY);
				}
			}

			if (!GetAsyncKeyState(0x54))
				SaveKeyUp = true;

			if (GetAsyncKeyState(0x48)) // h
			{
				if (HealthKeyUp)
				{
					HealthKeyUp = false;

					if (GetAsyncKeyState(VK_CONTROL))
					{
						SetValueAtAddress(100.0f, RodneyAddress, { 0x5C, 0x2F8 });
					}
				}
			}

			if (!GetAsyncKeyState(0x48)) // h
			{
				HealthKeyUp = true;
			}

			if (CustomFPS)
			{
				ReadProcessMemory(handle, (LPVOID)(clientdllBaseAddress + RodneyAddress), &ClocalPlayer, sizeof(ClocalPlayer), 0);

				if (ClocalPlayer)  // don't do on menu
				{
					if (GAMEVERSION == VERSION::EU)
					{
						SetValueAtAddress(NewFPS, TargetFPSAddress);
						SetValueAtAddress(NewFPS * 2, 0x68D51); // some levels force FPS back every frame, overriding that
					}

					if (GAMEVERSION == VERSION::US)
					{
						SetValueAtAddress(NewFPS, TargetFPSAddress);
						SetValueAtAddress(NewFPS * 2, 0x33F91); // some levels force FPS back every frame, overriding that
					}
				}
			}

			if (GetAsyncKeyState(0x46)) // f
			{
				if (FPSCap)
				{
					FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
					FPSCap = false;

					cout << "Enter new FPS cap (The game will try and force it to stay above 30)" << endl;
					cin >> NewFPS;
					cout << "FPS Cap Updated " << endl << endl;

					CustomFPS = true;
				}
			}

			if (!GetAsyncKeyState(0x46)) // f
				FPSCap = true;

			if (GetAsyncKeyState(0x4D)) // m
			{
				if (FPSCheck)
				{
					FPSCheck = false;

					int FPS = 30;
					GetValueFromAddress(FPS, CurrentFPSAddress);
					cout << "FPS: " << FPS << endl << endl;

				}
			}

			if (!GetAsyncKeyState(0x4D)) // m
				FPSCheck = true;

			if (GetAsyncKeyState(0x54) && OnWindow)  // T, save
			{
				if (SaveKeyUp)
				{
					if (!InsideBall)
					{
						SavedX = xPos;
						SavedY = yPos;
						SavedZ = zPos;

						ReadRodneyFloat(SavedSpinAngle, SpinOffset);
					}

					else
					{
						SavedBallX = BallX;
						SavedBallY = BallY;
						SavedBallZ = BallZ;

						cout << SavedBallX << endl;
						cout << SavedBallY << endl;
						cout << SavedBallZ << endl;

					}

					cout << "Saved " << endl;
					SaveKeyUp = false;
				}
			}

			if (!GetAsyncKeyState(0x59))
				LoadKeyUp = true;

			if (GetAsyncKeyState(0x59) && OnWindow) // Y, load
			{
				if (LoadKeyUp)
				{
					if (!InsideBall)
					{
						WriteRodneyFloat(SavedX, PosXOffset);
						WriteRodneyFloat(SavedY, PosYOffset);
						WriteRodneyFloat(SavedZ, PosZOffset);

						WriteRodneyFloat(SavedSpinAngle, SpinOffset);

						FrozenYPos = SavedY;
					}

					else
					{
						WriteBallXZ(SavedBallX, SavedBallZ);
						WriteBallY(SavedBallY);

						FrozenYBallPos = SavedBallY;
						ResetBallVel();
					}

					cout << "Loaded " << endl;
					LoadKeyUp = false;
				}
			}

			if (!GetAsyncKeyState(0x47))
				GravityKeyUp = true;

			if (GetAsyncKeyState(0x47) && OnWindow)  // g, 
			{
				if (GravityKeyUp)
				{
					FreezeYPos = !FreezeYPos;

					if (!InsideBall)
					{
						if (FreezeYPos)
						{
							FrozenYPos = yPos;
						}

						SetValueAtAddress(0, RodneyAddress, { 0x118,0x40,0x124, });
					}

					else
					{
						if (FreezeYPos)
						{
							FrozenYBallPos = BallY;
						}
					}
					GravityKeyUp = false;
				}
			}

			if (FreezeYPos)
			{
				if (!InsideBall)
				{
					WriteRodneyFloat(FrozenYPos, PosYOffset);
					SetValueAtAddress(0, RodneyAddress, { 0x118,0x40,0x124});  // zero yVel
				}

				else
				{
					ResetBallVel();
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));  // don't eat CPU
		}
	}
}
