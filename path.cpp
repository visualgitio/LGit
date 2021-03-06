#include "stdafx.h"

void LGitFreePathList(char **paths, int path_count)
{
	int i;
	for (i = 0; i < path_count; i++) {
		free(paths[i]);
	}
	free(paths);
}

/*
 * We want to transform the input path to have backslashes, since the libgit2
 * function returns a workdir path with forward slashes.
 */
LGIT_API void LGitTranslateStringChars(char *buf, int char1, int char2)
{
	size_t len = strlen(buf), i;
	for (i = 0; i < len; i++) {
		if (buf[i] == char1) {
			buf[i] = char2;
		}
	}
}

LGIT_API void LGitTranslateStringCharsW(wchar_t *buf, int char1, int char2)
{
	size_t len = wcslen(buf), i;
	for (i = 0; i < len; i++) {
		if (buf[i] == char1) {
			buf[i] = char2;
		}
	}
}

const char *LGitStripBasePath(LGitContext *ctx, const char *abs)
{
	if (abs == NULL || ctx == NULL || strlen(ctx->workdir_path) < 1) {
		return NULL;
	}
	char path[_MAX_PATH];
	/* strip trailing backslash, then accomodate if we need to bump later */
	strlcpy(path, ctx->workdir_path, _MAX_PATH);
	char *last_backslash = strrchr(path, '\\');
	if (last_backslash != NULL && last_backslash[1] == '\0') {
		last_backslash[0] = '\0';
	}
	const char *begin = strcasestr(abs, path);
	if (begin != abs) {
		return NULL;
	}
	abs += strlen(path);
	if (abs[0] == '\\') {
		abs++;
	}
	return abs;
}

const wchar_t *LGitStripBasePathW(LGitContext *ctx, const wchar_t *abs)
{
	if (abs == NULL || ctx == NULL || strlen(ctx->workdir_path) < 1) {
		return NULL;
	}
	wchar_t path[_MAX_PATH];
	/* strip trailing backslash, then accomodate if we need to bump later */
	wcslcpy(path, ctx->workdir_path_utf16, _MAX_PATH);
	wchar_t *last_backslash = wcsrchr(path, L'\\');
	if (last_backslash != NULL && last_backslash[1] == L'\0') {
		last_backslash[0] = '\0';
	}
	wchar_t *begin = wcscasestr(abs, path);
	if (begin != abs) {
		return NULL;
	}
	abs += wcslen(path);
	if (abs[0] == L'\\') {
		abs++;
	}
	return abs;
}

/**
 * Try to get the last directory component as a quick and dirty way to get the
 * project name. Expects Win32-style path with backslashes for now.
 *
 * XXX: Maybe try the URL if we fail with the path?
 */
LGIT_API BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz)
{
	char *temp_path = strdup(path);
	if (temp_path == NULL) {
		return FALSE;
	}
	int i, begin = -1;
	for (i = strlen(temp_path); i >= 0; i--) {
		if (temp_path[i] == '\\' && temp_path[i + 1] == '\0') {
			temp_path[i] = '\0';
		} else if (temp_path[i] == '\\') {
			begin = i + 1;
			break;
		}
	}
	if (begin == -1) {
		goto fin;
	}
	ZeroMemory(project, bufsz);
	strlcpy(project, temp_path + begin, bufsz);
fin:
	free(temp_path);
	return begin != -1;
}

/* Assume paths contains UTF-8 strings... */
void LGitOpenFiles(LGitContext *ctx, git_strarray *paths)
{
	for (int i = 0; i < paths->count; i++) {
		wchar_t full_path[2048], relative_path[2048];
		LGitUtf8ToWide(ctx->workdir_path, full_path, 2048);
		LGitUtf8ToWide(paths->strings[i], relative_path, 2048);
		wcslcat(full_path, relative_path, 2048);
		LGitTranslateStringCharsW(full_path, '/', '\\');

		SHELLEXECUTEINFOW info;
		ZeroMemory(&info, sizeof(SHELLEXECUTEINFO));

		info.cbSize = sizeof info;
		info.lpFile = full_path;
		info.nShow = SW_SHOW;
		info.fMask = SEE_MASK_INVOKEIDLIST;
		info.lpVerb = L"open";

		ShellExecuteExW(&info);
	}
}

/**
 * Creates a shortcut on the desktop to the repository, including the path to
 * the standalone Visual Git executable.
 */
