#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#include "Tracing.h"
#include "pluginsdk\TitanEngine\TitanEngine.h"
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "icons.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "resource.h"
#include <Commdlg.h>
#include <exception>


using namespace std;

//Plugin module hinstance, used to locate resources
HINSTANCE hInst;
//used to set the main dialog icon
HICON main_icon;


//Global FLAG
bool LoggingActive;
//Events list
vector<string> Events;

//Stopping VA
long long EndVA;

//Handle of the main dialog
HWND TracerWHandle;
//Name of the target module, retrieved when start button is clicked !
string TaregtModule;
//Is main dialog shown or not ?
bool FormDisplayed = false;
//used to display messages and alerts !
const WCHAR* MessageBoxTitle = L"x64_Tracer";

//indcates if we can calc file offset in target module or not ?
bool IsFileOffsetPossible;







string Int64ToHexString ( long long Number )
{
 
	ostringstream ss;
	ss << std::hex << Number;
	return ss.str();
}

string Int64ToHexString ( void* Number )
{
	ostringstream ss;
	ss << std::hex <<  *(long long*)(Number);
	return ss.str();
}

string Int32ToHexString ( int Number )
{
	ostringstream ss;
	ss << std::hex << Number;
	return ss.str();
}

bool SaveFileDialog(char Buffer[MAX_PATH])
{
	OPENFILENAMEA sSaveFileName;

	ZeroMemory(&sSaveFileName,sizeof(sSaveFileName));
	sSaveFileName.hwndOwner = GuiGetWindowHandle();

	const char szFilterString[] = "Text files (*.txt, *.log)\0*.txt;*.log\0All Files (*.*)\0*.*\0\0";
	const char szDialogTitle[] = "Select dump file...";


	sSaveFileName.lStructSize = sizeof(sSaveFileName);
	sSaveFileName.lpstrFilter = szFilterString;


	sSaveFileName.lpstrFile = Buffer;
	sSaveFileName.nMaxFile = MAX_PATH;
	sSaveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	sSaveFileName.lpstrTitle = szDialogTitle;
	sSaveFileName.lpstrDefExt = "txt";

	return (FALSE != GetSaveFileNameA(&sSaveFileName));
}

bool CheckENDAddress(HWND hWnd)
{

	CHAR buffer[1024];

	ZeroMemory(&buffer,sizeof(buffer));

	GetWindowTextA(GetDlgItem(hWnd, IDC_EDITEndAddress), buffer, sizeof(buffer));

	EndVA = DbgValFromString(buffer);

	if(DbgIsValidExpression(buffer)  && EndVA > 0)
	{
		return true;
	}

	return false;
}

bool CheckModuleName(HWND hWnd)
{

	CHAR buffer[1024];

	ZeroMemory(&buffer,sizeof(buffer));

	GetWindowTextA(GetDlgItem(hWnd, IDC_EDITModule), buffer, sizeof(buffer));

	TaregtModule = string(buffer);

	if(TaregtModule.length() > 1)
	{
		return true;
	}

	return false;
}

//returns if check box is toggled or not ?
bool ShouldDisableGUI()
{
 
	HWND CheckBoxHandle = GetDlgItem(TracerWHandle, IDC_CHECKGUI);

	long IsChecked = (SendMessage(CheckBoxHandle, BM_GETCHECK, 0, 0));
 
	if(IsChecked == BST_CHECKED)
	{
		return true;
	}

	return false;

}












//Convert VA to RVA
//64 bit
DWORD VAtoRVA(duint VA, duint ImageBase)
{

	return (DWORD)(VA - ImageBase);

}
//32 bit 
//DWORD VAtoRVA(UINT32 VA, UINT32 ImageBase)
//{
//
//	return (DWORD)(VA - ImageBase);
//
//}



