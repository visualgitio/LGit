/*
 * Diffs between files and directories.
 */

#include "stdafx.h"
#include "LGit.h"

typedef struct _LGitDiffDialogParams {
	LGitContext *ctx;
	git_diff *diff;

	/* Only likely relevant for single-file SccDiff */
	const char *path;
} LGitDiffDialogParams;

static void SetDiffTitleBar(HWND hwnd, LGitDiffDialogParams* params)
{
	if (params->path) {
		char title[256];
		_snprintf(title, 256, "Diff for %s", params->path);
		SetWindowText(hwnd, title);
	}
	/* defualt otherwise */
}

static void InitDiffView(HWND hwnd, LGitDiffDialogParams* params)
{
}

static int diff_output(const git_diff_delta *d,
					   const git_diff_hunk *h,
					   const git_diff_line *l,
					   void *p)
{
	HWND diffview = *(HWND*)p;
	char buf[256];
	int endidx;
	if (l->origin == GIT_DIFF_LINE_CONTEXT ||
		l->origin == GIT_DIFF_LINE_ADDITION ||
		l->origin == GIT_DIFF_LINE_DELETION) {
		buf[0] = l->origin;
		endidx = __min(255, l->content_len);
		strncpy(buf + 1, l->content, endidx);
	} else {
		endidx = __min(256, l->content_len);
		strncpy(buf, l->content, endidx);
	}
	buf[endidx] = '\0';
	/* Git clumps multiple lines together... */
	OutputDebugString(buf);
	SendMessage(diffview, LB_ADDSTRING, 0, (LPARAM)buf);
	return 0;
}

static BOOL FillDiffView(HWND hwnd, LGitDiffDialogParams* params)
{
	HWND diffview;
	diffview = GetDlgItem(hwnd, IDC_DIFFTEXT);
	if (diffview == NULL) {
		LGitLog(" ! Couldn't get diff control\n");
		return FALSE;
	}
	git_diff_print(params->diff, GIT_DIFF_FORMAT_PATCH, diff_output, &diffview);
	return TRUE;
}

static BOOL CALLBACK DiffDialogProc(HWND hwnd,
									unsigned int iMsg,
									WPARAM wParam,
									LPARAM lParam)
{
	LGitDiffDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitDiffDialogParams*)lParam;
		SetDiffTitleBar(hwnd, param);
		if (!FillDiffView(hwnd, param)) {
			EndDialog(hwnd, 0);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN LGitDiffInternal (LPVOID context, 
						 HWND hWnd, 
						 LPCSTR lpFileName, 
						 LONG dwFlags,
						 LPCMDOPTS pvOptions)
{
	git_diff_options diffopts;
	git_diff *diff;
	
	LGitDiffDialogParams params;

	const char *raw_path;
	char path[1024], *path_ptr;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccDiff** Flags %x, %s\n", dwFlags, lpFileName);

	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);

	raw_path = LGitStripBasePath(ctx, lpFileName);
	if (raw_path == NULL) {
		LGitLog("     Couldn't get base path for %s\n", lpFileName);
		return SCC_E_NONSPECIFICERROR;
	}
	/* Translate because libgit2 operates with forward slashes */
	strncpy(path, raw_path, 1024);
	LGitTranslateStringChars(path, '\\', '/');

	/* Work around a very weird C quirk where &array == array are the same */
	path_ptr = path;
	diffopts.pathspec.strings = &path_ptr;
	diffopts.pathspec.count = 1;

	if (dwFlags & SCC_DIFF_IGNORESPACE) {
		diffopts.flags |= GIT_DIFF_IGNORE_WHITESPACE;
	}
	/* XXX: Not sure if semantics are a precise match */
	if (dwFlags & SCC_DIFF_QD_CHECKSUM) {
		diffopts.flags |= GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	}

	/*
	 * Conceptually, SccDiff is "what's different from the commited copy".
	 * This is similar to a straight "git diff" with a specific file, that is,
	 * we're comparing the working tree to HEAD.
	 */
	if (git_diff_index_to_workdir(&diff, ctx->repo, NULL, &diffopts) != 0) {
		LGitLibraryError(hWnd, "SccDiff git_diff_index_to_workdir");
		return SCC_E_NONSPECIFICERROR;
	}
	/* XXX: Rename detection with git_diff_find_similar? */

	/* If it's a quick diff, don't pop up a UI */
	if (dwFlags & SCC_DIFF_QUICK_DIFF)
	{
		/* Contents only. We don't (yet?) support timestamp diff */
		size_t deltas;
		deltas = git_diff_num_deltas(diff);
		git_diff_free(diff);
		return deltas > 0 ? SCC_I_FILEDIFFERS : SCC_OK;
	}

	params.ctx = ctx;
	params.diff = diff;
	params.path = path;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_DIFF),
		hWnd,
		DiffDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		git_diff_free(diff);
		return SCC_E_NONSPECIFICERROR;
	default:
		break;
	}

	git_diff_free(diff);
	return SCC_E_OPNOTSUPPORTED;
}

/*
 * The LGitDiffInternal has the same semantics regardless if it's a file or a
 * directory being compared. Traditional SCC plugins would probably do a side
 * by side comparison of the file or directory, but Git doesn't really work
 * like that. Instead, we'll just shove it all into the same pathspec.
 */
SCCRTN SccDiff (LPVOID context, 
				HWND hWnd, 
				LPCSTR lpFileName, 
				LONG dwFlags,
				LPCMDOPTS pvOptions)
{
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}

SCCRTN SccDirDiff (LPVOID context, 
				   HWND hWnd, 
				   LPCSTR lpFileName, 
				   LONG dwFlags,
				   LPCMDOPTS pvOptions)
{
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}