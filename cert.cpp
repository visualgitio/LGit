/*
 * Displays certificates for remote callbacks.
 */

#include "stdafx.h"

typedef struct _LGitCertDialogParams {
	LGitContext *ctx;
	const char *host;
	git_cert *cert;
} LGitCertDialogParams;

extern "C" {
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "cryptui.lib")

/* This is defined in Vista+. Check if they're defined. */
typedef struct tagCRYPTUI_VIEWCERTIFICATE_STRUCTA {
    DWORD dwSize;
    HWND hwndParent;
    DWORD dwFlags;
    LPCTSTR szTitle;
    PCCERT_CONTEXT pCertContext;
    DWORD param6;
    DWORD param7;
    DWORD param8;
    DWORD param9;
    DWORD param10;
    DWORD param11;
    DWORD param12;
    DWORD param13;
    DWORD param14;
    DWORD param15;
    DWORD param16;
    DWORD param17;
    DWORD param18;

} CRYPTUI_VIEWCERTIFICATE_STRUCTA, *PCCRYPTUI_VIEWCERTIFICATE_STRUCT;

typedef struct tagCRYPTUI_VIEWCERTIFICATE_STRUCTW {
    DWORD dwSize;
    HWND hwndParent;
    DWORD dwFlags;
    LPCWSTR szTitle;
    PCCERT_CONTEXT pCertContext;
    DWORD param6;
    DWORD param7;
    DWORD param8;
    DWORD param9;
    DWORD param10;
    DWORD param11;
    DWORD param12;
    DWORD param13;
    DWORD param14;
    DWORD param15;
    DWORD param16;
    DWORD param17;
    DWORD param18;
} CRYPTUI_VIEWCERTIFICATE_STRUCTW, *PCCRYPTUI_VIEWCERTIFICATE_STRUCTW;

WINCRYPT32API
BOOL
WINAPI
CryptUIDlgViewCertificateA(CRYPTUI_VIEWCERTIFICATE_STRUCTA* vcs, BOOL *ps);

WINCRYPT32API
BOOL
WINAPI
CryptUIDlgViewCertificateW(CRYPTUI_VIEWCERTIFICATE_STRUCTW* vcs, BOOL *ps);

#ifdef UNICODE
    #define CRYPTUI_VIEWCERTIFICATE_STRUCT     CRYPTUI_VIEWCERTIFICATE_STRUCTW
    #define PCCRYPTUI_VIEWCERTIFICATE_STRUCT    PCCRYPTUI_VIEWCERTIFICATE_STRUCTW
    #define CryptUIDlgViewCertificate CryptUIDlgViewCertificateW
#else
    #define CRYPTUI_VIEWCERTIFICATE_STRUCT     CRYPTUI_VIEWCERTIFICATE_STRUCTA
    #define PCCRYPTUI_VIEWCERTIFICATE_STRUCT    PCCRYPTUI_VIEWCERTIFICATE_STRUCTA
    #define CryptUIDlgViewCertificate CryptUIDlgViewCertificateA
#endif
}

typedef BOOL (WINAPI *DlgViewCertFunc)(CRYPTUI_VIEWCERTIFICATE_STRUCTA*,BOOL*);

/* Workaround for FX!32 */
static BOOL CryptUIDlgViewCertificateWrapper(CRYPTUI_VIEWCERTIFICATE_STRUCTA* vcs, BOOL *ps)
{
	static BOOL wrapper_init = FALSE;
	static HMODULE cryptui = NULL;
	static DlgViewCertFunc func = NULL;
	if (!wrapper_init) {
		cryptui = LoadLibraryEx("cryptui.dll", NULL, 0);
		if (cryptui == NULL) {
			LGitLog("!! Failed to load cryptui.dll (%x)", GetLastError());
			return FALSE;
		}
		func = (DlgViewCertFunc)GetProcAddress(cryptui, "CryptUIDlgViewCertificateA");
		if (func == NULL) {
			LGitLog("!! Failed to load CryptUIDlgViewCertificateA (%x)", GetLastError());
			return FALSE;
		}
		wrapper_init = TRUE;
	}
	return func(vcs, ps);
}

