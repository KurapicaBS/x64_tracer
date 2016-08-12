#include "pluginmain.h"
#include "Tracing.h"


using namespace std;




//Plugin handle
int pluginHandle;
//GUI window handle
HWND hwndDlg;
//plugin menu handle
int hMenu;
 


//Called once on plugin loading event
DLL_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{

	_plugin_logprintf("[Ktracer] pluginHandle: %d : event %s \n", pluginHandle, "pluginit");

    initStruct->pluginVersion = plugin_version;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strcpy(initStruct->pluginName, plugin_name);
    pluginHandle = initStruct->pluginHandle;
    testInit(initStruct);
    return true;
}

//Called when plugin is stopped ?
DLL_EXPORT bool plugstop()
{
	_plugin_logprintf("[Ktracer] pluginHandle: %d : event %s \n", pluginHandle, "plugstop");

    testStop();
    return true;
}

//Called after the init event
DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{

	_plugin_logprintf("[Ktracer] pluginHandle: %d : event %s \n", pluginHandle, "plugsetup");

    hwndDlg = setupStruct->hwndDlg;
    hMenu = setupStruct->hMenu;
 
    testSetup();
}

extern "C" DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}



