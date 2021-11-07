/*
 * ...branches, of course. And other references like tags!
 */

#include <stdafx.h>

typedef struct _LGitAddBranchDialogParams {
	LGitContext *ctx;
	char new_name[128];
	char based_on[128];
	BOOL force, checkout;
} LGitAddBranchDialogParams;

static void InitBranchAddView(HWND hwnd, LGitAddBranchDialogParams* params)
{
	wchar_t new_name[128];
	LGitUtf8ToWide(params->new_name, new_name, 128);
	SetDlgItemTextW(hwnd, IDC_BRANCH_ADD_NAME, new_name);
	/* yeah, we should load it from the struct... */
	HWND ref_cb = GetDlgItem(hwnd, IDC_BRANCH_ADD_BASED_ON);
	LGitPopulateReferenceComboBox(hwnd, ref_cb, params->ctx);
	SetDlgItemText(hwnd, IDC_BRANCH_ADD_BASED_ON, "HEAD");
	CheckDlgButton(hwnd, IDC_BRANCH_ADD_FORCE, params->force ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_BRANCH_ADD_CHECKOUT, params->checkout ? BST_CHECKED : BST_UNCHECKED);
}

static BOOL SetBranchAddParams(HWND hwnd, LGitAddBranchDialogParams* params)
{
	wchar_t new_name[128], based_on[128];
	GetDlgItemTextW(hwnd, IDC_BRANCH_ADD_NAME, new_name, 128);
	LGitWideToUtf8(new_name, params->new_name, 128);
	GetDlgItemTextW(hwnd, IDC_BRANCH_ADD_BASED_ON, based_on, 128);
	LGitWideToUtf8(based_on, params->based_on, 128);
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
	/* check if the reference exists */
	git_object *obj = NULL;
	switch (git_revparse_single(&obj, params->ctx->repo, params->based_on)) {
	case GIT_ENOTFOUND:
		MessageBox(hwnd,
			"The revision to base off of doesn't exist.",
			"Invalid Revision",
			MB_ICONWARNING);
		return FALSE;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The revision to base off of is invalid.",
			"Invalid Revision",
			MB_ICONWARNING);
		return FALSE;
	case GIT_EAMBIGUOUS:
		MessageBox(hwnd,
			"The revision given isn't specific enough.",
			"Invalid Revision",
			MB_ICONWARNING);
		return FALSE;
	case 0:
		git_object_free(obj);
		break;
	default:
		LGitLibraryError(hwnd, "git_revparse_single");
		return FALSE;
	}
	params->force = IsDlgButtonChecked(hwnd, IDC_BRANCH_ADD_FORCE) == BST_CHECKED;
	params->checkout = IsDlgButtonChecked(hwnd, IDC_BRANCH_ADD_CHECKOUT) == BST_CHECKED;
	return TRUE;
}

static BOOL CALLBACK AddBranchDialogProc(HWND hwnd,
										 unsigned int iMsg,
										 WPARAM wParam,
										 LPARAM lParam)
{
	LGitAddBranchDialogParams *param;
	param = (LGitAddBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitAddBranchDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitBranchAddView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
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
	case WM_MEASUREITEM:
		if (wParam == IDC_BRANCH_ADD_BASED_ON) {
			return LGitMeasureIconComboBoxItem(hwnd, wParam, (MEASUREITEMSTRUCT *)lParam);
		}
		return FALSE;
	case WM_DRAWITEM:
		if (wParam == IDC_BRANCH_ADD_BASED_ON) {
			return LGitDrawIconComboBox(param->ctx, param->ctx->refTypeIl, hwnd, wParam, (DRAWITEMSTRUCT *)lParam);
		}
		return FALSE;
	default:
		return FALSE;
	}
}

typedef struct _LGitBranchDialogParams {
	LGitContext *ctx;
	HMENU menu;
	/* for label editor */
	wchar_t old_name[128];
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
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Status"
};

