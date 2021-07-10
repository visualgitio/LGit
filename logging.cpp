#include "stdafx.h"
#include "LGit.h"

void LGitLog(const char *format_str, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, format_str);
	_vsnprintf(msg, 1024, format_str, va);
	va_end(va);

	OutputDebugString(msg);
}

void LGitLibraryError(HWND hWnd, LPCSTR title)
{
	const git_error *gerror;
	char *msg;

	gerror = git_error_last();
	msg = gerror != NULL
		? gerror->message
		: "libgit2 returned a null error.";
	LGitLog("!!LGitLibraryError!! %s (GLE %x) %s\n",
		title,
		GetLastError(),
		msg);
	MessageBoxA(hWnd,
		msg,
		title,
		MB_ICONERROR | MB_OK);
}