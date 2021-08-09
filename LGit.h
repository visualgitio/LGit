
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the LGIT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// LGIT_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef LGIT_EXPORTS
#define LGIT_API __declspec(dllexport)
#else
#define LGIT_API __declspec(dllimport)
#endif

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
	/* big in case of Windows 10 */
	char path[1024], workdir_path[1024];
	char appName[SCC_NAME_SIZE];
	/* SCC provided username, used for some remote contexts */
	char username[SCC_USER_SIZE];
	/* Command options */
	LGitCommitOpts commitOpts;
	LGitGetOpts getOpts;
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
void LGitTranslateStringChars(char *buf, int char1, int char2);
const char *LGitStripBasePath(LGitContext *ctx, const char *abs);
BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz);

/* format.cpp */
BOOL LGitTimeToString(const git_time *time, char *buf, size_t bufsz);
int LGitFormatSignature(const git_signature *sig, char *buf, size_t bufsz);
BOOL LGitTimeToStringW(const git_time *time, wchar_t *buf, size_t bufsz);
int LGitFormatSignatureW(const git_signature *sig, wchar_t *buf, size_t bufsz);
UINT LGitGitToWindowsCodepage(const char *encoding);

/* checkout.cpp */
void LGitPushCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitPopCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitIsCheckout(LGitContext *ctx, const char *fileName);

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
SCCRTN LGitMergeNormal(LGitContext *ctx, HWND hwnd, git_annotated_commit *ac, git_merge_preference_t preference);
SCCRTN LGitShowMergeConflicts(LGitContext *ctx, HWND hwnd, git_index *index);

/* clone.cpp */
SCCRTN LGitClone(LGitContext *ctx, HWND hWnd, LPSTR lpProjName, LPSTR lpLocalPath, LPBOOL pbNew);

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

/* sigwin.cpp */
BOOL LGitSignatureDialog(LGitContext *ctx, HWND parent, char *name,  size_t name_sz, char *mail, size_t mail_sz);

/* commitvw.cpp */
void LGitViewCommitInfo(LGitContext *ctx, HWND hWnd, git_commit *commit);

/* progress.cpp */
BOOL LGitProgressInit(LGitContext *ctx, const char *title, UINT anim);
BOOL LGitProgressStart(LGitContext *ctx, HWND parent, BOOL quantifiable);
BOOL LGitProgressDeinit(LGitContext *ctx);
BOOL LGitProgressSet(LGitContext *ctx, ULONGLONG x, ULONGLONG outof);
BOOL LGitProgressText(LGitContext *ctx, const char *text, int line);
BOOL LGitProgressCancelled(LGitContext *ctx);
void LGitInitCheckoutProgressCallback(LGitContext *ctx, git_checkout_options *co_opts);

/* remote.cpp */
SCCRTN LGitShowRemoteManager(LGitContext *ctx, HWND hwnd);

/* remotecb.cpp */
void LGitInitRemoteCallbacks(LGitContext *ctx, HWND hWnd, git_remote_callbacks *cb);

/* cert.cpp */
BOOL LGitCertificatePrompt(LGitContext *ctx, HWND parent, git_cert *cert, const char *host);

/* string.cpp */
char *strcasestr(const char *s, const char *find);
size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t dsize);
size_t wcslcat(wchar_t *dst, const wchar_t *src, size_t dsize);

/* winutil.cpp */
void LGitPopulateRemoteComboBox(HWND parent, HWND cb, LGitContext *ctx);
BOOL LGitBrowseForFolder(HWND hwnd, const char *title, char *buf, size_t bufsz);
void LGitSetWindowIcon(HWND hwnd, HINSTANCE inst, LPCSTR name);