//32
ULONG RVA2FOA(DWORD ulRVA, PIMAGE_NT_HEADERS32 pNTHeader, IMAGE_SECTION_HEADER* SectionHeaders )
{

    int local = 0;
    
    for(int i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
    {
        if ( (ulRVA >= SectionHeaders[i].VirtualAddress) && (ulRVA <= SectionHeaders[i].VirtualAddress + SectionHeaders[i].SizeOfRawData) )
        {
          return SectionHeaders[i].PointerToRawData + (ulRVA - SectionHeaders[i].VirtualAddress);
        } 
    }
    return 0;
}
//64
ULONG RVA2FOA(DWORD ulRVA, PIMAGE_NT_HEADERS64 pNTHeader, IMAGE_SECTION_HEADER* SectionHeaders )
{

    int local = 0;
    
    for(int i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++)
    {
        if ( (ulRVA >= SectionHeaders[i].VirtualAddress) && (ulRVA <= SectionHeaders[i].VirtualAddress + SectionHeaders[i].SizeOfRawData) )
        {
          return SectionHeaders[i].PointerToRawData + (ulRVA - SectionHeaders[i].VirtualAddress);
        } 
    }
    return 0;
}


//32 bit
//DWORD VAtoFileOffset(UINT32 VA, UINT32 ImageBase)
//{
//
//	//first convert to RVA
//	DWORD RVA = VAtoRVA(VA, ImageBase);
// 
//	PIMAGE_NT_HEADERS32 pNTHeader = (PIMAGE_NT_HEADERS32)(NTdata);
//
//	IMAGE_SECTION_HEADER* SectionHeaders = (IMAGE_SECTION_HEADER*)(SectionHeadersData);
//
//	DWORD result = RVA2FOA(RVA, pNTHeader, SectionHeaders);
// 
//    return result;
//
//}
//64
DWORD VAtoFileOffset(duint VA, duint ImageBase)
{

	//first convert to RVA
	DWORD RVA = VAtoRVA(VA, ImageBase);

#if defined( _WIN64 )
	PIMAGE_NT_HEADERS64 pNTHeader = (PIMAGE_NT_HEADERS64)(NTdata);
#else
	PIMAGE_NT_HEADERS32 pNTHeader = (PIMAGE_NT_HEADERS32)(NTdata);
#endif

	IMAGE_SECTION_HEADER* SectionHeaders = (IMAGE_SECTION_HEADER*)(SectionHeadersData);

	DWORD result = RVA2FOA(RVA, pNTHeader, SectionHeaders);
 
    return result;

}


//Read the NT headers and the sections data after it into 2 buffers
void InitModuleNTData()
{
	
	//Get target module name
	char module[MAX_MODULE_SIZE] = "";
	ZeroMemory(&module,sizeof(module));
	GetWindowTextA(GetDlgItem(TracerWHandle, IDC_EDITModule), module, sizeof(module));

	//get target module base
	duint TargetImageBase = DbgModBaseFromName(module);
	//check if we could the target module base ?
	if(TargetImageBase > 0)
	{
		IsFileOffsetPossible = true;
	}
	else
	{
		IsFileOffsetPossible = false;
		return;
	}

	//Free memory first 
	free(SectionHeadersData);
	
	//Read 2 buffers only once when the start button is clicked !

#if defined( _WIN64 )
	
	//get DOS Headers pointer
	unsigned char DOSbuffer [0x40];
	DbgMemRead( TargetImageBase , DOSbuffer, 0x40);

	PIMAGE_DOS_HEADER DOS = (PIMAGE_DOS_HEADER)(DOSbuffer);


	//NT headers offset
	duint addr = (TargetImageBase + DOS->e_lfanew);
	DbgMemRead( addr , NTdata, NT64dataSize);
 
	PIMAGE_NT_HEADERS64 pNTHeader = (PIMAGE_NT_HEADERS64)(NTdata);
 

	// X
	int NumebrOfSections = pNTHeader->FileHeader.NumberOfSections;
	// X * 40 
	int sectionsDataSize = NumebrOfSections * NTSectionSize;

	//create dynamic buffer to hold section headers data
	SectionHeadersData = (unsigned char*)malloc(sectionsDataSize);
	DbgMemRead(addr + NT64dataSize, SectionHeadersData, sectionsDataSize);

#else

	
	//get DOS Headers pointer
	unsigned char DOSbuffer [0x40];
	DbgMemRead( TargetImageBase , DOSbuffer, 0x40);

	PIMAGE_DOS_HEADER DOS = (PIMAGE_DOS_HEADER)(DOSbuffer);

	//NT headers offset
	duint addr = (TargetImageBase + DOS->e_lfanew);
	DbgMemRead(addr, NTdata, NT32dataSize);
 
	PIMAGE_NT_HEADERS32 pNTHeader = (PIMAGE_NT_HEADERS32)(NTdata);
 

	// X
	int NumebrOfSections = pNTHeader->FileHeader.NumberOfSections;
	// X * 40 
	int sectionsDataSize = NumebrOfSections * NTSectionSize;

	//create dynamic buffer to hold section headers data
	SectionHeadersData = (unsigned char*)malloc(sectionsDataSize);
	DbgMemRead(addr + NT32dataSize, SectionHeadersData, sectionsDataSize);

#endif

}
















//Compare two char arrays ignoring case
bool iequals(const string& a, const string& b)
{
	unsigned int sz = a.size();
	if (b.size() != sz)
		return false;
	for (unsigned int i = 0; i < sz; ++i)
		if (tolower(a[i]) != tolower(b[i]))
			return false;
	return true;
}

//Show the GUI
void ShowForm(HINSTANCE hInstance, HINSTANCE hPrevInstance)
{

	_plugin_logprintf("IsFileOffsetPossible : [%d] !\n", IsFileOffsetPossible);

	//Global variable
	hInst = hInstance;

	//Prepare main dialog Icon
	main_icon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

	// Show dialog!
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOGMain), 0, &DialogProc );

}

