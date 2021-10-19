
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the LGIT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// LGIT_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef LGIT_EXPORTS
#define LGIT_API extern "C" __declspec(dllexport)
#else
#define LGIT_API extern "C" __declspec(dllimport)
#endif

/* Useful macros */
#define MF_IF(x) ((x) ? MF_ENABLED : MF_GRAYED)
#define MF_IF_CMD(x) (MF_BYCOMMAND | MF_IF(x))
#define CheckMenuItemIf(menu, command, cond) \
	CheckMenuItem(menu, command, MF_BYCOMMAND | ((cond) ? MF_CHECKED : MF_UNCHECKED))

/* Useful macros for simple conversions. Note -1 will copy the NUL too. */
#define LGitWideToUtf8(wide, utf8, utf8size) WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8size, NULL, NULL)
/* this is in wide codepoints */
#define LGitUtf8ToWide(utf8, wide, widesize) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, widesize)

/* XXX: Use a case-insensitive comparator */
typedef std::set<std::string> CheckoutQueue;

/* Defining here so they can be defined in ctx; used in cmdopts.cpp */
typedef struct _LGitCommitOpts {
	BOOL push;
} LGitCommitOpts;
typedef struct _LGitGetOpts {
	BOOL pull;
} LGitGetOpts;

typedef struct _LGitContext {
	/* housekeeping */
	BOOL active;
	HINSTANCE dllInst;
	/* With shared SCC subproject, so we don't free IDE provided dir string */
	BOOL addSccSuccess;
	/* git state */
	git_repository *repo;
	/* used for faking checkout status */
	CheckoutQueue *checkouts;
	/* callbacks and such provided by IDE */
	OPTNAMECHANGEPFN renameCb;
	LPVOID renameData;
	LPTEXTOUTPROC textoutCb;
	/* Progress dialog, used and destroyed on demand */
	IProgressDialog *progress;
	BOOL progressCancelled;
	/* big in case of Windows 10. keep a wide copy in case */
	char path[1024], workdir_path[1024];
	/* path isn't really used right now */
	wchar_t workdir_path_utf16[1024];
	char appName[SCC_NAME_SIZE];
	/* SCC provided username, used for some remote contexts */
	char username[SCC_USER_SIZE];
	/* Command options */
	LGitCommitOpts commitOpts;
	LGitGetOpts getOpts;
	/* Fonts */
	HFONT listviewFont, fixedFont;
} LGitContext;

/* LGit.cpp */
const char* LGitCommandName(enum SCCCOMMAND command);

/* caps.cpp */
LONG LGitGetCaps(void);

/* logging.cpp */
void LGitLog(const char *format_str, ...);
void LGitLibraryError(HWND hWnd, LPCSTR title);

/* path.cpp */
void LGitFreePathList(char **paths, int path_count);
LGIT_API void LGitTranslateStringChars(char *buf, int char1, int char2);
LGIT_API void LGitTranslateStringCharsW(wchar_t *buf, int char1, int char2);
const char *LGitStripBasePath(LGitContext *ctx, const char *abs);
const wchar_t *LGitStripBasePathW(LGitContext *ctx, const wchar_t *abs);
LGIT_API BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz);
void LGitOpenFiles(LGitContext *ctx, git_strarray *paths);
BOOL LGitCreateShortcut(LGitContext *ctx, HWND hwnd);

/* format.cpp */
char *LGitWideToUtf8Alloc(wchar_t *buf);
wchar_t *LGitUtf8ToWideAlloc(char *buf);
BOOL LGitTimeToString(const git_time *time, char *buf, size_t bufsz);
int LGitFormatSignature(const git_signature *sig, char *buf, size_t bufsz);
BOOL LGitTimeToStringW(const git_time *time, wchar_t *buf, size_t bufsz);
int LGitFormatSignatureW(const git_signature *sig, wchar_t *buf, size_t bufsz);
int LGitFormatSignatureWithTimeW(const git_signature *sig, wchar_t *buf, size_t bufsz);
UINT LGitGitToWindowsCodepage(const char *encoding);
const char *LGitRepoStateString(int state);
const char *LGitBranchType(git_branch_t type);
void LGitLfToCrLf(char *crlf, const char *lf, size_t crlf_len);
void LGitSetWindowTextFromCommitMessage(HWND ctrl, UINT codepage, const char *message);

