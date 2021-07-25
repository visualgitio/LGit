
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

typedef struct _LGitContext {
	/* housekeeping */
	BOOL active;
	int refcount;
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
	/* big in case of Windows 10 */
	char path[1024], workdir_path[1024];
	char appName[SCC_NAME_SIZE];
	/* SCC provided username, used for some remote contexts */
	char username[SCC_USER_SIZE];
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
BOOL LGitTimeToString(const git_time *time, char *buf, int bufsz);
int LGitFormatSignature(const git_signature *sig, char *buf, int bufsz);

/* checkout.cpp */
void LGitPushCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitPopCheckout(LGitContext *ctx, const char *fileName);
BOOL LGitIsCheckout(LGitContext *ctx, const char *fileName);

/* clone.cpp */
SCCRTN LGitClone(LGitContext *ctx, HWND hWnd, LPSTR lpProjName, LPSTR lpLocalPath, LPBOOL pbNew);

/* diffwin.cpp */
typedef struct _LGitDiffDialogParams {
	LGitContext *ctx;
	git_diff *diff;

	/* Only likely relevant for single-file SccDiff */
	const char *path;
} LGitDiffDialogParams;

int LGitDiffWindow(HWND parent, LGitDiffDialogParams *params);

/* remotecb.cpp */
void LGitInitRemoteCallbacks(LGitContext *ctx, HWND hWnd, git_remote_callbacks *cb);

/* cert.cpp */
BOOL LGitCertificatePrompt(LGitContext *ctx, HWND parent, git_cert *cert, const char *host);

/* string.cpp */
char *strcasestr(const char *s, const char *find);
size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);
