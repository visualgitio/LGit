/*
 * Launches the Visual Git explorer UI as required. This allows standalone
 * usage without being hosted as an SCC plugin.
 *
 * This might be useful as a generic SCC wrapper...
 */

#include "stdafx.h"

#define HandleSccError(buf, bufsz, ret, msg) if (ret == SCC_I_OPERATIONCANCELED || ret == SCC_E_COULDNOTCREATEPROJECT) { \
	return 2; \
	} else if (ret != SCC_OK) { \
	_snprintf(buf, bufsz, "%s (ret %x)", msg, ret); \
	OutputDebugString(buf); \
	MessageBoxA(NULL, buf, "Visual Git Standalone Error", MB_ICONERROR); \
	return 1; \
	}

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR     lpCmdLine,
                      int       nCmdShow)
{
	SCCRTN ret;
	LPVOID ctx;
	LONG coLen, commentLen, caps; // ignored
	char provName[SCC_NAME_LEN], auxPath[SCC_AUXLABEL_LEN]; // ignored
	char user[SCC_USER_LEN]; // ignored
	char buf[2048];
	wchar_t path[2048];
	char path_utf8[2048];
	char shortName[SCC_PRJPATH_LEN]; // XXX: Make the project name used
	ZeroMemory(path, 2048 * 2);
	ZeroMemory(shortName, SCC_PRJPATH_LEN);
	ZeroMemory(user, SCC_USER_LEN);
	ZeroMemory(provName, SCC_NAME_LEN);
	ZeroMemory(auxPath, SCC_AUXLABEL_LEN);

	CoInitialize(NULL);
	InitCommonControls();

	ret = SccInitialize(&ctx, NULL, "Visual Git Standalone", provName, &caps, auxPath, &coLen, &commentLen);
	HandleSccError(buf, 2048, ret, "SccInitialize");

	// decide to clone or open a project (clone dialog offers to open existing)
	// if no path provided, then go for it
	wcscpy(path, lpCmdLine);
	LGitWideToUtf8(path, path_utf8, 2048);
	if (wcslen(lpCmdLine) > 0) {
		if (GetFileAttributesW(lpCmdLine) & FILE_ATTRIBUTE_DIRECTORY) {
			LGitTranslateStringChars(path_utf8, '/', '\\');
			LGitGetProjectNameFromPath(shortName, path_utf8, SCC_PRJPATH_LEN);
		} else {
			MessageBoxW(NULL,
				L"The specified path doesn't exist.",
				lpCmdLine,
				MB_ICONERROR);
			return 1;
		}
		ret = LGitOpenProject(ctx, NULL, "", shortName, path_utf8, "", "", NULL, SCC_OP_CREATEIFNEW);
		HandleSccError(buf, 2048, ret, "SccOpenProject");
	}/* else {
		BOOL isNew = TRUE;
		// GetProjPath returns an error on cancel
		ret = SccGetProjPath(ctx, NULL, "", shortName, path, auxPath, TRUE, &isNew);
		HandleSccError(buf, 2048, ret, "SccGetProjPath");
		//ret = LGitClone(ctx, NULL, shortName, path, &isNew);
		//HandleSccError(buf, 2048, ret, "LGitClone");
	}*/

	// let's go
	ret = LGitStandaloneExplorer(ctx, 0, NULL);
	HandleSccError(buf, 2048, ret, "LGitStandaloneExplorer");

	// tear down
	ret = SccCloseProject(ctx);
	HandleSccError(buf, 2048, ret, "SccCloseProject");
	ret = SccUninitialize(ctx);
	HandleSccError(buf, 2048, ret, "SccUninitialize");

	return 0;
}