/* checkout.cpp */
SCCRTN LGitCheckoutStaged(LGitContext *ctx, HWND hwnd, git_strarray *paths);
SCCRTN LGitCheckoutHead(LGitContext *ctx, HWND hwnd, git_strarray *paths);
SCCRTN LGitCheckoutRef(LGitContext *ctx, HWND hwnd, git_reference *branch);
SCCRTN LGitCheckoutRefByName(LGitContext *ctx,  HWND hwnd, const char *name);
SCCRTN LGitCheckoutTree(LGitContext *ctx, HWND hwnd, const git_oid *commit_oid);
void LGitPushCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitPopCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitIsCheckout(LGitContext *ctx, const char *fileName);

/* commit.cpp */
SCCRTN LGitCommitIndex(HWND hWnd, LGitContext *ctx, git_index *index, LPCSTR lpComment, git_signature *author, git_signature *committer);
SCCRTN LGitCommitIndexAmendHead(HWND hWnd, LGitContext *ctx, git_index *index, LPCSTR lpComment, git_signature *author, git_signature *committer);

/* commitmk.cpp */
SCCRTN LGitCreateCommitDialog(LGitContext *ctx, HWND hwnd, BOOL amend_last, const char *proposed_message, git_index *proposed_index);

/* status.cpp */
SCCRTN LGitFileProperties(LGitContext *ctx, HWND hWnd, LPCSTR relative_path);

/* revert.cpp */
SCCRTN LGitRevertCommit(LGitContext *ctx, HWND hwnd, const git_oid *commit_oid);
SCCRTN LGitResetToCommit(LGitContext *ctx, HWND hwnd, const git_oid *commit_oid, BOOL hard);

/* history.cpp */
SCCRTN LGitHistoryForRefByName(LPVOID context, HWND hWnd, const char *ref);
SCCRTN LGitHistory(LPVOID context, HWND hWnd, git_strarray *paths);

/* pushpull.cpp */
typedef enum _LGitPullStrategy {
	LGPS_FETCH = 0,
	LGPS_MERGE_TO_HEAD = 1,
} LGitPullStrategy;

SCCRTN LGitPush(LGitContext *ctx, HWND hwnd, git_remote *remote, git_reference *refname);
SCCRTN LGitPushDialog(LGitContext *ctx, HWND hwnd);
SCCRTN LGitPull(LGitContext *ctx, HWND hwnd, git_remote *remote, LGitPullStrategy strategy);
SCCRTN LGitPullDialog(LGitContext *ctx, HWND hwnd);

/* merge.cpp */
SCCRTN LGitMergeFastForward(LGitContext *ctx, HWND hwnd, const git_oid *target_oid, BOOL is_unborn);
SCCRTN LGitMergeNormal(LGitContext *ctx, HWND hwnd, const git_annotated_commit *ac, git_merge_preference_t preference);
SCCRTN LGitMerge(LGitContext *ctx, HWND hwnd, const git_annotated_commit *ann);
SCCRTN LGitMergeRefByName(LGitContext *ctx, HWND hwnd, const char *name);
SCCRTN LGitShowMergeConflicts(LGitContext *ctx, HWND hwnd, git_index *index);

/* clone.cpp */
LGIT_API SCCRTN LGitClone(LGitContext *ctx, HWND hWnd, LPSTR lpProjName, LPSTR lpLocalPath, LPBOOL pbNew);

/* stage.cpp */
SCCRTN LGitStageAddFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths, BOOL update);
SCCRTN LGitStageRemoveFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths);
SCCRTN LGitStageUnstageFiles(LGitContext *ctx, HWND hwnd, git_strarray *paths);
SCCRTN LGitStageAddDialog(LGitContext *ctx, HWND hwnd);
SCCRTN LGitStageDragTarget(LGitContext *ctx, HWND hwnd, HDROP drop);

/* tag.cpp */
SCCRTN LGitAddTagDialog(LGitContext *ctx, HWND hwnd);

/* diffwin.cpp */
typedef struct _LGitDiffDialogParams {
	LGitContext *ctx;
	git_diff *diff;
	git_commit *commit;
	/* Only likely relevant for single-file SccDiff */
	const char *path;
	/* Internal done by LGitDiffWindow */
	HMENU menu;
} LGitDiffDialogParams;

int LGitDiffWindow(HWND parent, LGitDiffDialogParams *params);

/* diff.cpp */
SCCRTN LGitCommitToCommitDiff(LGitContext *ctx, HWND hwnd, git_commit *commit_b, git_commit *commit_a, git_diff_options *diffopts);
SCCRTN LGitCommitToParentDiff(LGitContext *ctx, HWND hwnd, git_commit *commit, git_diff_options *diffopts);
SCCRTN LGitDiffStageToWorkdir(LGitContext *ctx, HWND hwnd, git_strarray *paths);
SCCRTN LGitDiffTreeToWorkdir(LGitContext *ctx, HWND hwnd, git_strarray *paths, git_tree *tree);

