/*
 * Formatting of libgit2 things into human-readable things.
 */

#include "stdafx.h"
#include "LGit.h"

static void TimetToFileTime(_int64 t, LPFILETIME pft)
{
    LONGLONG ll = (t * 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD) ll;
    pft->dwHighDateTime = (DWORD)(ll >>32);
}

BOOL LGitTimeToString(const git_time *time, char *buf, int bufsz)
{
	FILETIME ft;
	SYSTEMTIME st;
	int written;
	char strdate[128], strtime[128];
	TimetToFileTime(time->time, &ft);
	FileTimeToSystemTime(&ft, &st);
	written = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, strdate, 128);
	if (written == -1) {
		return FALSE;
	}
	strlcpy(buf, strdate, bufsz);
	strlcat(buf, " ", bufsz);
	written = GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buf + strlen(buf), 128);
	if (written == -1) {
		return FALSE;
	}
	strlcat(buf, strtime, bufsz);
	return TRUE;
}

int LGitFormatSignature(const git_signature *sig, char *buf, int bufsz)
{
	return _snprintf(buf, bufsz, "%s <%s>", sig->name, sig->email);
}