static void InitBranchView(HWND hwnd, LGitBranchDialogParams* params)
{
	SetMenu(hwnd, params->menu);
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);

	ListView_SetUnicodeFormat(lv, TRUE);
	SendMessage(lv, WM_SETFONT, (WPARAM)params->ctx->listviewFont, TRUE);

	ListView_InsertColumn(lv, 0, &name_column);
	ListView_InsertColumn(lv, 1, &full_name_column);
	ListView_InsertColumn(lv, 2, &type_column);

	ListView_SetImageList(lv, params->ctx->refTypeIl, LVSIL_SMALL);
}

static void FillBranchView(HWND hwnd, LGitBranchDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	size_t index = 0;
	/* clear if we're replenishing */
	ListView_DeleteAllItems(lv);
	git_reference_iterator *iter = NULL;
	git_reference *ref = NULL;
	int rc;
	if (git_reference_iterator_new(&iter, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_reference_iterator_new");
		return;
	}
	LVITEMW lvi;
	while ((rc = git_reference_next(&ref, iter)) == 0) {
		wchar_t name_utf16[128];
		const char *name = git_reference_shorthand(ref);
		if (name == NULL) {
			LGitLog("!! Ope\n");
			continue;
		}
		/*LGitLog(" ! %s\n", name);*/
		LGitUtf8ToWide(name, name_utf16, 128);
		
		ZeroMemory(&lvi, sizeof(LVITEMW));
		lvi.mask = LVIF_TEXT | LVIF_IMAGE;
		lvi.iImage = LGitGetIconForRef(params->ctx, ref);
		lvi.pszText = name_utf16;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		lvi.iItem = SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			continue;
		}
		/* now for the subitems... */
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		LGitUtf8ToWide(git_reference_name(ref), name_utf16, 128);
		lvi.pszText = name_utf16;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
		wchar_t type_str[32];
		ZeroMemory(type_str, 32);
		if (git_branch_is_checked_out(ref)) {
			wcslcat(type_str, L", Checked Out", 32);
		}
		if (git_branch_is_head(ref)) {
			wcslcat(type_str, L", HEAD", 32);
		}
		lvi.iSubItem = 2;
		lvi.pszText = (wchar_t*)(type_str[0] == L',' ? type_str + 2 : type_str);
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
	}
	if (rc != GIT_ITEROVER) {
		LGitLibraryError(hwnd, "git_reference_next");
		return;
	}
	if (iter != NULL) {
		git_reference_iterator_free(iter);
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
	wchar_t temp_buf[1024];
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = selected;
	/* We want the full reference name here */
	lvi.iSubItem = 1;
	lvi.pszText = temp_buf;
	lvi.cchTextMax = 1024;
	if (SendMessage(lv, LVM_GETITEMTEXTW, selected, (LPARAM)&lvi) < 1) {
		return FALSE;
	}
	LGitWideToUtf8(temp_buf, buf, bufsz);
	return TRUE;
}

static BOOL GetSelectedBranch(HWND hwnd, int *index)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return FALSE;
	}
	*index = selected;
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

static void BranchHistory(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! History for %s\n", name);
	LGitHistoryForRefByName(params->ctx, hwnd, name);
}

