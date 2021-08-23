/*
 * ...branches, of course.
 */

#include <stdafx.h>

typedef struct _LGitAddBranchDialogParams {
	LGitContext *ctx;
	char new_name[128];
	BOOL force;
} LGitAddBranchDialogParams;

static void InitBranchAddView(HWND hwnd, LGitAddBranchDialogParams* params)
{
	SetDlgItemText(hwnd, IDC_BRANCH_ADD_NAME, params->new_name);
	CheckDlgButton(hwnd, IDC_BRANCH_ADD_FORCE, params->force ? BST_CHECKED : BST_UNCHECKED);
}

static BOOL SetBranchAddParams(HWND hwnd, LGitAddBranchDialogParams* params)
{
	GetDlgItemText(hwnd, IDC_BRANCH_ADD_NAME, params->new_name, 128);
	int valid = 0;
	/* if there's an error, then if it's valid */
	if (git_branch_name_is_valid(&valid, params->new_name) != 0) {
		LGitLibraryError(hwnd, "git_branch_name_is_valid");
		return FALSE;
	}
	if (!valid) {
		MessageBox(hwnd,
			"The branch name is invalid.",
			"Invalid Branch Name",
			MB_ICONWARNING);
		return FALSE;
	}
	params->force = IsDlgButtonChecked(hwnd, IDC_BRANCH_ADD_FORCE) == BST_CHECKED;
	return TRUE;
}

static BOOL CALLBACK AddBranchDialogProc(HWND hwnd,
										 unsigned int iMsg,
										 WPARAM wParam,
										 LPARAM lParam)
{
	LGitAddBranchDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitAddBranchDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitBranchAddView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitAddBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			if (SetBranchAddParams(hwnd, param)) {
				EndDialog(hwnd, 2);
			}
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

typedef struct _LGitBranchDialogParams {
	LGitContext *ctx;
	/* for label editor */
	char old_name[128];
	BOOL editing;
} LGitBranchDialogParams ;

/* put here for convenience */
static LVCOLUMN full_name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Full Name"
};

static LVCOLUMN name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Name"
};

static LVCOLUMN type_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 75, "Type"
};

static void InitBranchView(HWND hwnd, LGitBranchDialogParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);

	ListView_InsertColumn(lv, 0, &name_column);
	ListView_InsertColumn(lv, 1, &full_name_column);
	ListView_InsertColumn(lv, 2, &type_column);
}

static void FillBranchView(HWND hwnd, LGitBranchDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	size_t index = 0;
	/* clear if we're replenishing */
	ListView_DeleteAllItems(lv);
	git_branch_iterator *iter = NULL;
	git_reference *ref = NULL;
	git_branch_t type;
	int rc;
	if (git_branch_iterator_new(&iter, params->ctx->repo, GIT_BRANCH_ALL) != 0) {
		LGitLibraryError(hwnd, "git_branch_iterator_new");
		return;
	}
	while ((rc = git_branch_next(&ref, &type, iter)) == 0) {
		const char *name = NULL;
		if (git_branch_name(&name, ref) != 0) {
			LGitLog("!! Ope\n");
			continue;
		}
		LGitLog(" ! %x %s\n", type, name);
		LVITEM lvi;
		
		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (char*)name;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		lvi.iItem = ListView_InsertItem(lv, &lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			continue;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		lvi.pszText = (char*)git_reference_name(ref);
		ListView_SetItem(lv, &lvi);
		char type_str[128];
		strlcpy(type_str, LGitBranchType(type), 128);
		if (git_branch_is_checked_out(ref)) {
			strlcat(type_str, ", Checked Out", 128);
		}
		if (git_branch_is_head(ref)) {
			strlcat(type_str, ", HEAD", 128);
		}
		lvi.iSubItem = 2;
		lvi.pszText = (char*)type_str;
		ListView_SetItem(lv, &lvi);
	}
	if (rc != GIT_ITEROVER) {
		LGitLibraryError(hwnd, "git_branch_next");
		return;
	}
	if (iter != NULL) {
		git_branch_iterator_free(iter);
	}
}

static BOOL GetSelectedBranch(HWND hwnd, char *buf, size_t bufsz)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return FALSE;
	}
	/* We want the full reference name here */
	ListView_GetItemText(lv, selected, 1, buf, bufsz);
	return TRUE;
}

static void BranchCheckout(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! Checking out %s?\n", name);
	if (LGitCheckoutRefByName(params->ctx, hwnd, name) == SCC_OK) {
		FillBranchView(hwnd, params);
	}
}

static void BranchMerge(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! Checking out %s?\n", name);
	if (LGitMergeRefByName(params->ctx, hwnd, name) == SCC_OK) {
		FillBranchView(hwnd, params);
	}
}

