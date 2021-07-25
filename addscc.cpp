/*
 * Adds files to the project from Git. Luckily, the the plugin handles adding
 * them to disks, which is already done, we just need to provide an interface
 * for the IDE to select files and bring them on board.
 *
 * For some reason, Visual C++ 6 calls this "share" with a link icon. Weird.
 */

#include "stdafx.h"
#include "LGit.h"

static LVCOLUMN path_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Path"
};

typedef struct _LGitAddFromDialogParams {
	LGitContext *ctx;
	git_index *index;
	char *restraint_path;
	const char ***output;
	long *output_size;
} LGitAddFromDialogParams;

static void InitAddFromView(HWND hwnd, LGitAddFromDialogParams* params)
{
	HWND lv;
	char title[1024];

	if (params->restraint_path != NULL && strlen(params->restraint_path) > 0) {
		_snprintf(title, 1024, "Add files from Git (%s)", params->restraint_path);
		SetWindowText(hwnd, title);
	}

	lv = GetDlgItem(hwnd, IDC_ADDSCC_LIST);

	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_CHECKBOXES
		| LVS_EX_LABELTIP);

	/* XXX: We could add more columns for other fields in git_index_entry */
	ListView_InsertColumn(lv, 0, &path_column);
}

static BOOL FillAddFromView(HWND hwnd, LGitAddFromDialogParams* params)
{
	const git_index_entry *entry;
	size_t entry_count, i, insert_index;
	HWND lv;

	lv = GetDlgItem(hwnd, IDC_ADDSCC_LIST);

	entry_count = git_index_entrycount(params->index);
	insert_index = 0;
	/* XXX: How do we skip files the IDE already knows about? */
	for (i = 0; i < entry_count; i++) {
		LVITEM lvi;
		entry = git_index_get_byindex(params->index, i);
		/* XXX */
		if (params->restraint_path != NULL &&
			strstr(entry->path, params->restraint_path) != entry->path) {
			LGitLog(" ! Prefix not shared ('%s' in '%s')\n", params->restraint_path, entry->path);
			continue;
		}
		LGitLog(" ! Adding %s to list\n", entry->path);

		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (char*)entry->path;
		lvi.iSubItem = 0;
		lvi.iItem = insert_index++;
		/* XXX: Icons? or does checkboxes disable that? */

		lvi.iItem = ListView_InsertItem(lv, &lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed for %s\n", entry->path);
			continue;
		}
	}
	/* Recalculate after adding because of scroll bars */
	ListView_SetColumnWidth(lv, 0, LVSCW_AUTOSIZE_USEHEADER);
	return TRUE;
}

static void BuildAddList(HWND hwnd, LGitAddFromDialogParams* params)
{
	size_t entry_count, i;
	long to_set;
	const char **output;
	HWND lv;

	lv = GetDlgItem(hwnd, IDC_ADDSCC_LIST);

	/* There may not be as many LV entries as there are index entries */
	entry_count = ListView_GetItemCount(lv);
	/* We won't use all of it; set a NULL as the last for the sake of free */
	output = (const char**)calloc(entry_count + 1, sizeof(char*));

	to_set = 0;
	for (i = 0; i < entry_count; i++) {
		LVITEM lvi;
		char *path, relative_path[1024];
		/* Maybe change criteria; LVM_GETNEXTITEM looks neat */
		if (!ListView_GetCheckState(lv, i)) {
			continue;
		}
		path = (char*)malloc(2048);
		if (path == NULL) {
			LGitLog(" ! Uh-oh, no alloc\n");
			continue;
		}
		/* We may need to provide an absolute path. */
		strlcpy(path, params->ctx->workdir_path, 2048);

		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.iItem = i;
		lvi.pszText = relative_path;
		lvi.cchTextMax = 1024;
		SendMessage(lv, LVM_GETITEMTEXT, i, (LPARAM)&lvi);
		/* Combine and ranslate yet again */
		strlcat(path, relative_path, 2048);
		LGitTranslateStringChars(path, '/', '\\');
		LGitLog(" ! Using %s\n", path);
		output[to_set++] = path;
	}
	output[to_set] = NULL;
	LGitLog(" ! Returning %ld files\n", to_set);
	*params->output_size = to_set;
	*params->output = output;
}

static BOOL CALLBACK AddFromDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitAddFromDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitAddFromDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitAddFromView(hwnd, param);
		if (!FillAddFromView(hwnd, param)) {
			EndDialog(hwnd, 0);
		}
		return TRUE;
	case WM_COMMAND:
		param = (LGitAddFromDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			BuildAddList(hwnd, param);
			EndDialog(hwnd, 2);
			return TRUE;
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN SccAddFromScc (LPVOID context, 
					  HWND hWnd, 
					  LPLONG pnFiles,
					  LPCSTR** lplpFileNames)
{
	git_index *index;
	SCCRTN ret = SCC_OK;
	
	LGitAddFromDialogParams params;

	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccAddFromScc**\n");

	if (pnFiles == NULL) {
		/*
		 * Make sure we don't free the initial value that we were given.
		 * XXX: Can we check this without setting it in the context? Null OK?
		 */
		LGitLog("  Freeing %p?\n", lplpFileNames);
		if (*lplpFileNames != NULL && ctx->addSccSuccess) {
			const char **files = *lplpFileNames;
			int i = 0;
			while (files[i] != NULL) {
				LGitLog("  Freeing item %p[%d] = %s\n", files, i, files[i]);
				free((void*)files[i]);
				i++;
			}
			LGitLog("  Freeing array.\n");
			free(files);
		}
		return SCC_OK;
	}
	LGitLog("  files %d\n", *pnFiles);
	ctx->addSccSuccess = FALSE;
	/*
	 * Only subdirectories are an acceptable constraint. Anything outside of
	 * the workdir doesn't make sense. We only need the path for the add
	 * dialog, so keep it stack local.
	 */
	char path[_MAX_PATH];
	path[0] = '\0';
	const char *raw_path;
	if (*pnFiles == 1 && lplpFileNames != NULL) {
		raw_path = LGitStripBasePath(ctx, **lplpFileNames);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", **lplpFileNames);
			return SCC_E_NONSPECIFICERROR;
		}
		/* Translate because libgit2 operates with forward slashes */
		strlcpy(path, raw_path, _MAX_PATH);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog(" ! Destination is '%s'\n", path);
		/* If it's just the root workdir directory, don't bother */
		if (strlen(path) == 0) {
			LGitLog(" ! Empty path, not using\n");
		} else if (path[strlen(path) - 1] != '/') {
			/* Make sure it has a / */
			strlcat(path, "/", _MAX_PATH);
			LGitLog(" ! Appended slash\n");
		}
	}

	LGitLog (" ! Getting index for share\n");
	if (git_repository_index(&index, ctx->repo) != 0) {
		return SCC_E_NONSPECIFICERROR;
	}

	params.ctx = ctx;
	params.index = index;
	params.output_size = pnFiles;
	params.output = lplpFileNames;
	params.restraint_path = path;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_ADDFROM),
		hWnd,
		AddFromDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	case 1:
		LGitLog(" ! Cancelled\n");
		ret = SCC_I_OPERATIONCANCELED;
		goto fin;
	case 2:
		ctx->addSccSuccess = TRUE;
		break;
	}
fin:
	git_index_free(index);
	return ret;
}

/* There is the AddFilesFromSCC which is a 1.3 thing used by VS2003/2005. */