/* XXX: Offer a stage version. */
static void BranchDiff(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! Diff for Chosen->%s?\n", name);
	git_diff_options diffopts;
	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);
	LGitInitDiffProgressCallback(params->ctx, &diffopts);
	/* selected -> from the listview, chosen -> from the prompt */
	git_commit *chosen_commit = NULL, *selected_commit = NULL;
	git_object *chosen_object = NULL, *selected_object = NULL;
	git_reference *chosen_ref = NULL;
	git_oid selected_oid;
	if (git_reference_name_to_id(&selected_oid, params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "Selected git_reference_name_to_id");
		goto fin;
	}
	if (LGitRevparseDialog(params->ctx, hwnd, "Diff from Revision", "HEAD", &chosen_object, &chosen_ref) == SCC_OK) {
		/* XXX: We should be peeling a tree */
		if (git_object_peel((git_object**)&chosen_commit, chosen_object, GIT_OBJECT_COMMIT) != 0) {
			LGitLibraryError(hwnd, "Couldn't Peel Chosen Commit");
		}
	}
	/* This could be a hard tag object instead, try to get it peeled */
	if (git_object_lookup(&selected_object, params->ctx->repo, &selected_oid, GIT_OBJECT_ANY) != 0) {
		LGitLibraryError(hwnd, "Selected git_object_lookup");
		goto fin;
	}
	if (git_object_peel((git_object**)&selected_commit, selected_object, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "Selected git_object_peel");
		goto fin;
	}
	/* We'll compare this commit against HEAD... */
	LGitCommitToCommitDiff(params->ctx, hwnd, selected_commit, chosen_commit, &diffopts);
fin:
	git_reference_free(chosen_ref);
	git_commit_free(chosen_commit);
	git_object_free(chosen_object);
	git_commit_free(selected_commit);
	git_object_free(selected_object);
}

/**
 * Views the object behind a reference, could be a commit or tag.
 */
static void ReferenceView(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! View for %s?\n", name);
	git_commit *selected = NULL;
	git_tag *selected_tag = NULL;
	git_object *selected_object = NULL;
	git_oid selected_oid;
	if (git_reference_name_to_id(&selected_oid, params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_reference_name_to_id");
		goto fin;
	}
	/* Get commit and (XXX) tag object if possible */
	if (git_object_lookup(&selected_object, params->ctx->repo, &selected_oid, GIT_OBJECT_ANY) != 0) {
		LGitLibraryError(hwnd, "git_object_lookup");
		goto fin;
	}
	if (git_object_peel((git_object**)&selected, selected_object, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "Commit git_object_peel");
		goto fin;
	}
	if (git_object_peel((git_object**)&selected_tag, selected_object, GIT_OBJECT_TAG) != 0) {
		/* not important */
	}
	LGitViewCommitInfo(params->ctx, hwnd, selected, selected_tag);
fin:
	if (selected_tag != NULL) {
		git_tag_free(selected_tag);
	}
	if (selected != NULL) {
		git_commit_free(selected);
	}
	if (selected_object != NULL) {
		git_object_free(selected_object);
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
		"The reference will be removed.",
		"Remove Reference?",
		MB_ICONWARNING | MB_YESNO) != IDYES) {
		goto err;
	}
	/* Special case only for branches; tag delete just does lookup for us */
	if (git_reference_is_branch(ref) || git_reference_is_remote(ref)) {
		if (git_branch_delete(ref) != 0) {
			LGitLibraryError(hwnd, "git_remote_delete");
			goto err;
		}
	} else {
		if (git_reference_delete(ref) != 0) {
			LGitLibraryError(hwnd, "git_reference_delete");
			goto err;
		}
	}
	/* Optimization would be removing the list view item */
	FillBranchView(hwnd, params);
err:
	if (ref != NULL) {
		git_reference_free(ref);
	}
}

static void TagAdd(HWND hwnd, LGitBranchDialogParams *params)
{
	if (LGitAddTagDialog(params->ctx, hwnd) == SCC_OK) {
		FillBranchView(hwnd, params);
	}
}

