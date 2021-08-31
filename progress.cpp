/*
 * Progress tracking. The way we handle progrss tracking is that it's a
 * singleton of the context. There's no sense having multiple progress bars.
 *
 * A function should:
 * - Init the progress, set up descriptions
 * - Set up callbacks that move the progress bar forward (i.e. clone, fetch;
 *   all should reuse the same progress bar)
 * - Start the progress bar with a parent window
 * - Once the function is done, deinit
 *
 * It's worth noting that even if the progress dialog couldn't be made, it can
 * still be useful for i.e. textout procedures.
 */

#include "stdafx.h"

BOOL LGitProgressInit(LGitContext *ctx, const char *title, UINT anim)
{
	if (ctx == NULL || ctx->progress != NULL) {
		return FALSE;
	}
	IProgressDialog *pd = NULL;
	HRESULT ret = CoCreateInstance(CLSID_ProgressDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IProgressDialog,
		(void**)&pd);
	if (ret != S_OK || pd == NULL) {
		if (ctx != NULL && ctx->textoutCb != NULL) {
			ctx->textoutCb("(Finished)", SCC_MSG_STOPCANCEL);
			return TRUE;
		} else {
			return FALSE;
		}
	}
	ctx->progress = pd;
	/* This API is wide */
	wchar_t msg[512];
	ZeroMemory(msg, 512);
	if (MultiByteToWideChar(CP_UTF8, 0, title, -1, msg, 512) == 0) {
		return FALSE;
	}
	ctx->progress->SetTitle(msg);
	if (anim != 0) {
		ctx->progress->SetAnimation(ctx->dllInst, anim);
	}
	/* XXX: Hardcoded */
	ctx->progress->SetCancelMsg(L"Please wait...", NULL);
	return TRUE;
}

BOOL LGitProgressStart(LGitContext *ctx, HWND parent, BOOL quantifiable)
{
	if (ctx == NULL || ctx->progress == NULL) {
		return FALSE;
	}
	/* XXX: Does hardcoding these flags make sense? */
	DWORD flags = PROGDLG_NOMINIMIZE | PROGDLG_NOMINIMIZE | PROGDLG_NOTIME;
	if (!quantifiable) {
#if _WIN32_WINNT >= 0x0600
		flags |= PROGDLG_MARQUEEPROGRESS;
#else
		flags |= PROGDLG_NOPROGRESSBAR;
#endif
	}
	ctx->progress->StartProgressDialog(parent, NULL,
		flags,
		NULL);
	return TRUE;
}

BOOL LGitProgressDeinit(LGitContext *ctx)
{
	ctx->progressCancelled = FALSE;
	if (ctx == NULL || ctx->progress == NULL) {
		if (ctx != NULL && ctx->textoutCb != NULL) {
			ctx->textoutCb("(Finished)", SCC_MSG_STOPCANCEL);
			return TRUE;
		} else {
			return FALSE;
		}
	}
	/* Once we stop we no longer need the dialog anymore, so combine free */
	ctx->progress->StopProgressDialog();
	ctx->progress->Release();
	ctx->progress = NULL;
	return TRUE;

}

BOOL LGitProgressSet(LGitContext *ctx, ULONGLONG x, ULONGLONG outof)
{
	if (ctx == NULL || ctx->progress == NULL) {
		return FALSE;
	}
	ctx->progress->SetProgress64(x, outof);
	return TRUE;
}

/* XXX: Unicode version? */
BOOL LGitProgressText(LGitContext *ctx, const char *text, int line)
{
	if (ctx == NULL || ctx->progress == NULL) {
		if (ctx != NULL && ctx->textoutCb != NULL) {
			/* XXX: We could display it with a cancellation message */
			if (ctx->textoutCb(text, SCC_MSG_STATUS) == SCC_MSG_RTN_CANCEL) {
				ctx->progressCancelled = TRUE;
			}
			return TRUE;
		} else {
			return FALSE;
		}
	}
	wchar_t msg[512];
	ZeroMemory(msg, 512);
	if (MultiByteToWideChar(CP_UTF8, 0, text, -1, msg, 512) == 0) {
		return FALSE;
	}
	ctx->progress->SetLine(line, msg, FALSE, NULL);
	return TRUE;
}

BOOL LGitProgressCancelled(LGitContext *ctx)
{
	if (ctx->progressCancelled) {
		return TRUE;
	}
	if (ctx == NULL || ctx->progress == NULL) {
		return FALSE;
	}
	return ctx->progress->HasUserCancelled();
}

static void CheckoutProgress(const char *path, size_t current, size_t total, void *payload)
{
	if (total == 0) {
		/* we were called with nothing */
		return;
	}
	LGitContext *ctx = (LGitContext*)payload;
	/* No cancellations */
	char msg[256];
	if (path != NULL ) {
		_snprintf(msg, 256, "Checking out '%s' %u/%u", path, current, total);
	} else {
		_snprintf(msg, 256, "Checking out %u/%u", current, total);
	}
	LGitProgressText(ctx, msg, 2);
	LGitProgressSet(ctx, current, total);
}

void LGitInitCheckoutProgressCallback(LGitContext *ctx, git_checkout_options *co_opts)
{
	/* Thankfully, there's no need for the caller to free payload. */
	co_opts->progress_cb = CheckoutProgress;
	co_opts->progress_payload = ctx;
}

static int DiffProgress(const git_diff *diff_so_far, const char *old_path, const char *new_path, void *payload)
{
	LGitContext *ctx = (LGitContext*)payload;
	if (LGitProgressCancelled(ctx)) {
		return GIT_EUSER;
	}
	char msg[256];
	/* first, how many deltas we have; we won't know how many for total */
	size_t num_deltas = git_diff_num_deltas(diff_so_far);
	_snprintf(msg, 256, "%u delta(s)", num_deltas);
	LGitProgressText(ctx, msg, 1);
	/* now for the files being compared */
	if (old_path != NULL && new_path != NULL && strcmp(old_path, new_path) != 0) {
		_snprintf(msg, 256, "Comparing '%s' to  '%s'", old_path, new_path);
	} else if (old_path != NULL) {
		_snprintf(msg, 256, "Comparing '%s'", old_path);
	} else if (new_path != NULL) {
		_snprintf(msg, 256, "Comparing '%s'", new_path);
	} else {
		strlcpy(msg, "Comparing unknown files", 256);
	}
	LGitProgressText(ctx, msg, 2);
	return 0;
}

void LGitInitDiffProgressCallback(LGitContext *ctx, git_diff_options *diff_opts)
{
	/* Thankfully, there's no need for the caller to free payload. */
	diff_opts->progress_cb = DiffProgress;
	/* XXX: notify CB */
	diff_opts->payload = ctx;
}