//Set the current Target module name in the text box
void TrySetTargetModule()
{

	try
	{
		CHAR buffer[MAX_MODULE_SIZE];

		//get Current RIP
		duint entry = GetContextData(UE_CIP);

		DbgGetModuleAt(entry, buffer);

		SetWindowTextA(GetDlgItem(TracerWHandle, IDC_EDITModule), buffer );


	}
	catch (exception& e)
	{
		
		_plugin_logprintf("[%s] : TrySetTargetModule failed !\n", plugin_name);

	}

}

//Update the label which views number of events
void UpdateCountLabel()
{

	CHAR buffer[20];

	//get Current RIP
	int count = Events.size();

	itoa(count,buffer, 10);

	SetWindowTextA(GetDlgItem(TracerWHandle, IDC_STATIC_Count), buffer );

}



















VOID CALLBACK TimerProcSingleStep(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) 
{

	KillTimer(hWnd, nIDEvent);

	if(!LoggingActive)
	{
		return;
	}

	DbgCmdExec("eSingleStep");
}

VOID CALLBACK TimerProcStepOut(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) 
{

	KillTimer(hWnd, nIDEvent);

	if(!LoggingActive)
	{
		return;
	}

	DbgCmdExec("eStepOver");
}

VOID CALLBACK TimerProcRTR(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) 
{

	KillTimer(hWnd, nIDEvent);

	if(!LoggingActive)
	{
		return;
	}

	DbgCmdExec("StepOut");
}