BOOL LGitCreateShortcut(LGitContext *ctx, HWND hwnd)
{
	wchar_t shortcut_path[_MAX_PATH];
	wchar_t self_exe[_MAX_PATH], *last_slash;
	LPITEMIDLIST desktop_pidl = NULL;
	wchar_t desktop[_MAX_PATH], repo_name[128];
	char repo_nameA[128];
	HRESULT hres;
	IShellLinkW* psl = NULL;
	IPersistFile* ppf = NULL;
	LPMALLOC shell_malloc = NULL;

	if (SHGetMalloc(&shell_malloc) != NOERROR) {
		LGitLog("!! Failed to get shell malloc\n");
		goto fin;
	}

	/* get our own stuff */
	if (GetModuleFileNameW(ctx->dllInst, self_exe, _MAX_PATH) == 0) {
		LGitLog("!! Failed to get library name\n");
		goto fin;
	}
	/* this is for the DLL so replace */
	last_slash = wcsrchr(self_exe, L'\\');
	*last_slash = '\0';
	wcslcat(self_exe, L"\\VGit.exe", _MAX_PATH);

	/* get the location for this */
	if (!LGitGetProjectNameFromPath(repo_nameA, ctx->workdir_path, 128)) {
		LGitLog("!! Failed to get project name\n");
		goto fin;
	}
	LGitUtf8ToWide(repo_nameA, repo_name, 128);
	if (SHGetSpecialFolderLocation(hwnd, CSIDL_DESKTOP, &desktop_pidl) != NOERROR) {
		LGitLog("!! Failed to create desktop PIDL\n");
		goto fin;
	}
	SHGetPathFromIDListW(desktop_pidl, desktop);
	_snwprintf(shortcut_path, _MAX_PATH, L"%s\\Shortcut to %s.lnk", desktop, repo_name);

	/* shell sludge */
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
	if (FAILED(hres)) {
		LGitLog("!! Failed to create IShellLink object (%x)\n", hres);
		goto fin;
	}
	/* XXX: Do we need to escape? */
	psl->SetPath(self_exe);
	psl->SetArguments(ctx->workdir_path_utf16);
	psl->SetDescription(repo_name);
	hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 
	if (FAILED(hres)) {
		LGitLog("!! Failed to create IPersistFile object (%x)\n", hres);
		goto fin;
	}
	hres = ppf->Save(shortcut_path, TRUE);
fin:
	if (shell_malloc != NULL) {
		shell_malloc->Free(desktop_pidl);
		shell_malloc->Release();
	}
	if (ppf != NULL) {
		ppf->Release();
	}
	if (psl != NULL) {
		psl->Release();
	}
	return TRUE;
}

/* note that "file" will get autodiscovered... i hope */
SCCRTN LGitOpenNewInstance(LGitContext *ctx, const char *file, HWND hwnd)
{
	STARTUPINFOW sinfo;
	PROCESS_INFORMATION pinfo;
	ZeroMemory(&sinfo, sizeof(sinfo));
	ZeroMemory(&pinfo, sizeof(pinfo));

	wchar_t self_exe[_MAX_PATH], *last_slash;
	/* get the DLL path so we can swap it out */
	if (GetModuleFileNameW(ctx->dllInst, self_exe, _MAX_PATH) == 0) {
		LGitLog("!! Failed to get library name\n");
		return SCC_E_UNKNOWNERROR;
	}
	/* this is for the DLL so replace */
	last_slash = wcsrchr(self_exe, L'\\');
	*last_slash = '\0';
	wcslcat(self_exe, L"\\VGit.exe", _MAX_PATH);

	/* lazy way to make the args; we don't need to quote it in full! */
	wchar_t combined_argv[2048];
	wcslcpy(combined_argv, L"vgit", 2048);
	if (file != NULL) {
		/* must be discovered bc the .exe checks if dir */
		char file_utf8[1024];
		LGitAnsiToUtf8(file, file_utf8, 1024);
		git_buf discovered = {0,0};
		int rc = git_repository_discover(&discovered, file_utf8, 1, NULL);
		if (rc != 0) {
			LGitLibraryError(hwnd, "Discovering Repository Root");
			goto fin;
		}
		LGitTranslateStringChars(file_utf8, '/', '\\');

		wcslcat(combined_argv, L" ", 2048);
		wchar_t path_w[1024];
		LGitUtf8ToWide(discovered.ptr, path_w, 1024);
		wcslcat(combined_argv, path_w, 2048);
		git_buf_dispose(&discovered);
		/* XXX: append files we should highlight (requires changes in exe) */
	}
fin:
	LGitLog(" ! exe %S argv %S\n", self_exe, combined_argv);
	int ret = CreateProcessW(self_exe, combined_argv, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo);
	if (ret != 0) {
		LGitLog(" ! CreateProcess returned %d (GLE %x)\n", ret, GetLastError());
	}
	return ret != 0 ? SCC_OK : SCC_E_UNKNOWNERROR;
}