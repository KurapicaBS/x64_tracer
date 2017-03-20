#ifndef _TEST_H
#define _TEST_H

#include "pluginmain.h"
#include <string>

//plugin sub-menu identifiers
#define MENU_Start 0
#define MENU_Stop 1
#define MENU_Save 2
#define MENU_Abandon 3

//Name of the plugin module, used later to get the hinstance of the plugin module
//which will be used to load the dialog resources
#if defined( _WIN64 )
#define plugin_DLLname "Ktracer.dp64"
#else
#define plugin_DLLname "Ktracer.dp32"
#endif

//Interval of each step
#define TimerStepMs 10

//NT header size for 32 and 64 bit PE files
#define NT32dataSize 0xF8 
#define NT64dataSize 0x108
//NT section size in PE files
#define NTSectionSize 0x28

//buffer used to read the NT header from the target module
#if defined( _WIN64 )
static unsigned char NTdata [NT64dataSize];
#else
static unsigned char NTdata [NT32dataSize];
#endif

//Pointer to an array of section headers, will be used to calc the file offset
static unsigned char* SectionHeadersData;


using namespace std;






//functions
void testInit(PLUG_INITSTRUCT* initStruct);
void testStop();
void testSetup();

//Dialog callback procedure
INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
//used to show the main dialog of the plugin
void ShowForm(HINSTANCE hInstance, HINSTANCE hPrevInstance);
//checks if end VA is valid
bool CheckENDAddress(HWND hWnd);
//shows the save file dialog and returns true if OK is pressed
bool SaveFileDialog(char Buffer[MAX_PATH]);

//Convert numbers into HEX strings
string Int32ToHexString ( int Number );
string Int64ToHexString ( long long Number );
string Int64ToHexString ( INT64* Number );

//Plugin callback procedure from the host
void MyCallBack (CBTYPE cbType, void* callbackinfo);


DWORD VAtoRVA(duint VA, duint ImageBase);
DWORD VAtoRVA(UINT32 VA, UINT32 ImageBase);
ULONG RVA2FOA(DWORD ulRVA, PIMAGE_NT_HEADERS32 pNTHeader, IMAGE_SECTION_HEADER* SectionHeaders );
void InitModuleNTData(INT64 TargetImageBase);
DWORD VAtoFileOffset(UINT32 VA, UINT32 ImageBase);
DWORD VAtoFileOffset(duint VA, duint ImageBase);

//Timers call back functions which send a command to the host
VOID CALLBACK TimerProcSingleStep(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime);
VOID CALLBACK TimerProcStepOut(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) ;


#endif // _TEST_H