void MyCallBack (CBTYPE cbType, void* callbackinfo)
{
 

	// If logging is not active then return !
	if( !LoggingActive )
	{
		return;
	}

	//If the form was closed then return !
	//Means plugin is not in use.
	if( !FormDisplayed )
	{
		return;
	}


	//get Current RIP
	duint entry = GetContextData(UE_CIP);

	if(entry == EndVA)
	{
		GuiUpdateEnable(true);
		LoggingActive = false;
		MessageBoxW(hwndDlg, L"END of tracing reached" , MessageBoxTitle, MB_ICONINFORMATION);
		return;

	}


	//Current Module name
	char module[MAX_MODULE_SIZE] = "";

	switch(cbType)
	{

	case CB_STEPPED:

		//Now depending on the module we are in, we should choose the right command.

		if (DbgGetModuleAt(entry,module))
		{
			if(LoggingActive == true)
			{
				//compare current module with our target module ?
				//if ( iequals(string(module), TaregtModule) )
				{
					//disassemble current line 
					BASIC_INSTRUCTION_INFO basicinfo;
					DbgDisasmFastAt(entry, &basicinfo);

					//we will ignore calls, only log jmps
					if ((basicinfo.branch))// && !(basicinfo.call)))
					{

						char disasm[GUI_MAX_DISASSEMBLY_SIZE] = "";
						GuiGetDisassembly(entry, disasm);

						bool IsTaken = DbgIsJumpGoingToExecute(entry);

						//File offset
						duint ImageBase = DbgModBaseFromName(module);
						duint FileOffset = ConvertVAtoFileOffset(ImageBase, entry, false);

						//
						//char instr[GUI_MAX_DISASSEMBLY_SIZE] = "";
						//DbgAssembleAt(entry, instr);

						//
						char Data[2048] = "";
 
						string Row;
						if(basicinfo.call)
						{
							Row =  Int32ToHexString( VAtoRVA(entry, ImageBase)) + "\t\t" + (IsFileOffsetPossible ? Int32ToHexString( VAtoFileOffset(entry, ImageBase)) : Int32ToHexString( 0)) + "\t\t\t-->";//\t\t\t\t"  + string(basicinfo.instruction);
						}
						else
						{
							Row =  Int32ToHexString( VAtoRVA(entry, ImageBase)) + "\t\t" + (IsFileOffsetPossible ? Int32ToHexString( VAtoFileOffset(entry, ImageBase)) : Int32ToHexString( 0)) + "\t\t\t" + (IsTaken ? "Yes" : "Not");// + string(basicinfo.instruction);
						}

						Events.push_back(Row);

						UpdateCountLabel();

					}
				}

				//branch destination
				duint destination = DbgGetBranchDestination(entry);

				//branch destination module
				char DestModule[MAX_MODULE_SIZE] = "";
				DbgGetModuleAt(destination,DestModule);

				//compare current module with our target module ?
				//internal jmp ?
				if ( iequals(string(module), TaregtModule) )
				{

					//we are inside our target
					if(iequals(TaregtModule , DestModule))
					{
					//Is the JMP going into out target module ?
						UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcSingleStep);
					}
					else//JMP is from target module to some external module so step over !
					{
						UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcStepOut);
					}
				}
				else//Now we are outside the target module !
				{

					//Execute till return !
					UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcRTR);

					//we are outside the target module !
					//if(iequals(TaregtModule , DestModule))
					//{
						//If the branch goes back to our target module then step into
						//UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcSingleStep);
						 
					//}
					//else
					//{	//If the jmp goes somewhere else then step over it
						//UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcStepOut);
					//}

					//After this the stepping event won't be fired
					//we will face the PasuedDebug event there

				}
			}
		}
		else
		{
			//execute till return in this unknown module !
			//UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcStepOut);
			//UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcSingleStep);
			//UINT_PTR TimerId = SetTimer(hwndDlg, 0, 10, (TIMERPROC)TimerProcStepOut);
		}

		break;

	case CB_BREAKPOINT:

		{

		}
		break;

	case CB_PAUSEDEBUG:
		{

			//disassemble current line 
			BASIC_INSTRUCTION_INFO basicinfo;
			DbgDisasmFastAt(entry, &basicinfo);

			//Current module we are in !
			char module[MAX_MODULE_SIZE] = "";
			duint entry = GetContextData(UE_CIP);

			//Are we at a "RET" instruction ?
			if( iequals( string(basicinfo.instruction) , "RET"))
			{

				if (DbgGetModuleAt(entry,module))
				{
					//If we are outside the target module then do a single step
					//if (! iequals(string(module), TaregtModule) )
					//{
						UINT_PTR TimerId = SetTimer(hwndDlg, 0, TimerStepMs, (TIMERPROC)TimerProcSingleStep);
					//}

				}

				//DO a single step !

				//UINT_PTR TimerId = SetTimer(hwndDlg, 0, 10, (TIMERPROC)TimerProcSingleStep);

			}

		}
		break;


	case CB_RESUMEDEBUG:

		break;

	}

}

