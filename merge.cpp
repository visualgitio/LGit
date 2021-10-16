/*
 * Convenience functions for merging..
 */

#include "stdafx.h"

SCCRTN LGitMergeFastForward(LGitContext *ctx, HWND hwnd, const git_oid *target_oid, BOOL is_unborn)
{
	SCCRTN ret = SCC_OK;
	LGitLog("**LGitMergeFastForward** Context=%p\n", ctx);
	LGitLog("  oid %s\n", git_oid_tostr_s(target_oid));

	/* adapted from perform_fastforward in examples/merge.c */
	git_checkout_options ff_checkout_options;
	git_checkout_options_init(&ff_checkout_options, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &ff_checkout_options);
	git_reference *target_ref = NULL;
	git_reference *new_target_ref = NULL;
	git_object *target = NULL;
	int err = 0;

	if (is_unborn) {
		const char *symbolic_ref;
		git_reference *head_ref;

		/* HEAD reference is unborn, lookup manually so we don't try to resolve it */
		err = git_reference_lookup(&head_ref, ctx->repo, "HEAD");
		if (err != 0) {
			LGitLibraryError(hwnd, "failed to lookup HEAD ref");
			ret = SCC_E_UNKNOWNERROR;
			goto fin;
		}

		/* Grab the reference HEAD should be pointing to */
		symbolic_ref = git_reference_symbolic_target(head_ref);

		/* Create our master reference on the target OID */
		err = git_reference_create(&target_ref, ctx->repo, symbolic_ref, target_oid, 0, NULL);
		if (err != 0) {
			LGitLibraryError(hwnd, "failed to create HEAD reference");
			ret = SCC_E_UNKNOWNERROR;
			goto fin;
		}

		git_reference_free(head_ref);
	} else {
		/* HEAD exists, just lookup and resolve */
		err = git_repository_head(&target_ref, ctx->repo);
		if (err != 0) {
			LGitLibraryError(hwnd, "failed to get HEAD reference");
			ret = SCC_E_UNKNOWNERROR;
			goto fin;
		}
	}

	/* Lookup the target object */
	err = git_object_lookup(&target, ctx->repo, target_oid, GIT_OBJECT_COMMIT);
	if (err != 0) {
		LGitLibraryError(hwnd, "failed to lookup OID");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}

	/* Checkout the result so the workdir is in the expected state */
	ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
	LGitProgressInit(ctx, "Fast-Forward", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	err = git_checkout_tree(ctx->repo, target, &ff_checkout_options);
	LGitProgressDeinit(ctx);
	if (err != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "failed to checkout HEAD reference");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}

	/* Move the target reference to the target OID */
	err = git_reference_set_target(&new_target_ref, target_ref, target_oid, NULL);
	if (err != 0) {
		LGitLibraryError(hwnd, "failed to move HEAD reference");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
fin:
	if (target_ref != NULL) {
		git_reference_free(target_ref);
	}
	if (new_target_ref != NULL) {
		git_reference_free(new_target_ref);
	}
	if (target!= NULL) {
		git_object_free(target);
	}
	return ret;
}

SCCRTN LGitMergeNormal(LGitContext *ctx, HWND hwnd, const git_annotated_commit *ac, git_merge_preference_t preference)
{
	LGitLog("**LGitMergeNormal** Context=%p\n", ctx);
	/* adapted from perform_fastforward in examples/merge.c */
	git_checkout_options co_opts;
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	git_merge_options merge_opts;
	git_merge_options_init(&merge_opts, GIT_MERGE_OPTIONS_VERSION);

	merge_opts.flags = 0;
	merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;

	co_opts.checkout_strategy = GIT_CHECKOUT_FORCE|GIT_CHECKOUT_ALLOW_CONFLICTS;

	if (preference & GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY) {
		MessageBox(hwnd,
			"Fast-forward is preferred, but only a merge is possible.",
			"Can't Merge",
			MB_ICONERROR);
		return SCC_E_UNKNOWNERROR;
	}

	LGitProgressInit(ctx, "Merging", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	int rc = git_merge(ctx->repo,
		(const git_annotated_commit **)&ac, 1,
		&merge_opts, &co_opts);
	LGitProgressDeinit(ctx);
	if (rc != 0) {
		LGitLibraryError(hwnd, "git_merge");
		return SCC_E_UNKNOWNERROR;
	}

	return SCC_OK;
}

SCCRTN LGitMerge(LGitContext *ctx, HWND hwnd, const git_annotated_commit *ann)
{
	SCCRTN ret = SCC_OK;
	git_merge_analysis_t merge_analysis;
	git_merge_preference_t merge_preference;
	git_index *index = NULL;
	int repo_state;
	LGitLog("**LGitMerge** Context=%p\n", ctx);
	LGitLog("  annotated commit %p\n", ann);

	repo_state = git_repository_state(ctx->repo);
	LGitLog("! Repo state %d\n", repo_state);
	if (GIT_REPOSITORY_STATE_NONE != repo_state) {
		MessageBox(hwnd,
			"The repository is in an unknown state; another operation may be in progress.",
			"Invalid Repo State",
			MB_ICONERROR);
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	if (git_merge_analysis(&merge_analysis,
		&merge_preference,
		ctx->repo,
		&ann, 1) != 0) {
		LGitLibraryError(hwnd, "git_merge_analysis");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	LGitLog(" ! Analysis %x Preference %x\n", merge_analysis, merge_preference);
	/* XXX: Do we need to provide file scope for SccGet to make sense? */
	if (merge_analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
		/* nothing to do */
		goto fin;
	} else if (merge_analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
		/* just set our branch then checkout */
		const git_oid *target_oid = git_annotated_commit_id(ann);
		LGitMergeFastForward(ctx,
			hwnd,
			target_oid,
			merge_analysis & GIT_MERGE_ANALYSIS_UNBORN);
	} else if (merge_analysis & GIT_MERGE_ANALYSIS_NORMAL) {
		/* actually merge now */
		LGitMergeNormal(ctx, hwnd, ann, merge_preference);
	}
	/* Check for conflicts now. */
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_merge_analysis");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	if (git_index_has_conflicts(index)) {
		LGitShowMergeConflicts(ctx, hwnd, index);
	} else {
		/* XXX: if no conflicts, we can create a merge commit */
		git_repository_state_cleanup(ctx->repo);
	}
fin:
	if (index != NULL) {
		git_index_free(index);
	}
	return ret;
}

SCCRTN LGitMergeRefByName(LGitContext *ctx, HWND hwnd, const char *name)
{
	SCCRTN ret = SCC_OK;
	git_oid oid;
	git_annotated_commit *ann = NULL;
	LGitLog("**LGitMergeRefByName** Context=%p\n", ctx);
	LGitLog("  name %s\n", name);
	if (git_reference_name_to_id(&oid, ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_reference_name_to_id");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	if (git_annotated_commit_lookup(&ann, ctx->repo, &oid) != 0) {
		LGitLibraryError(hwnd, "git_annotated_commit_lookup");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	ret = LGitMerge(ctx, hwnd, ann);
fin:
	if (ann != NULL) {
		git_annotated_commit_free(ann);
	}
	return ret;
}

typedef struct _LGitMergeConflictDialogParams {
	LGitContext *ctx;
	git_index *index;
} LGitMergeConflictDialogParams;

static LVCOLUMN ancestor_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Ancestor"
};

static LVCOLUMN ours_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Ours"
};

static LVCOLUMN theirs_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Theirs"
};

static void InitConflictView(HWND hwnd, LGitMergeConflictDialogParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_MERGE_CONFLICT_LIST);

	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_LABELTIP);
	ListView_SetUnicodeFormat(lv, TRUE);
	SendMessage(lv, WM_SETFONT, (WPARAM)params->ctx->listviewFont, TRUE);

	ListView_InsertColumn(lv, 0, &ancestor_column);
	ListView_InsertColumn(lv, 1, &ours_column);
	ListView_InsertColumn(lv, 2, &theirs_column);
}

