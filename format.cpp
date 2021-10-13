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
	LGitUtf8ToWide(sig->name, name, 128);
	LGitUtf8ToWide(sig->email, email, 128);
	return _snwprintf(buf, bufsz, L"%s <%s>", name, email);
}

int LGitFormatSignatureWithTimeW(const git_signature *sig, wchar_t *buf, size_t bufsz)
{
	wchar_t name[128], email[128], time[128];
	LGitUtf8ToWide(sig->name, name, 128);
	LGitUtf8ToWide(sig->email, email, 128);
	LGitTimeToStringW(&sig->when, time, 128);
	return _snwprintf(buf, bufsz, L"%s %s <%s>", time, name, email);
}

UINT LGitGitToWindowsCodepage(const char *encoding)
{
	/* Likely */
	if (encoding == NULL || strcmp(encoding, "UTF-8") == 0) {
		goto utf8;
	} else if (strcmp(encoding, "iso-8859-4") == 0) {
		/* found in libssh2 */
		return 28594;
	}
	LGitLog(" ! Unknown encoding '%s'\n",  encoding);
utf8:
	return CP_UTF8;
}

/**
 * Converts Unix to Windows newlines. crlf_len should be strlen(lf) * 2.
 */
void LGitLfToCrLf(char *crlf, const char *lf, size_t crlf_len)
{
	if (crlf_len < 1) {
		return;
	}
	crlf[0] = '\0';
	size_t len = strlen(lf);
	for (size_t i = 0, j = 0; i < len; i++) {
		/* XXX: could be faster? */
		if (j >= crlf_len) {
			break;
		}
		if (lf[i] == '\n' && i > 0 && lf[i - 1] != '\r') {
			/*
			crlf[j] = '\r';
			crlf[j + 1] = '\n';
			*/
			j += 2;
			/* check overflow and if last char is a CR, trunc */
			if (strlcat(crlf, "\r\n", crlf_len) >= crlf_len
				&& crlf[crlf_len - 1] == '\r') {
				crlf[crlf_len - 1] = '\0';
				break;
			}
		} else {
			crlf[j++] = lf[i];
		}
	}
}

/**
 * Primarily used for commit messages, which are usually stored with a charset
 * and possibly Unix-style newlines we don't want in an edit control.
 */
void LGitSetWindowTextFromCommitMessage(HWND ctrl, UINT codepage, const char *message)
{
	wchar_t *new_msg_conv = NULL;
	/* we need to convert newlines */
	size_t len = strlen(message);
	char *message_converted = (char*)calloc(len, 2);
	if (message_converted == NULL) {
		/* mojibake AND bad newlines! */
		SetWindowTextA(ctrl, message);
		return;
	}
	/* i think len * 2 is safe is calloc succeeded (no wrap?) */
	LGitLfToCrLf(message_converted, message, len * 2);
	/* then convert to UCS-2 */
	int new_len = MultiByteToWideChar(codepage, 0, message_converted, -1, NULL, 0);
	if (new_len < 0) {
		/* enjoy your mojibake */
		SetWindowTextA(ctrl, message_converted);
		goto not_unicode;
	}
	new_msg_conv = (wchar_t*)calloc(new_len + 1, sizeof(wchar_t));
	if (new_msg_conv == NULL) {
		SetWindowTextA(ctrl, message_converted);
		goto not_unicode;
	}
	MultiByteToWideChar(codepage, 0, message_converted, -1, new_msg_conv, new_len + 1);
	SetWindowTextW(ctrl, new_msg_conv);
	free(new_msg_conv);
not_unicode:
	free(message_converted);
}