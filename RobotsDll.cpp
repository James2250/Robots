//https://github.com/Nukem9/detours

#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <fstream>
#include <string>
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>
#include "stdint.h"
#include <chrono>
#include <ctime>    

#include <iomanip>
#include <sstream>

#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

DWORD* vTable;
LPD3DXFONT pFont;
static volatile LPDIRECT3DDEVICE9 pDevice;

unsigned int RodneyAddress, BallAddress, CurrentFPSAddress;

bool ShowMenuButton = false;
bool TextColorButton = false;

bool ShowMenu = false;
bool TextWhite = true;
DWORD TextColor = D3DCOLOR_ARGB(255, 255, 255, 255);

template<class T>
void GetValueFromAddress(T &var, std::uint32_t address)
{
    std::uint32_t Base = (std::uint32_t)GetModuleHandleA(NULL);

    if (IsBadReadPtr((VOID*)(Base + address), sizeof(std::uint32_t)))
    {
        var = 0; 
        return; 
    }

    var = *(T*)(Base + address);
    //return (Base + address);
}

template<class T> 
void GetValueFromAddress(T &var, std::uint32_t address, const std::vector<unsigned int> &offsets, bool nom = false)
{
    std::uint32_t Base = (std::uint32_t)GetModuleHandleA(NULL);
    std::uint32_t StartAddress = *(uint32_t*)(Base + address);

    if (IsBadReadPtr((VOID*)StartAddress, sizeof(std::uint32_t)))
    {
        var = 0;
        return; 
    }

    if (offsets.size() == 1)
    {
        if (IsBadReadPtr((VOID*)(StartAddress + offsets[0]), sizeof(std::uint32_t)))
        {
            var = 0;
            return; 
        }

        var = *(T*)(StartAddress + offsets[0]);
        //return StartAddress + offsets[0];
    }

    uint32_t NewAddress = 0;
    uint32_t NextAdd = StartAddress;

    for (int i = 0; i < offsets.size() - 1; i++)
    {
        NewAddress = NextAdd + offsets[i];

        if (IsBadReadPtr((VOID*)NewAddress, sizeof(std::uint32_t)))
        {
            var = 0;
            return; 
        }

        NextAdd = *(uint32_t*)NewAddress;
    }

    if (IsBadReadPtr((VOID*)(NextAdd + offsets[offsets.size() - 1]), sizeof(std::uint32_t)))
    {
        var = 0;
        return; 
    }

    var = *(T*)(NextAdd + offsets[offsets.size() - 1]);
    //return NextAdd + offsets[offsets.size() - 1];
}

template<class T>
std::string NumToString(T num)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(3) << num;
    return stream.str(); 
}

template<class T>
void Print(int x, int y, T var, std::string label, DWORD color)
{
    RECT rect;
    SetRect(&rect, x, y + 20, 40, 20);

    label += NumToString(var);
    pFont->DrawTextA(NULL, label.c_str(), -1, &rect, DT_NOCLIP, TextColor);
}

void GetRobotsVersion()
{
    char RandomByte = 0;

    std::uint32_t Base = (std::uint32_t)GetModuleHandleA(NULL);
    RandomByte = *(char*)(Base + 0x2174E4);

    if ((int)RandomByte == 85)  // US
    {
        RodneyAddress = 0x3B25AC;
        BallAddress = 0x003B25B0;
        CurrentFPSAddress = 0x220024;
    }

    else if ((int)RandomByte == 73)  // EU
    {
        RodneyAddress = 0x003BA7C0;
        BallAddress = 0x003BA7C4;
        CurrentFPSAddress = 0x3AD25C;

    }
}

