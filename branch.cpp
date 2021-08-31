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
	SetDlgItemText(hwnd, IDC_BRANCH_ADD_NAME, params->new_name);
	/* yeah, we should load it from the struct... */
	HWND ref_cb = GetDlgItem(hwnd, IDC_BRANCH_ADD_BASED_ON);
	LGitPopulateReferenceComboBox(hwnd, ref_cb, params->ctx);
	SetDlgItemText(hwnd, IDC_BRANCH_ADD_BASED_ON, "HEAD");
	CheckDlgButton(hwnd, IDC_BRANCH_ADD_FORCE, params->force ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_BRANCH_ADD_CHECKOUT, params->checkout ? BST_CHECKED : BST_UNCHECKED);
}

static BOOL SetBranchAddParams(HWND hwnd, LGitAddBranchDialogParams* params)
{
	GetDlgItemText(hwnd, IDC_BRANCH_ADD_NAME, params->new_name, 128);
	GetDlgItemText(hwnd, IDC_BRANCH_ADD_BASED_ON, params->based_on, 128);
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
	git_reference *ref = NULL;
	switch (git_reference_lookup(&ref, params->ctx->repo, params->based_on)) {
	case GIT_ENOTFOUND:
		MessageBox(hwnd,
			"The reference name to base off of doesn't exist.",
			"Invalid Reference Name",
			MB_ICONWARNING);
		return FALSE;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The reference name to base off of is invalid.",
			"Invalid Reference Name",
			MB_ICONWARNING);
		return FALSE;
	case 0:
		git_reference_free(ref);
		break;
	default:
		LGitLibraryError(hwnd, "git_reference_lookup");
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
	size_t index = 0, i;
	/* clear if we're replenishing */
	ListView_DeleteAllItems(lv);
	git_branch_iterator *iter = NULL;
	git_reference *ref = NULL;
	git_branch_t type;
	int rc;
	/* First branches (XXX: convert to ref list, maybe we can ignore notes) */
	if (git_branch_iterator_new(&iter, params->ctx->repo, GIT_BRANCH_ALL) != 0) {
		LGitLibraryError(hwnd, "git_branch_iterator_new");
		return;
	}
	LVITEM lvi;
	while ((rc = git_branch_next(&ref, &type, iter)) == 0) {
		const char *name = NULL;
		if (git_branch_name(&name, ref) != 0) {
			LGitLog("!! Ope\n");
			continue;
		}
		LGitLog(" ! %x %s\n", type, name);
		
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
	/* Now for tags. We'll want the long name, but we can just add the prefix */
	git_strarray tags = {0};
	if (git_tag_list(&tags, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_tag_list");
		return;
	}
	LGitLog(" ! tag count %d\n", tags.count);
	for (i = 0; i < tags.count; i++) {
		char fullname[128];
		LGitLog(" ! tag %s\n", tags.strings[i]);
		
		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (char*)tags.strings[i];
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		lvi.iItem = ListView_InsertItem(lv, &lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			continue;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		strlcpy(fullname, "refs/tags/", 128);
		strlcat(fullname, tags.strings[i], 128);
		lvi.pszText = (char*)fullname;
		ListView_SetItem(lv, &lvi);
		lvi.iSubItem = 2;
		lvi.pszText = (char*)"Tag"; /* XXX: It can never be HEAD, but guess */
		ListView_SetItem(lv, &lvi);
	}
	git_strarray_dispose(&tags);
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

static void BranchDiff(HWND hwnd, LGitBranchDialogParams* params)
{
	char name[128];
	if (!GetSelectedBranch(hwnd, name, 128)) {
		LGitLog(" ! No branch?\n");
		return;
	}
	LGitLog(" ! Diff for HEAD->%s?\n", name);
	git_diff_options diffopts;
	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);
	LGitInitDiffProgressCallback(params->ctx, &diffopts);
	git_commit *head = NULL, *selected = NULL;
	git_object *selected_object;
	git_oid head_oid, selected_oid;
	if (git_reference_name_to_id(&head_oid, params->ctx->repo, "HEAD") != 0) {
		LGitLibraryError(hwnd, "HEAD git_reference_name_to_id");
		goto fin;
	}
	if (git_reference_name_to_id(&selected_oid, params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "Selected git_reference_name_to_id");
		goto fin;
	}
	if (git_commit_lookup(&head, params->ctx->repo, &head_oid) != 0) {
		LGitLibraryError(hwnd, "HEAD git_commit_lookup");
		goto fin;
	}
	/* This could be a hard tag object instead, try to get it peeled */
	if (git_object_lookup(&selected_object, params->ctx->repo, &selected_oid, GIT_OBJECT_ANY) != 0) {
		LGitLibraryError(hwnd, "Selected git_object_lookup");
		goto fin;
	}
	if (git_object_peel((git_object**)&selected, selected_object, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "Selected git_object_peel");
		goto fin;
	}
	/* We'll compare this commit against HEAD... */
	LGitCommitToCommitDiff(params->ctx, hwnd, selected, head, &diffopts);
fin:
	if (head != NULL) {
		git_commit_free(head);
	}
	if (selected != NULL) {
		git_commit_free(selected);
	}
	if (selected_object != NULL) {
		git_object_free(selected_object);
	}
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
	git_object *selected_object;
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
	LGitViewCommitInfo(params->ctx, hwnd, selected);
fin:
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
		"Remove Branch?",
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
	const char *branch_name;
	int rc;
	/*
	 * We need the commit of the branch to base off of; assume HEAD for now.
	 * XXX: Is it safe to make a branch without a commit (unborn); if so, just
	 * skip the init here and go to the branch create call.
	 */
	if (git_reference_name_to_id(&commit_oid, params->ctx->repo, ab_params.based_on) != 0) {
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
	int rc = -1;
	/* it would be weird if we got not found here */
	if (git_reference_lookup(&branch, params->ctx->repo, params->old_name) != 0) {
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
		case IDC_BRANCH_HISTORY:
			BranchHistory(hwnd, param);
			return TRUE;
		case IDC_BRANCH_DIFF:
			BranchDiff(hwnd, param);
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
				ReferenceView(hwnd, param);
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