/* apply.cpp */
SCCRTN LGitApplyPatch(LGitContext *ctx, HWND hwnd, git_diff *diff, git_apply_location_t loc, BOOL check_only);
SCCRTN LGitFileToDiff(LGitContext *ctx, HWND hwnd, const wchar_t *file, git_diff **out);
SCCRTN LGitApplyPatchDialog(LGitContext *ctx, HWND hwnd);

/* sigwin.cpp */
SCCRTN LGitSignatureDialog(LGitContext *ctx, HWND parent, char *name,  size_t name_sz, char *mail, size_t mail_sz, BOOL enable_set_default);
SCCRTN LGitGetDefaultSignature(HWND hWnd, LGitContext *ctx, git_signature **signature);

/* commitvw.cpp */
void LGitViewCommitInfo(LGitContext *ctx, HWND hWnd, git_commit *commit, git_tag *tag);

/* gitconf.cpp */
SCCRTN LGitManageConfig(LGitContext *ctx, HWND hwnd, git_config *config, const char *title);

/* progress.cpp */
BOOL LGitProgressInit(LGitContext *ctx, const char *title, UINT anim);
BOOL LGitProgressStart(LGitContext *ctx, HWND parent, BOOL quantifiable);
BOOL LGitProgressDeinit(LGitContext *ctx);
BOOL LGitProgressSet(LGitContext *ctx, ULONGLONG x, ULONGLONG outof);
BOOL LGitProgressText(LGitContext *ctx, const char *text, int line);
BOOL LGitProgressCancelled(LGitContext *ctx);
void LGitInitCheckoutProgressCallback(LGitContext *ctx, git_checkout_options *co_opts);
void LGitInitDiffProgressCallback(LGitContext *ctx, git_diff_options *diff_opts);

/* branch.cpp */
SCCRTN LGitShowBranchManager(LGitContext *ctx, HWND hwnd);

/* remote.cpp */
SCCRTN LGitShowRemoteManager(LGitContext *ctx, HWND hwnd);

/* remotecb.cpp */
void LGitInitRemoteCallbacks(LGitContext *ctx, HWND hWnd, git_remote_callbacks *cb);

/* revparse.cpp */
SCCRTN LGitRevparseDialog(LGitContext *ctx, HWND hwnd, const char *title, const char *suggested_spec, git_object **obj, git_reference **ref);
SCCRTN LGitRevparseDialogString(LGitContext *ctx, HWND hwnd, const char *title, char *spec, size_t bufsz);

/* cert.cpp */
BOOL LGitCertificatePrompt(LGitContext *ctx, HWND parent, git_cert *cert, const char *host);

/* string.cpp */
char *strcasestr(const char *s, const char *find);
wchar_t *wcscasestr(const wchar_t *s, const wchar_t *find);
size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t dsize);
size_t wcslcat(wchar_t *dst, const wchar_t *src, size_t dsize);

/* winutil.cpp */
void LGitPopulateRemoteComboBox(HWND parent, HWND cb, LGitContext *ctx);
void LGitPopulateReferenceComboBox(HWND parent, HWND cb, LGitContext *ctx);
BOOL LGitBrowseForFolder(HWND hwnd, const wchar_t *title, wchar_t *buf, size_t bufsz);
void LGitSetWindowIcon(HWND hwnd, HINSTANCE inst, LPCSTR name);
void LGitUninitializeFonts(LGitContext *ctx);
void LGitInitializeFonts(LGitContext *ctx);
void LGitSetMonospaceFont(LGitContext *ctx, HWND ctrl);
LONG LGitMeasureWidth(HWND measure_with, const char *text);
LONG LGitMeasureWidthW(HWND measure_with, const wchar_t *text);
BOOL CALLBACK LGitImmutablePropSheetProc(HWND hwnd, unsigned int iMsg, LPARAM lParam);
BOOL LGitContextMenuFromSubmenu(HWND hwnd, HMENU menu, int position, int x, int y);
void LGitControlFillsParentDialog(HWND hwnd, UINT dlg_item);
void LGitControlFillsParentDialogCarveout(HWND hwnd, UINT dlg_item, RECT *bounds);
HIMAGELIST LGitGetSystemImageList();

/* about.cpp */
void LGitAbout(HWND hwnd, LGitContext *ctx);

/* runscc.cpp */
LGIT_API SCCRTN LGitStandaloneExplorer(LGitContext *ctx, LONG nFiles, LPCSTR* lpFileNames);
