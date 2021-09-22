/*
 * Manages the stage (git index). Primarily for the explorer UI.
 *
 * Some index operations are done as part of SCC commands, which also do a
 * commit after.
 */

 #include <stdafx.h>

/**
 * Can be used to add new files and update existing ones.
 *
 * "paths" are relative.
 */
SCCRTN LGitStageAddFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths, BOOL update)
{
	LGitLog("**LGitStageAddFiles** Context=%p\n");
	LGitLog("  paths count %u\n", paths->count);
	LGitLog("  update? %d\n", update);
	SCCRTN ret = SCC_OK;
	git_index *index = NULL;
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "Acquiring Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (update) {
		if (git_index_update_all(index, paths, NULL, NULL) != 0) {
			LGitLibraryError(hwnd, "Updating Stage");
			ret = SCC_E_NONSPECIFICERROR;
			goto fin;
		}
	} else {
		if (git_index_add_all(index, paths, 0, NULL, NULL) != 0) {
			LGitLibraryError(hwnd, "Adding to Stage");
			ret = SCC_E_NONSPECIFICERROR;
			goto fin;
		}
	}
	if (git_index_write(index) != 0) {
		LGitLibraryError(hwnd, "Writing Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
fin:
	if (index != NULL) {
		git_index_free(index);
	}
	return ret;
}

SCCRTN LGitStageRemoveFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths)
{
	LGitLog("**LGitStageRemoveFiles** Context=%p\n");
	LGitLog("  paths count %u\n", paths->count);
	SCCRTN ret = SCC_OK;
	git_index *index = NULL;
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "Acquiring Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_index_remove_all(index, paths, NULL, NULL) != 0) {
		LGitLibraryError(hwnd, "Removing from Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_index_write(index) != 0) {
		LGitLibraryError(hwnd, "Writing Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
fin:
	if (index != NULL) {
		git_index_free(index);
	}
	return ret;
}

/* XXX: git_reset_default */
SCCRTN LGitStageUnstageFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths)
{
	LGitLog("**LGitStageUnstageFiles** Context=%p\n");
	LGitLog("  paths count %u\n", paths->count);
	SCCRTN ret = SCC_OK;
	git_object *head_obj = NULL;
	git_reference *head_ref = NULL;
	/* If we have no HEAD, then reset_default will remove, which makes sense */
	if (git_revparse_ext(&head_obj, &head_ref, ctx->repo, "HEAD") != 0) {
		LGitLog(" ! No HEAD, unstage will remove from stage\n");
	}
	if (git_reset_default(ctx->repo, head_obj, paths) != 0) {
		LGitLibraryError(hwnd, "Writing Stage");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
fin:
	if (head_obj != NULL) {
		git_object_free(head_obj);
	}
	if (head_ref != NULL) {
		git_reference_free(head_ref);
	}
	return ret;
}

/* Here lies dragons */

#define STAGE_ADD_DIALOG_MAX_PATH (MAX_PATH * 4)

SCCRTN LGitStageAddDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitStageAddDialog** Context=%p\n", ctx);
	char *path, *stripped, common_path[2048], new_path[2048];
	BOOL relative = FALSE;
	SCCRTN ret;
	OPENFILENAME ofn;
	TCHAR fileNames[STAGE_ADD_DIALOG_MAX_PATH];
	ZeroMemory(fileNames, STAGE_ADD_DIALOG_MAX_PATH);
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lpstrFile = ctx->workdir_path;
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrTitle = "Stage Files";
	ofn.lpstrFile = fileNames;
	ofn.nMaxFile = STAGE_ADD_DIALOG_MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT;
	if (!GetOpenFileName(&ofn)) {
		return SCC_I_OPERATIONCANCELED;
	}
	/* sadly we need to alloc because  */
	git_strarray strings;
	strings.count = 0;
	char **paths = (char**)calloc(256, sizeof(char*));
	strings.strings = paths;
	if (paths == NULL) {
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	path = fileNames;
	while (*path != '\0') {
		LGitLog(" ! Staging %s\n", path);
		if (strings.count == 0 &&
			GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) {
			LGitLog(" ! First path was a directory\n");
			/* it's a directory, skip including, but get the common path */
			const char *common = LGitStripBasePath(ctx, path);
			/* if there's nothing in common then... */
			if (common == NULL) {
				LGitLog("!! No common path\n");
				ret = SCC_E_NONSPECIFICERROR;
				goto fin;
			}
			LGitLog(" ! Common path is %s\n", common);
			strlcpy(common_path, common, 2048);
			if (strlen(common) != 0) {
				/* append slash as we don't have one */
				strlcat(common_path, "/", 2048);
			}
			relative = TRUE;
			goto skip;
		}
		if (strings.count == 256) {
			break;
		}
		if (relative) {
			/* combine into a new array */
			strlcpy(new_path, common_path, 2048);
			strlcat(new_path, path, 2048);
			LGitLog(" ! New path is %s\n", new_path);
			paths[strings.count++] = strdup(new_path);
		} else {
			/* for an absolute path: we need to strip the workdir */
			stripped = (char*)LGitStripBasePath(ctx, path);
			if (stripped == NULL) {
				LGitLog("!! Couldn't get base path for %s\n", path);
				goto skip;
			}
			/* it's safe to mutate now, we own the buf */
			LGitTranslateStringChars(stripped, '\\', '/');
			LGitLog(" ! Stripped path is %s\n", stripped);
			paths[strings.count++] = strdup(stripped);
		}
skip:
		path += strlen(path) + 1;
	}
	LGitLog(" ! Total of %d file(s)\n", strings.count);
	if (strings.count == 0) {
		LGitLog("!! No paths\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
fin:
	ret = LGitStageAddFiles(ctx, hwnd, &strings, FALSE);
	LGitFreePathList(strings.strings, strings.count);
	return ret;
}