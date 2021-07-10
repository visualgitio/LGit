#include "stdafx.h"
#include "LGit.h"

const char *LGitStripBasePath(LGitContext *ctx, const char *abs)
{
	int common;
	char common_path[MAX_PATH];
	if (ctx == NULL) {
		return NULL;
	}
#pragma comment(lib, "shlwapi")
	// Maybe PathRelativePathToA instead?
	common = PathCommonPrefixA(ctx->path, abs, common_path);
	abs += common;
	if (abs[0]) {
		abs++;
	}
	return abs;
}