static void BranchAdd(HWND hwnd, LGitBranchDialogParams* params)
{
	LGitAddBranchDialogParams ab_params;
	ZeroMemory(&ab_params, sizeof(LGitAddBranchDialogParams));
	ab_params.ctx = params->ctx;
	switch (DialogBoxParamW(params->ctx->dllInst,
		MAKEINTRESOURCEW(IDD_BRANCH_ADD),
		hwnd,
		AddBranchDialogProc,
		(LPARAM)&ab_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return;
	case 1:
		return;
	case 2:
		break;
	}
	git_reference *branch = NULL;
	git_object *object = NULL;
	git_commit *commit = NULL;
	const char *branch_name;
	int rc;
	/*
	 * XXX: Is it safe to make a branch without a commit (unborn); if so, just
	 * skip the init here and go to the branch create call.
	 */
	if (git_revparse_single(&object, params->ctx->repo, ab_params.based_on) != 0) {
		LGitLibraryError(hwnd, "git_revparse_single");
		goto err;
	}
	if (git_object_peel((git_object**)&commit, object, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "git_object_peel");
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
	branch_name = git_reference_name(branch);
	if (ab_params.checkout) {
		if (LGitCheckoutRefByName(params->ctx, hwnd, branch_name) != SCC_OK) {
			/* it'll handle messages for us */
			goto err;
		}
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
	if (object != NULL) {
		git_object_free(object);
	}
}

static BOOL BeginBranchRename(HWND hwnd, LGitBranchDialogParams* params, UINT index)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	LGitLog(" ! Begin rename for %u\n", index);
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.pszText = params->old_name;
	lvi.cchTextMax = 128;
	if (SendMessage(lv, LVM_GETITEMTEXTW, index, (LPARAM)&lvi) < 1) {
		LGitLog("!! Failed to get rename text for %u\n", index);
		return FALSE;
	}
	params->editing = TRUE;
	LGitLog(" ! Beginning rename for %S\n", params->old_name);
	return TRUE;
}

static BOOL BranchRename(HWND hwnd, LGitBranchDialogParams* params, const wchar_t *new_name_utf16)
{
	LGitLog(" ! Renaming %S to %S\n", params->old_name, new_name_utf16);
	params->editing = FALSE;
	/* Check if it's the same name or null; lg2 will throw an error if so. */
	if (new_name_utf16 == NULL || wcscmp(params->old_name, new_name_utf16) == 0) {
		LGitLog("!! New name is empty\n");
		return FALSE;
	}
	/* preserve existing semantics w/ utf8 strings */
	char new_name[128], old_name[128];
	LGitWideToUtf8(params->old_name, old_name, 128);
	LGitWideToUtf8(new_name_utf16, new_name, 128);
	/* We need the handle of the branch. */
	BOOL ret = FALSE;
	git_reference *branch = NULL, *new_branch = NULL;
	int rc = -1;
	/* it would be weird if we got not found here */
	if (git_reference_lookup(&branch, params->ctx->repo, old_name) != 0) {
		LGitLibraryError(hwnd, "git_reference_lookup");
		goto fin;
	}
	/* Special function only for branches. All else needs special prefix */
	if (git_reference_is_branch(branch) || git_reference_is_remote(branch)) {
		rc = git_branch_move(&new_branch, branch, new_name, 0);
	} else if (git_reference_is_tag(branch)) {
		char fullname[128];
		int valid = 0;
		if (git_tag_name_is_valid(&valid, new_name) != 0 || !valid) {
			MessageBox(hwnd,
				"The tag name is invalid.",
				"Invalid Tag Name",
				MB_ICONERROR);
			goto fin;
		}
		strlcpy(fullname, "refs/tags/", 128);
		strlcat(fullname, new_name, 128);
		rc = git_reference_rename(&new_branch, branch, fullname, 0, NULL);
	} else {
		/* IDK */
		MessageBox(hwnd,
			"Renaming this reference isn't supported.",
			"Can't Rename Reference",
			MB_ICONERROR);
		goto fin;
	}
	switch (rc) {
	case 0:
		ret = TRUE;
		FillBranchView(hwnd, params);
		goto fin;
		/* XXX: These messages are common with New */
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The reference has an invalid name.",
			"Invalid Reference",
			MB_ICONERROR);
		goto fin;
	case GIT_EEXISTS:
		MessageBox(hwnd,
			"The reference by that name already exists.",
			"Invalid Reference",
			MB_ICONERROR);
		goto fin;
	default:
		LGitLibraryError(hwnd, "Rename Reference");
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

static void UpdateRefMenu(HWND hwnd, LGitBranchDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
	UINT selected = ListView_GetSelectedCount(lv);
	UINT newState = MF_BYCOMMAND
		| (selected ? MF_ENABLED : MF_GRAYED);
#define EnableMenuItemIfCommitSelected(id) EnableMenuItem(params->menu,id,newState)
	EnableMenuItemIfCommitSelected(ID_REFERENCE_REMOVE);
	EnableMenuItemIfCommitSelected(ID_REFERENCE_CHECKOUT);
	EnableMenuItemIfCommitSelected(ID_REFERENCE_MERGE);
	EnableMenuItemIfCommitSelected(ID_REFERENCE_HISTORY);
	EnableMenuItemIfCommitSelected(ID_REFERENCE_DIFF);
	EnableMenuItemIfCommitSelected(ID_REFERENCE_VIEW);
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
		LGitSetWindowIcon(hwnd, param->ctx->dllInst, MAKEINTRESOURCE(IDI_BRANCH));
		InitBranchView(hwnd, param);
		FillBranchView(hwnd, param);
		LGitControlFillsParentDialog(hwnd, IDC_BRANCH_LIST);
		UpdateRefMenu(hwnd, param);
		return TRUE;
	case WM_SIZE:
		LGitControlFillsParentDialog(hwnd, IDC_BRANCH_LIST);
		return TRUE;
	case WM_CONTEXTMENU:
		param = (LGitBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		return LGitContextMenuFromSubmenu(hwnd, param->menu, 1, LOWORD(lParam), HIWORD(lParam));
	case WM_COMMAND:
		param = (LGitBranchDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		if (param->editing) {
			/* so editing in the label won't close the dialog */
			return TRUE;
		}
		switch (LOWORD(wParam)) {
		case ID_BRANCH_ADD:
			BranchAdd(hwnd, param);
			return TRUE;
		case ID_TAG_ADD:
			TagAdd(hwnd, param);
			return TRUE;
		case ID_REFERENCE_REMOVE:
			BranchRemove(hwnd, param);
			return TRUE;
		case ID_REFERENCE_CHECKOUT:
			BranchCheckout(hwnd, param);
			return TRUE;
		case ID_REFERENCE_MERGE:
			BranchMerge(hwnd, param);
			return TRUE;
		case ID_REFERENCE_HISTORY:
			BranchHistory(hwnd, param);
			return TRUE;
		case ID_REFERENCE_DIFF:
			BranchDiff(hwnd, param);
			return TRUE;
		case ID_REFERENCE_VIEW:
			ReferenceView(hwnd, param);
			return TRUE;
		/* XXX: LVM_EDITLABEL doesn't want to work, yet does when by user?
		case ID_REFERENCE_RENAME:
			{
				// we need to get the index ourselves, also focus
				HWND lv = GetDlgItem(hwnd, IDC_BRANCH_LIST);
				SetFocus(lv);
				int selected;
				if (GetSelectedBranch(hwnd, &selected)) {
					LGitLog(" ! %p <- %d\n", ListView_EditLabel(hwnd, selected), selected);
				}
			}
			return TRUE;
		*/
		case ID_REFERENCE_REFRESH:
			FillBranchView(hwnd, param);
			return TRUE;
		case ID_REFERENCE_CLOSE:
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
			NMLVDISPINFOW *child_edit = (NMLVDISPINFOW*)lParam;
			HWND lv = (HWND)wParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				ReferenceView(hwnd, param);
				return TRUE;
			case LVN_BEGINLABELEDITW:
				return BeginBranchRename(hwnd, param, child_edit->item.iItem);
			case LVN_ENDLABELEDITW:
				return BranchRename(hwnd, param, child_edit->item.pszText);
			case LVN_ITEMCHANGED:
				UpdateRefMenu(hwnd, param);
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
	params.menu = LoadMenu(ctx->dllInst, MAKEINTRESOURCE(IDR_REFERENCE_MENU));
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_BRANCHES),
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
	DestroyMenu(params.menu);
	return SCC_OK;
}