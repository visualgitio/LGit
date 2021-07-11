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