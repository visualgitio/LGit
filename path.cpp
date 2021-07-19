#include "stdafx.h"
#include "LGit.h"

#pragma comment(lib, "shlwapi")

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
void LGitTranslateStringChars(char *buf, int char1, int char2)
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
	int common;
	char common_path[MAX_PATH];
	if (ctx == NULL) {
		return NULL;
	}
	// Maybe PathRelativePathToA instead?
	common = PathCommonPrefixA(ctx->workdir_path, abs, common_path);
	abs += common;
	if (abs[0]) {
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
BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz)
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
	strncpy(project, temp_path + begin,
		__min(strlen(temp_path + begin), bufsz));
fin:
	free(temp_path);
	return begin != -1;
}