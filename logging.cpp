#include "stdafx.h"

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

const char* LGitCommandName(enum SCCCOMMAND command)
{
	switch (command) {
	case -1:
		return "(No command)";
	case SCC_COMMAND_CHECKOUT:
		return "Checkout";
	case SCC_COMMAND_GET:
		return "Get";
	case SCC_COMMAND_CHECKIN:
		return "Checkin";
	case SCC_COMMAND_UNCHECKOUT:
		return "Uncheckout";
	case SCC_COMMAND_ADD:
		return "Add";
	case SCC_COMMAND_REMOVE:
		return "Remove";
	case SCC_COMMAND_DIFF:
		return "Diff";
	case SCC_COMMAND_HISTORY:
		return "History";
	case SCC_COMMAND_RENAME:
		return "Rename";
	case SCC_COMMAND_PROPERTIES:
		return "Properties";
	case SCC_COMMAND_OPTIONS:
		return "Options";
	default:
		/* should be able to return printed string, but I digress */
		return "Unknown";
	}
}