static void BranchRemove(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! Removing %s?\n", name);
	/* git_branch_delete will check if we can delete, but it needs a handle */
	git_reference *ref = NULL;
	/* it would be weird if we got not found here */
	if (git_reference_lookup(&ref, params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_reference_lookup");
		return;
	}
	if (MessageBox(hwnd,
		"The branch will be removed.",
		"Remove Branch?",
		MB_ICONWARNING | MB_YESNO) != IDYES) {
		goto err;
	}
	if (git_branch_delete(ref) != 0) {
		LGitLibraryError(hwnd, "git_remote_delete");
		goto err;
	}
	/* Optimization would be removing the list view item */
	FillBranchView(hwnd, params);
err:
	if (ref != NULL) {
		git_reference_free(ref);
	}
}

static void BranchAdd(HWND hwnd, LGitBranchDialogParams* params)
{
	LGitAddBranchDialogParams ab_params;
	ZeroMemory(&ab_params, sizeof(LGitAddBranchDialogParams));
	ab_params.ctx = params->ctx;
	switch (DialogBoxParam(params->ctx->dllInst,
		MAKEINTRESOURCE(IDD_BRANCH_ADD),
		hwnd,
		AddBranchDialogProc,
		(LPARAM)&ab_params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		return;
	case 1:
		return;
	case 2:
		break;
	}
	git_reference *branch = NULL;
	git_oid commit_oid;
	git_commit *commit = NULL;
	int rc;
	/*
	 * We need the commit of the branch to base off of; assume HEAD for now.
	 * XXX: Is it safe to make a branch without a commit (unborn); if so, just
	 * skip the init here and go to the branch create call.
	 */
	if (git_reference_name_to_id(&commit_oid, params->ctx->repo, "HEAD") != 0) {
		LGitLibraryError(hwnd, "git_reference_name_to_id");
		goto err;
	}
	if (git_commit_lookup(&commit, params->ctx->repo, &commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	rc = git_branch_create(&branch,
		params->ctx->repo,
		ab_params.new_name,
		commit,
		ab_params.force);
	switch (rc) {
	case 0:
		break;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The branch has an invalid name.",
			"Invalid Branch",
			MB_ICONERROR);
		goto err;
	case GIT_EEXISTS:
		MessageBox(hwnd,
			"The branch by that name already exists.",
			"Invalid Branch",
			MB_ICONERROR);
		goto err;
	default:
		LGitLibraryError(hwnd, "git_branch_create");
		goto err;
	}
	/* XXX: Should we check out after? */
	FillBranchView(hwnd, params);
err:
	if (branch != NULL) {
		git_reference_free(branch);
	}
	if (commit != NULL) {
		git_commit_free(commit);
	}
}

static BOOL BeginBranchRename(HWND hwnd, LGitBranchDialogParams* params, UINT index)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	ListView_GetItemText(lv, index, 1, params->old_name, 128);
	params->editing = TRUE;
	return TRUE;
}

static BOOL BranchRename(HWND hwnd, LGitBranchDialogParams* params, const char *new_name)
{
	LGitLog(" ! Renaming %s to %s\n", params->old_name, new_name);
	params->editing = FALSE;
	/* Check if it's the same name or null; lg2 will throw an error if so. */
	if (new_name == NULL || strcmp(params->old_name, new_name) == 0) {
		return FALSE;
	}
	/* We need the handle of the branch. */
	BOOL ret = FALSE;
	git_reference *branch = NULL, *new_branch = NULL;
	/* it would be weird if we got not found here */
	if (git_reference_lookup(&branch, params->ctx->repo, params->old_name) != 0) {
		LGitLibraryError(hwnd, "git_reference_lookup");
		goto fin;
	}
	switch (git_branch_move(&new_branch, branch, new_name, 0)) {
	case 0:
		ret = TRUE;
		FillBranchView(hwnd, params);
		goto fin;
		/* XXX: These messages are common with New */
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The branch has an invalid name.",
			"Invalid Branch",
			MB_ICONERROR);
		goto fin;
	case GIT_EEXISTS:
		MessageBox(hwnd,
			"The remote by that name already exists.",
			"Invalid Branch",
			MB_ICONERROR);
		goto fin;
	default:
		LGitLibraryError(hwnd, "git_remote_rename");
		goto fin;
	}
fin:
	if (branch != NULL) {
		git_reference_free(branch);
	}
	if (new_branch != NULL) {
		git_reference_free(new_branch);
	}
	return ret;
}

static BOOL CALLBACK BranchManagerDialogProc(HWND hwnd,
											 unsigned int iMsg,
											 WPARAM wParam,
											 LPARAM lParam)
{
	LGitBranchDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitBranchDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitBranchView(hwnd, param);
		FillBranchView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		if (param->editing) {
			/* so editing in the label won't close the dialog */
			return TRUE;
		}
		switch (LOWORD(wParam)) {
		case IDC_BRANCH_ADD:
			BranchAdd(hwnd, param);
			return TRUE;
		case IDC_BRANCH_DELETE:
			BranchRemove(hwnd, param);
			return TRUE;
		case IDC_BRANCH_CHECKOUT:
			BranchCheckout(hwnd, param);
			return TRUE;
		case IDC_BRANCH_MERGE:
			BranchMerge(hwnd, param);
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		param = (LGitBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (wParam) {
		case IDC_BRANCH_LIST:
			LPNMHDR child_msg = (LPNMHDR)lParam;
			LPNMITEMACTIVATE child_activate = (LPNMITEMACTIVATE)lParam;
			NMLVDISPINFO *child_edit = (NMLVDISPINFO*)lParam;
			HWND lv = (HWND)wParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				ListView_EditLabel(lv, child_activate->iItem);
				return TRUE;
			case LVN_BEGINLABELEDIT:
				return BeginBranchRename(hwnd, param, child_edit->item.iItem);
			case LVN_ENDLABELEDIT:
				return BranchRename(hwnd, param, child_edit->item.pszText);
			case LVN_ITEMCHANGED:
				return TRUE;
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN LGitShowBranchManager(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitShowBranchManager** Context=%p\n", ctx);
	LGitBranchDialogParams params;
	ZeroMemory(&params, sizeof(LGitBranchDialogParams));
	params.ctx = ctx;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_BRANCHES),
		hwnd,
		BranchManagerDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_UNKNOWNERROR;
	default:
		break;
	}
	return SCC_OK;
}