// Main Dialog Procedure
INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	

	switch (uMsg)
	{


	case WM_INITDIALOG:
		{
			//Save the dialog window handle
			TracerWHandle = hWnd;

			//Set the flag
			FormDisplayed = true;

			//Set dialog icon
			SendMessage (hWnd, WM_SETICON, WPARAM (ICON_SMALL), LPARAM (main_icon));

			//Set the initial target module name if possible !
			TrySetTargetModule();

		}


		// +- BUTTON ROUTINES -+
	case WM_COMMAND:
		{

			switch (LOWORD(wParam))
			{

			case IDC_START:
				{

					if(!DbgIsDebugging())
					{
						MessageBoxW(hWnd, (L"You need to be debugging to use this !"), MessageBoxTitle , MB_OK | MB_ICONHAND);
					}
					else if(!CheckENDAddress(hWnd))
					{
						MessageBoxW(hWnd, (L"Invalid END Address !"), MessageBoxTitle , MB_OK | MB_ICONHAND);
					}
					else if(!CheckModuleName(hWnd))
					{
						MessageBoxW(hWnd, (L"Invalid target module name !"), MessageBoxTitle, MB_OK | MB_ICONHAND);
					}
					else//Can go !
					{
						//read the target module NT header info !
						InitModuleNTData();

						LoggingActive = true;

						DbgCmdExec("sst");
					}

				}
				break;

			case IDC_CHECKGUI:
				{
					//Check box was clicked so decide what to do ?
					if(ShouldDisableGUI())
					{
						GuiUpdateDisable();
					}
					else
					{
						GuiUpdateEnable(true);
					}

				}
				break;

			case IDC_FLUSH:
				{

					Events.clear();
					UpdateCountLabel();

				}
				break;

			case IDC_SAVE:
				{


					if(Events.size() < 1)
					{
						MessageBoxW(hwndDlg, L"Logging buffer is empty !", MessageBoxTitle, MB_ICONHAND);
					}
					else
					{
						//suspend the debugee
						DbgCmdExec("pause");

						char szFileName[MAX_PATH];
						ZeroMemory(szFileName, MAX_PATH);
						if(SaveFileDialog(szFileName))
						{


							//print the header
							string Row;
 
							Row = "RVA\t\t\tOffset\t\t\tStatus";

							ofstream Outfile(szFileName);

							Outfile << "-----------------------------------------------------------------------------------------" << endl;
							Outfile << Row << endl;
							Outfile << "-----------------------------------------------------------------------------------------" << endl;

							for(int t=0;t< Events.size();++t)
							{
								Outfile << Events[t] << endl;
							}

							MessageBoxW(hwndDlg, L"Log file saved !", MessageBoxTitle, MB_ICONINFORMATION);

						}

					}

				}
				break;


			}
		}
		break;


	case WM_CLOSE:
		{

			//If GUI was locked then unlock it at exit !
			if(ShouldDisableGUI)
			{
				GuiUpdateEnable(true);
			}

			//Clean and reset !
			hInst = NULL;
			main_icon = NULL;

			FormDisplayed = false;
			LoggingActive = false;
			IsFileOffsetPossible = false;

			Events.clear();
			EndVA = 0;
			TracerWHandle = 0;


			DestroyIcon(main_icon);
			DestroyWindow(hWnd);
		}
		break;

		return (DefWindowProc(hWnd, uMsg, wParam, lParam));
	}
	//prevents closing the dialog !
	return FALSE;
}