VOID WriteText(LPDIRECT3DDEVICE9 pDevice, INT x, INT y, DWORD color)
{
    if (GetAsyncKeyState(VK_F1))
    {
        if (ShowMenuButton)
        {
            ShowMenu = !ShowMenu;
            ShowMenuButton = false;
        }
    }

    if (!GetAsyncKeyState(VK_F1))
    {
        ShowMenuButton = true;
    }

    if (GetAsyncKeyState(VK_F2))
    {
        if (TextColorButton)
        {
            if (TextWhite)
                TextColor = D3DCOLOR_ARGB(255, 255, 255, 255);

            else
                TextColor = D3DCOLOR_ARGB(255, 0, 0, 0);

            TextWhite = !TextWhite;
            TextColorButton = false;
        }
    }

    if (!GetAsyncKeyState(VK_F2))
    {
        TextColorButton = true;
    }

    if (ShowMenu)
    {
        std::uint32_t Base = (std::uint32_t)GetModuleHandleA(NULL);
        DWORD LocalPlayer = *(std::uint32_t*)(Base + RodneyAddress);

        if (!LocalPlayer)
        {
            ShowMenu = false; 
            return;
        }
        
        std::string text = "";

        float xPos = 0, yPos = 0, zPos = 0, xBall = 0, yBall = 0, zBall = 0;
        float xVel = 0, yVel = 0, zVel = 0, xVel2, yVel2, zVel2, xVelBall, yVelBall, zVelBall, Speed = 0;
        int FPS = 30.0;

        GetValueFromAddress(xPos, RodneyAddress, { 0xD0 });
        GetValueFromAddress(yPos, RodneyAddress, { 0xD4 });
        GetValueFromAddress(zPos, RodneyAddress, { 0xD8 });

        if (xPos || yPos)  // lazy way to make sure we are in a stage 
        {
            GetValueFromAddress(xVel, RodneyAddress, { 0x118,0x40,0x1B8 });
            GetValueFromAddress(yVel, RodneyAddress, { 0x118,0x40,0x124 });
            GetValueFromAddress(zVel, RodneyAddress, { 0x118,0x40,0x1c0 });

            GetValueFromAddress(xVel2, RodneyAddress, { 0x118,0x40,0x2C });
            GetValueFromAddress(yVel2, RodneyAddress, { 0x118,0x40,0x30 });
            GetValueFromAddress(zVel2, RodneyAddress, { 0x118,0x40,0x34 });

            GetValueFromAddress(xBall, BallAddress, { 0x104,0x4C,0x58 });
            GetValueFromAddress(yBall, BallAddress, { 0x8, 0x114,0x40, 0x5C });
            GetValueFromAddress(zBall, BallAddress, { 0x104,0x4C,0x60 });

            GetValueFromAddress(xVelBall, BallAddress, { 0x104,0x4C,0x78 });
            GetValueFromAddress(yVelBall, BallAddress, { 0x104, 0x4C,0x7C });
            GetValueFromAddress(zVelBall, BallAddress, { 0x104,0x4C,0x80 });

            GetValueFromAddress(FPS, CurrentFPSAddress);
            GetValueFromAddress(Speed, RodneyAddress, { 0x5C, 0x4E0 });

            Print(x, y,       Speed, "Speed: ", color);
            Print(x + 150, y, FPS,   "FPS: ",   color);

            if (xBall == 0)
            {
                Print(x,     y + 20, xPos, "XPos: ", color);
                Print(x+150, y + 20, yPos, "YPos: ", color);
                Print(x+300, y + 20, zPos, "ZPos: ", color);
                
                Print(x,       y + 40, xVel, "xVel: ", color);
                Print(x + 150, y + 40, yVel, "yVel: ", color);
                Print(x + 300, y + 40, zVel, "zVel: ", color);

                Print(x,       y + 60, xVel2, "xVel (Air): ", color);
                Print(x + 150, y + 60, yVel2, "yVel (2): ",   color);
                Print(x + 300, y + 60, zVel2, "zVel (Air): ", color);
            }

            else
            {
                Print(x,       y + 20, xBall, "XPos: ", color);
                Print(x + 150, y + 20, yBall, "YPos: ", color);
                Print(x + 300, y + 20, zBall, "ZPos: ", color);

                Print(x,       y + 40, xVelBall, "xVel: ", color);
                Print(x + 150, y + 40, yVelBall, "yVel: ", color);
                Print(x + 300, y + 40, zVelBall, "zVel: ", color);
            }
        }
    }
}

typedef HRESULT(__stdcall* f_EndScene)(IDirect3DDevice9* pDevice);
f_EndScene pTrp_EndScene;

HRESULT __stdcall hk_EndScene(IDirect3DDevice9* pDevice)
{
    D3DXCreateFont(pDevice, 20, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &pFont);
    WriteText(pDevice, 200, 0, D3DCOLOR_ARGB(255, 255, 255, 255));

    pFont->Release();
    return pTrp_EndScene(pDevice);

}

void Hook()
{
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
        return;

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetForegroundWindow();
    d3dpp.Windowed = TRUE;

    IDirect3DDevice9* pDummyDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);
    if (FAILED(hr) || !pDummyDevice)
    {
        pD3D->Release();
        return;
    }

    void** pVTable = *reinterpret_cast<void***>(pDummyDevice);

    GetRobotsVersion();

    pDummyDevice->Release();
    pD3D->Release();

    //https://github.com/Nukem9/detours
    pTrp_EndScene = (f_EndScene)Detours::X86::DetourFunction((uintptr_t)pVTable[42], (uintptr_t)hk_EndScene);

}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Hook, NULL, NULL, NULL);
    }
    return TRUE;
}