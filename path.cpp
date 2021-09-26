#include "stdafx.h"

void LGitFreePathList(char **paths, int path_count)
{
	int i;
	for (i = 0; i < path_count; i++) {
		free(paths[i]);
	}
	free(paths);
}

/*
 * We want to transform the input path to have backslashes, since the libgit2
 * function returns a workdir path with forward slashes.
 */
LGIT_API void LGitTranslateStringChars(char *buf, int char1, int char2)
{
	size_t len = strlen(buf), i;
	for (i = 0; i < len; i++) {
		if (buf[i] == char1) {
			buf[i] = char2;
		}
	}
}

const char *LGitStripBasePath(LGitContext *ctx, const char *abs)
{
	if (ctx == NULL || strlen(ctx->workdir_path) < 1) {
		return NULL;
	}
	char path[_MAX_PATH];
	/* strip trailing backslash, then accomodate if we need to bump later */
	strlcpy(path, ctx->workdir_path, _MAX_PATH);
	char *last_backslash = strrchr(path, '\\');
	if (last_backslash != NULL && last_backslash[1] == '\0') {
		last_backslash[0] = '\0';
	}
	const char *begin = strcasestr(abs, path);
	if (begin != abs) {
		return NULL;
	}
	abs += strlen(path);
	if (abs[0] == '\\') {
		abs++;
	}
	return abs;
}

/**
 * Try to get the last directory component as a quick and dirty way to get the
 * project name. Expects Win32-style path with backslashes for now.
 *
 * XXX: Maybe try the URL if we fail with the path?
 */
LGIT_API BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz)
{
	char *temp_path = strdup(path);
	if (temp_path == NULL) {
		return FALSE;
	}
	int i, begin = -1;
	for (i = strlen(temp_path); i >= 0; i--) {
		if (temp_path[i] == '\\' && temp_path[i + 1] == '\0') {
			temp_path[i] = '\0';
		} else if (temp_path[i] == '\\') {
			begin = i + 1;
			break;
		}
	}
	if (begin == -1) {
		goto fin;
	}
	ZeroMemory(project, bufsz);
	strlcpy(project, temp_path + begin, bufsz);
fin:
	free(temp_path);
	return begin != -1;
}

void LGitOpenFiles(LGitContext *ctx, git_strarray *paths)
{
	for (int i = 0; i < paths->count; i++) {
		char full_path[2048];
		strlcpy(full_path, ctx->workdir_path, 2048);
		strlcat(full_path, paths->strings[i], 2048);
		LGitTranslateStringChars(full_path, '/', '\\');

		SHELLEXECUTEINFO info;
		ZeroMemory(&info, sizeof(SHELLEXECUTEINFO));

		info.cbSize = sizeof info;
		info.lpFile = full_path;
		info.nShow = SW_SHOW;
		info.fMask = SEE_MASK_INVOKEIDLIST;
		info.lpVerb = "open";

		ShellExecuteEx(&info);
	}
}