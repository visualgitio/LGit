/*
 * Unicode conversion and handling.
 */

#include <stdafx.h> 

char *LGitWideToUtf8Alloc(const wchar_t *buf)
{
	size_t required_size = LGitWideToUtf8(buf, NULL, 0);
	char *allocated = (char*)malloc(required_size);
	if (allocated == NULL) {
		return NULL;
	}
	LGitWideToUtf8(buf, allocated, required_size);
	return allocated;
}

wchar_t *LGitUtf8ToWideAlloc(const char *buf)
{
	size_t required_size = LGitUtf8ToWide(buf, NULL, 0);
	wchar_t *allocated = (wchar_t*)calloc(required_size, 2);
	if (allocated == NULL) {
		return NULL;
	}
	LGitUtf8ToWide(buf, allocated, required_size);
	return allocated;
}

char *LGitAnsiToUtf8Alloc(const char *buf)
{
	size_t required_size = MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
	wchar_t *allocated = (wchar_t*)calloc(required_size, 2);
	if (allocated == NULL) {
		return NULL;
	}
	MultiByteToWideChar(CP_ACP, 0, buf, -1, allocated, required_size);
	/* now to utf8 */
	required_size = LGitUtf8ToWide(buf, NULL, 0);
	char *allocated_utf8 = (char*)malloc(required_size);
	if (allocated_utf8 == NULL) {
		return NULL;
	}
	LGitWideToUtf8(allocated, allocated_utf8, required_size);
	free(allocated);
	return allocated_utf8;
}

int LGitAnsiToUtf8(const char *buf, char *utf8_buf, size_t utf8_bufsz)
{
	size_t required_size = MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
	wchar_t *allocated = (wchar_t*)calloc(required_size, 2);
	if (allocated == NULL) {
		return NULL;
	}
	MultiByteToWideChar(CP_ACP, 0, buf, -1, allocated, required_size);
	/* now to utf8 */
	int ret = LGitWideToUtf8(allocated, utf8_buf, utf8_bufsz);
	free(allocated);
	return ret;
}

int LGitUtf8ToAnsi(const char *buf, char *ansi_buf, size_t ansi_bufsz)
{
	size_t required_size = LGitUtf8ToWide(buf, NULL, 0);
	wchar_t *allocated = (wchar_t*)calloc(required_size, 2);
	if (allocated == NULL) {
		return NULL;
	}
	LGitUtf8ToWide(buf, allocated, required_size);
	/* now to utf8 */
	int ret = WideCharToMultiByte(CP_ACP, 0, allocated, -1, ansi_buf, ansi_bufsz, NULL, NULL);
	free(allocated);
	return ret;
}