static void DisplayCertificate(HWND parent,
							   git_cert_x509 *x509,
							   const char *host)
{
	/* create a certificate store */
	HCERTSTORE hMemStore;
	OutputDebugString(" ! Opening cert store\n");
	hMemStore = CertOpenStore(
		CERT_STORE_PROV_MEMORY,   // the memory provider type
		0,                        // the encoding type is not needed
		NULL,                     // use the default HCRYPTPROV
		0,                        // accept the default dwFlags
		NULL                      // pvPara is not used
	);
	/* now the certificate... */
	PCCERT_CONTEXT cert_ctx;
	OutputDebugString(" ! Adding encoded cert\n");
	if (!CertAddEncodedCertificateToStore(hMemStore,
		X509_ASN_ENCODING,
		(BYTE*)x509->data,
		x509->len,
		CERT_STORE_ADD_ALWAYS,
		&cert_ctx)) {
		OutputDebugString("!! Error adding encoded cert\n");
		goto fin_close;
	}
	/* now create the dialog */
	BOOL changed;
	CRYPTUI_VIEWCERTIFICATE_STRUCT vc_st;
	ZeroMemory(&vc_st, sizeof(CRYPTUI_VIEWCERTIFICATE_STRUCT));
	vc_st.dwSize = sizeof(CRYPTUI_VIEWCERTIFICATE_STRUCT);
	vc_st.pCertContext = cert_ctx;
	vc_st.szTitle = host;
	vc_st.hwndParent = parent;
	OutputDebugString(" ! Displaying cert\n");
	if (!CryptUIDlgViewCertificateWrapper(&vc_st, &changed)) {
		OutputDebugString("!! Error displaying UI\n");
	}
	/* cleanup */
	CertFreeCertificateContext(cert_ctx);
fin_close:
	CertCloseStore(hMemStore, 0);
}

static void InitCertView(HWND hwnd, LGitCertDialogParams *params)
{
	/* Could be fancier, I guess */
	HICON warning_icon = LoadIcon(NULL, IDI_WARNING);
	SendDlgItemMessage(hwnd, IDC_CERT_PROMPT_ICON, STM_SETICON, (WPARAM)warning_icon, 0);
	if (params->cert->cert_type != GIT_CERT_X509) {
		HWND view_cert = GetDlgItem(hwnd, IDC_VIEW_CERT);
		EnableWindow(view_cert, FALSE);
	}
	char msg[512], fmt[256];
	GetDlgItemText(hwnd, IDC_CERT_MESSAGE, fmt, 256);
	_snprintf(msg, 512, fmt, params->host);
	SetDlgItemText(hwnd, IDC_CERT_MESSAGE, msg);
}

static BOOL CALLBACK CertDialogProc(HWND hwnd,
									unsigned int iMsg,
									WPARAM wParam,
									LPARAM lParam)
{
	LGitCertDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitCertDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitCertView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCertDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDC_VIEW_CERT:
			DisplayCertificate(hwnd, (git_cert_x509*)param->cert, param->host);
			return TRUE;
		case IDOK:
			EndDialog(hwnd, 2);
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

BOOL LGitCertificatePrompt(LGitContext *ctx,
						   HWND parent,
						   git_cert *cert,
						   const char *host)
{
	LGitCertDialogParams params;
	params.ctx = ctx;
	params.host = host;
	params.cert = cert;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_CERT_PROMPT),
		parent,
		CertDialogProc,
		(LPARAM)&params)) {
	default:
		LGitLog(" ! Uh-oh, dialog error\n");
	case 1:
		return FALSE;
	case 2:
		return TRUE;
	}
}