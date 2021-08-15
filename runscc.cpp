/*
 * The SccRunScc/"Explorer" UI that does everything the IDE can't.
 */

#include "stdafx.h"

typedef struct _LGitExplorerParams {
	LGitContext *ctx;
	HMENU menu;
} LGitExplorerParams;

static void GetHeadState(HWND hwnd, LGitExplorerParams *params, char *buf, size_t bufsz)
{
	git_reference *head = NULL;
	const char *ref = NULL;
	switch (git_repository_head(&head, params->ctx->repo)) {
	case GIT_EUNBORNBRANCH:
		strlcpy(buf, "Unborn HEAD", bufsz);
		return;
	case GIT_ENOTFOUND:
		strlcpy(buf, "No HEAD", bufsz);
		return;
	default:
		LGitLibraryError(hwnd, "git_repository_head");
		strlcpy(buf, "Error", bufsz);
		return;
	case 0:
		ref = git_reference_shorthand(head);
		break;
	}
	/* XXX: Is some fallback possible? (i.e. OID) */
	strlcpy(buf, ref == NULL ? "HEAD without branch" : ref, bufsz);
	if (head != NULL) {
		git_reference_free(head);
	}
}

static void RefreshExplorer(HWND hwnd, LGitExplorerParams *params)
{
	char newTitle[512];
	if (params->ctx->active && params->ctx->repo != NULL) {
		char head[64];
		GetHeadState(hwnd, params, head, 64);
		_snprintf(newTitle, 512,
			"Repository Explorer - (%s) %s",
			head,
			params->ctx->workdir_path);
	} else {
		strlcpy(newTitle, "Repository Explorer - (no repo)", 512);
	}
	SetWindowText(hwnd, newTitle);
}

static BOOL HandleExplorerCommand(HWND hwnd, UINT cmd, LGitExplorerParams *params)
{
	switch (cmd) {
	case ID_EXPLORER_REPOSITORY_REFRESH:
		RefreshExplorer(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_HISTORY:
		SccHistory(params->ctx, hwnd, 0, NULL, 0, NULL);
		return TRUE;
	case ID_EXPLORER_REMOTE_MANAGEREMOTES:
		LGitShowRemoteManager(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_REMOTE_PUSH:
		LGitPushDialog(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_REMOTE_PULL:
		LGitPullDialog(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_CONFIG_REPOSITORY:
		{
			git_config *config = NULL;
			if (git_repository_config(&config, params->ctx->repo) != 0) {
				LGitLibraryError(hwnd, "git_repository_config");
				return true;
			}
			/* XXX: Do we force a scope with git_config_open_level? */
			LGitManageConfig(params->ctx, hwnd, config, "Repository");
			git_config_free(config);
		}
		return TRUE;
	case ID_EXPLORER_CONFIG_GLOBAL:
		{
			git_config *config = NULL;
			/* Covers the sys-wide to read from, even if we only edit user */
			if (git_config_open_default(&config) != 0) {
				LGitLibraryError(hwnd, "git_config_open_default");
				return true;
			}
			LGitManageConfig(params->ctx, hwnd, config, "Global");
			git_config_free(config);
		}
		return TRUE;
	case ID_EXPLORER_HELP_ABOUT:
		LGitAbout(hwnd, params->ctx);
		return TRUE;
	default:
		return FALSE;
	}
}

static void InitExplorerView(HWND hwnd, LGitExplorerParams *params)
{
	LGitSetWindowIcon(hwnd, params->ctx->dllInst, MAKEINTRESOURCE(IDI_LGIT));
	SetMenu(hwnd, params->menu);
	RefreshExplorer(hwnd, params);
	/* Because we can be invoked without a valid repo... */
	UINT newState = MF_BYCOMMAND
		| (params->ctx->active ? MF_ENABLED : MF_GRAYED);
#define EnableMenuItemIfInRepo(id) EnableMenuItem(params->menu,id,newState)
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_REFRESH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_HISTORY);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_MANAGEREMOTES);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PUSH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PULL);
	EnableMenuItemIfInRepo(ID_EXPLORER_CONFIG_REPOSITORY);
}

static BOOL CALLBACK ExplorerDialogProc(HWND hwnd,
										unsigned int iMsg,
										WPARAM wParam,
										LPARAM lParam)
{
	LGitExplorerParams *param;
	param = (LGitExplorerParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitExplorerParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitExplorerView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_EXPLORER_REPOSITORY_CLOSE:
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		default:
			/* we might have a LOT of commands here */
			return HandleExplorerCommand(hwnd, LOWORD(wParam), param);
		}
	default:
		return FALSE;
	}
}

SCCRTN SccRunScc(LPVOID context, 
				 HWND hWnd, 
				 LONG nFiles, 
				 LPCSTR* lpFileNames)
{
	int i, path_count = 0;
	LGitContext *ctx = (LGitContext*)context;
	LGitLog("**SccRunScc** Context=%p\n", context);
	LGitLog("  files %d\n", nFiles);
	char **paths = (char**)calloc(sizeof(char*), nFiles);
	if (paths == NULL) {
		return SCC_E_NONSPECIFICERROR;
	}
	for (i = 0; i < nFiles; i++) {
		char *path;
		const char *raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		path = strdup(raw_path);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    %s\n", path);
		paths[path_count++] = path;
	}
	LGitExplorerParams params;
	params.ctx = ctx;
	params.menu = LoadMenu(ctx->dllInst, MAKEINTRESOURCE(IDR_EXPLORER_MENU));
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_EXPLORER),
		hWnd,
		ExplorerDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	default:
		break;
	}
	DestroyMenu(params.menu);
	if (paths != NULL) {
		LGitFreePathList(paths, path_count);
	}
	return SCC_OK;
}
