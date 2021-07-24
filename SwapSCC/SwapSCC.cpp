// SwapSCC.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#define SWAP_KEY_INSTALLED "SOFTWARE\\SourceCodeControlProvider\\InstalledSCCProviders"
#define SWAP_KEY_SCC "SOFTWARE\\SourceCodeControlProvider"
#define SWAP_KEY_PROVIDER "ProviderRegKey"

typedef std::map<std::string, std::string> ProviderMap;
ProviderMap providerMap;

static void GetProviders(HWND hdlg)
{
	LONG ret;
	HKEY providersKey;

	DWORD i;
	/* so much sludge */
    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
    TCHAR  achValue[255]; 
    DWORD cchValue = 255;
	/* value */
	BYTE skValue[255], selectedValue[255];
	DWORD skValueLen, type;
	/* window */
	HWND combo;
	combo = GetDlgItem(hdlg, IDC_PROVIDERS);
	DWORD toUse;

	// get the current provider
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		SWAP_KEY_SCC,
		0,
		KEY_READ,
		&providersKey);
	skValueLen = 255;
	ret = RegQueryValueEx(providersKey, SWAP_KEY_PROVIDER, NULL, &type, selectedValue, &skValueLen);
	if (REG_SZ != type) {
		return;
	}
	std::string selectedValueStr((const char*)selectedValue);
	RegCloseKey(providersKey);
	// now provider list
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		SWAP_KEY_INSTALLED,
		0,
		KEY_READ,
		&providersKey);
	ret = RegQueryInfoKey(
        providersKey,            // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 
	for (i = 0; i < cValues; i++) {
		cchValue = 255; 
        achValue[0] = '\0'; 
        ret = RegEnumValue(providersKey, i, 
                achValue, 
                &cchValue, 
                NULL, 
                NULL,
                NULL,
                NULL);
		skValueLen = 255;
		ret = RegQueryValueEx(providersKey, achValue, NULL, &type, skValue, &skValueLen);
		if (REG_SZ != type) {
			continue;
		}
		std::string k(achValue);
		std::string v((const char*)skValue);
		ProviderMap::value_type vt(k, v);
		providerMap.insert(vt);
		DWORD index = SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)achValue);
		if (selectedValueStr == v) {
			toUse = index;
		}
	}
	RegCloseKey(providersKey);
	SendMessage(combo, CB_SETCURSEL, toUse, 0);
}

static void SetProvider(HWND hdlg)
{
	HKEY key;
	long ret;
	char text[256];
	GetDlgItemText(hdlg, IDC_PROVIDERS, text, 256);
	std::string selected = providerMap[text];
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		SWAP_KEY_SCC,
		0,
		KEY_WRITE,
		&key);
	ret = RegSetValueEx(key, SWAP_KEY_PROVIDER, 0, REG_SZ,
		(BYTE*)selected.c_str(), selected.length());
	RegCloseKey(key);
}

static BOOL CALLBACK SwapProc(HWND hdlg,
							  unsigned int iMsg,
							  WPARAM wParam,
							  LPARAM lParam)
{
	switch (iMsg) {
	case WM_INITDIALOG:
		GetProviders(hdlg);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			SetProvider(hdlg);
			EndDialog(hdlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hdlg, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	// Required for themes
	InitCommonControls();
	int ret = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_SWAP), NULL, SwapProc, 0);
	if (ret == -1) {
		char msg[256];
		_snprintf(msg, 256, "Couldn't make dialog window (%x)\n", GetLastError());
		OutputDebugString(msg);
		return 2;
	}
	return 0;
}