static void FillConflictView(HWND hwnd, LGitMergeConflictDialogParams* params)
{
	git_index_conflict_iterator *conflicts = NULL;
	const git_index_entry *ancestor = NULL;
	const git_index_entry *our = NULL;
	const git_index_entry *their = NULL;

	if (git_index_conflict_iterator_new(&conflicts, params->index) != 0) {
		LGitLibraryError(hwnd, "git_index_conflict_iterator_new");
		return;
	}

	HWND lv = GetDlgItem(hwnd, IDC_MERGE_CONFLICT_LIST);

	int index = 0, err = 0;
	while ((err = git_index_conflict_next(&ancestor, &our, &their, conflicts)) == 0) {
		LGitLog(" ! %d conflict: a:%s o:%s t:%s\n",
			index,
			ancestor ? ancestor->path : "NULL",
			our->path ? our->path : "NULL",
			their->path ? their->path : "NULL");
		LVITEMW lvi;
		wchar_t buf[1024];
		
		ZeroMemory(&lvi, sizeof(LVITEMW));
		lvi.mask = LVIF_TEXT;
		LGitUtf8ToWide(ancestor ? ancestor->path : "", buf, 1024);
		lvi.pszText = buf;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			continue;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		LGitUtf8ToWide(our->path ? our->path : "", buf, 1024);
		lvi.pszText = buf;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
		
		lvi.iSubItem = 2;
		LGitUtf8ToWide(their->path ? their->path : "", buf, 1024);
		lvi.pszText = buf;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
	}
	LGitLog(" ! Finished enumeration, %d file(s), rc %d\n", index, err);

	if (err != GIT_ITEROVER) {
		LGitLibraryError(hwnd, "error iterating conflicts");
	}

	git_index_conflict_iterator_free(conflicts);
}

static BOOL CALLBACK MergeConflictDialogProc(HWND hwnd,
											 unsigned int iMsg,
											 WPARAM wParam,
											 LPARAM lParam)
{
	LGitMergeConflictDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitMergeConflictDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitConflictView(hwnd, param);
		FillConflictView(hwnd, param);
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

SCCRTN LGitShowMergeConflicts(LGitContext *ctx, HWND hwnd, git_index *index)
{
	LGitLog("**LGitShowMergeConflicts** Context=%p\n", ctx);
	LGitMergeConflictDialogParams params;
	params.ctx = ctx;
	params.index = index;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_MERGE_CONFLICTS),
		hwnd,
		MergeConflictDialogProc,
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