/*
 * Formatting of libgit2 things into human-readable things.
 */

#include "stdafx.h"

static void TimetToFileTime(_int64 t, LPFILETIME pft)
{
    LONGLONG ll = (t * 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD) ll;
    pft->dwHighDateTime = (DWORD)(ll >>32);
}

BOOL LGitTimeToString(const git_time *time, char *buf, size_t bufsz)
{
	FILETIME ft;
	SYSTEMTIME st;
	int written;
	char strbuf[128];
	TimetToFileTime(time->time, &ft);
	FileTimeToSystemTime(&ft, &st);
	written = GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, strbuf, 128);
	if (written == -1) {
		return FALSE;
	}
	strlcpy(buf, strbuf, bufsz);
	strlcat(buf, " ", bufsz);
	written = GetTimeFormatA(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, strbuf, 128);
	if (written == -1) {
		return FALSE;
	}
	strlcat(buf, strbuf, bufsz);
	return TRUE;
}

BOOL LGitTimeToStringW(const git_time *time, wchar_t *buf, size_t bufsz)
{
	FILETIME ft;
	SYSTEMTIME st;
	int written;
	wchar_t strbuf[128];
	TimetToFileTime(time->time, &ft);
	FileTimeToSystemTime(&ft, &st);
	written = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, strbuf, 128);
	if (written == -1) {
		return FALSE;
	}
	wcslcpy(buf, strbuf, bufsz);
	wcslcat(buf, L" ", bufsz);
	written = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, strbuf, 128);
	if (written == -1) {
		return FALSE;
	}
	wcslcat(buf, strbuf, bufsz);
	return TRUE;
}

int LGitFormatSignature(const git_signature *sig, char *buf, size_t bufsz)
{
	return _snprintf(buf, bufsz, "%s <%s>", sig->name, sig->email);
}

int LGitFormatSignatureW(const git_signature *sig, wchar_t *buf, size_t bufsz)
{
	/* I assume these are UTF-8; it's never clear */
	wchar_t name[128], email[128];
	MultiByteToWideChar(CP_UTF8, 0, sig->name, -1, name, 128);
	MultiByteToWideChar(CP_UTF8, 0, sig->email, -1, email, 128);
	return _snwprintf(buf, bufsz, L"%s <%s>", name, email);
}

UINT LGitGitToWindowsCodepage(const char *encoding)
{
	/* Likely */
	if (encoding == NULL || strcmp(encoding, "UTF-8") == 0) {
		goto utf8;
	}
	LGitLog(" ! Unknown encoding '%s'\n",  encoding);
utf8:
	return CP_UTF8;
}