//Called when the plugin menu is clicked !
extern "C" __declspec(dllexport) void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
	switch(info->hEntry)
	{
	case MENU_Start:
		{
			if(!DbgIsDebugging())
			{
				MessageBoxW(hwndDlg, (L"You need to be debugging to use this !"), MessageBoxTitle , MB_OK | MB_ICONHAND);
				//_plugin_logputs("you need to be debugging to use this command");
				return;
			}
			//If form is still displayed then bring it to front !
			if(FormDisplayed)
			{
				BringWindowToTop(TracerWHandle);
				return;
			}

			//Show the form if debugger is on !
			ShowForm(GetModuleHandleA(plugin_DLLname),NULL);
		}
		break;

	case MENU_Stop:
		{

		}
		break;

	case MENU_Save:
		{


		}
		break;

	case MENU_Abandon:
		{

		}
		break;
	}
}

//Called when a new debugging session starts !
extern "C" __declspec(dllexport) void CBINITDEBUG(CBTYPE cbType, PLUG_CB_INITDEBUG* info)
{
	_plugin_logprintf("[TEST] debugging of file %s started!\n", (const char*)info->szFileName);
}

//Called when a debugging session ends
extern "C" __declspec(dllexport) void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
	_plugin_logputs("[TEST] debugging stopped!");
}


bool cbTestCommand(int argc, char* argv[])
{
	//Show an input box !
	char line[GUI_MAX_LINE_SIZE] = "";
	if(!GuiGetLineWindow("Enter VA", line))
	{
		_plugin_logputs("[TEST] cancel pressed!");
	}
	else
	{

		duint VA = DbgValFromString(line);
		
		char module[MAX_MODULE_SIZE] = "";
		
		DbgGetModuleAt(VA, module);
		
		duint ImageBase = DbgModBaseFromName(module);

		DWORD RVA = VAtoRVA(VA, ImageBase);


		//_plugin_logprintf("[TEST] VA : \"%s\"\n", Int64ToHexString(VA));
		//_plugin_logprintf("[TEST] module : \"%s\"\n", module);
		//_plugin_logprintf("[TEST] ImageBase : \"%s\"\n", Int64ToHexString(ImageBase));

		_plugin_logprintf("[TEST] VA to RVA : \"%d\"\n", RVA);

	}

	return true;
}

//Initialize plugin with host
void testInit(PLUG_INITSTRUCT* initStruct)
{
	//This will print the plugin handle in the log screen
	//_plugin_logprintf("[TEST] pluginHandle: %d\n", pluginHandle);


	//Register a command "ktracer" which can be written in the command text box
	//The command will be handled in the Method "cbTestCommand"
	if(!_plugin_registercommand(pluginHandle, plugin_command, cbTestCommand, false))
	{
		_plugin_logputs("[TEST] error registering the \"ktracer\" command!");
	}


}

//Called when closing the host !
void testStop()
{

	_plugin_unregistercommand(pluginHandle, plugin_command);

	_plugin_menuclear(hMenu);

	EndDialog(TracerWHandle, NULL);

}

//Called once after the "init" event
void testSetup()
{
	//Set the menu icon
	ICONDATA rocket;
	rocket.data = icon_rocket;
	rocket.size = sizeof(icon_rocket);

	_plugin_menuseticon(hMenu, &rocket);

	//Set the menus
	_plugin_menuaddentry(hMenu, MENU_Start, "&Start logging...");
	//_plugin_menuaddentry(hMenu, MENU_Stop, "&Stop loging");
	//_plugin_menuaddentry(hMenu, MENU_Save, "&Save to text file...");
	//_plugin_menuaddentry(hMenu, MENU_Abandon, "&Clear logging buffer");


	//Register debugging events callback
	_plugin_registercallback(pluginHandle, CB_STEPPED, MyCallBack);
	_plugin_registercallback(pluginHandle, CB_PAUSEDEBUG, MyCallBack);


}
