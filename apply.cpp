/*
 * Apply patches, and the user interface to do so.
 */

#include <stdafx.h>

SCCRTN LGitApplyPatch(LGitContext *ctx,
					  HWND hwnd,
					  git_diff *diff,
					  git_apply_location_t loc,
					  BOOL check_only)
{
	LGitLog("**LGitApplyPatchDialog** Context=%p\n", ctx);
	LGitLog("  location %x\n", loc);
	LGitLog("  check only? %d\n", check_only);
	SCCRTN ret = SCC_OK;
	git_apply_options opts;
	git_apply_options_init(&opts, GIT_APPLY_OPTIONS_VERSION);
	if (check_only) { 
		opts.flags = GIT_APPLY_CHECK;
	}
	if (git_apply(ctx->repo, diff, loc, &opts) != 0) {
		LGitLibraryError(hwnd, "git_apply");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
fin:
	return ret;
}

SCCRTN LGitFileToDiff(LGitContext *ctx, HWND hwnd, const char *file, git_diff **out)
{
	SCCRTN ret = SCC_OK;
	size_t buf_sz;
	HANDLE fh = INVALID_HANDLE_VALUE, mh = INVALID_HANDLE_VALUE;
	char *buf;
	/* we're boned if we need to allocate more than (uint32)-1 on x86 */
	fh = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		/* not an lg2 error */
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	buf_sz = GetFileSize(fh, NULL);
	if (buf_sz == 0xFFFFFFFF) {
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	/* just get a memory mapping of the file, no need for a read */
	mh = CreateFileMapping(fh, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mh == INVALID_HANDLE_VALUE) {
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	buf = (char*)MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0);
	if (buf == NULL) {
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	if (git_diff_from_buffer(out, buf, buf_sz) != 0) {
		LGitLibraryError(hwnd, "git_diff_from_buffer");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
fin:
	if (mh != INVALID_HANDLE_VALUE) {
		CloseHandle(fh);
	}
	if (fh != INVALID_HANDLE_VALUE) {
		CloseHandle(fh);
	}
	return ret;
}

SCCRTN LGitApplyPatchDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitApplyPatchDialog** Context=%p\n", ctx);
	OPENFILENAME ofn;
	TCHAR fileName[MAX_PATH];
	ZeroMemory(fileName, MAX_PATH);
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrTitle = "Apply Patch";
	ofn.lpstrDefExt = "diff";
	ofn.lpstrFilter = "Diff\0*.diff;*.patch\0";
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
	/* XXX: Add some stuff to the dialog for i.e. A&M, apply to, etc. */
	if (GetOpenFileName(&ofn)) {
		/* XXX: turn into loop, break into sep func */
		git_diff *diff = NULL;
		SCCRTN ret = SCC_OK;
		if (LGitFileToDiff(ctx, hwnd, fileName, &diff) != SCC_OK) {
			ret = SCC_E_UNKNOWNERROR;
			goto fin;
		}
		/* For now, we just apply to the working tree */
		if (LGitApplyPatch(ctx, hwnd, diff, GIT_APPLY_LOCATION_WORKDIR, FALSE) != SCC_OK) {
			ret = SCC_E_UNKNOWNERROR;
			goto fin;
		}
fin:
		if (diff != NULL) {
			git_diff_free(diff);
		}
		return ret;
	} else {
		DWORD err = CommDlgExtendedError();
		if (err) {
			LGitLog("!! OFN returned error %x\n", err);
		}
		return err ? SCC_E_UNKNOWNERROR : SCC_I_OPERATIONCANCELED;
	}
}