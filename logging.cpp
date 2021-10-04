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

const char *LGitRepoStateString(int state)
{
	switch (state) {
	/* much of this is irrelevant for us */
	case GIT_REPOSITORY_STATE_NONE:
		return "Normal";
	case GIT_REPOSITORY_STATE_MERGE:
		return "Merging";
	case GIT_REPOSITORY_STATE_REVERT:
		return "Reverting";
	case GIT_REPOSITORY_STATE_REVERT_SEQUENCE:
		return "Reverting Sequence";
	case GIT_REPOSITORY_STATE_CHERRYPICK:
		return "Cherry-Picking";
	case GIT_REPOSITORY_STATE_CHERRYPICK_SEQUENCE:
		return "Cherry-Picking Sequence";
	case GIT_REPOSITORY_STATE_BISECT:
		return "Bisecting";
	case GIT_REPOSITORY_STATE_REBASE:
		return "Rebasing";
	case GIT_REPOSITORY_STATE_REBASE_INTERACTIVE:
		return "Interactively Rebasing";
	case GIT_REPOSITORY_STATE_REBASE_MERGE:
		return "Rebase Merging";
	case GIT_REPOSITORY_STATE_APPLY_MAILBOX:
		return "Applying Mailbox";
	case GIT_REPOSITORY_STATE_APPLY_MAILBOX_OR_REBASE:
		return "Applying Mailbox/Rebasing";
	default:
		return "Unknown";
	}
}

const char *LGitBranchType(git_branch_t type)
{
	switch (type) {
	case GIT_BRANCH_LOCAL: return "Local";
	case GIT_BRANCH_REMOTE: return "Remote";
	case GIT_BRANCH_ALL: return "All";
	default: return "Unknown";
	}
}