#include <windows.h>
#include <glib.h>

#include "platform.h"
#include "win32-res.h"

static int splash_argc = 0;
static char** splash_argv = 0;

socket_t make_socket(int domain, int type, int protocol)
{
	return WSASocket(domain, type, protocol, 0, 0, 0);
}

void console_init() { }
void console_cleanup() { }

static void splash_setargs(HWND hWnd)
{
	splash_argc = 4;
	splash_argv = g_new0(char *, 5);

	char buf_server[256] = {0}, buf_size[256] = {0};
	GetWindowText(GetDlgItem(hWnd, IDC_SPLASH_SERVER), buf_server, sizeof buf_server);
	GetWindowText(GetDlgItem(hWnd, IDC_SPLASH_SIZE), buf_size, sizeof buf_size);

	splash_argv[0] = "mcmap";
	splash_argv[1] = "-c";

	if (strcmp(buf_size, "nomap") == 0)
		splash_argv[2] = "-m";
	else
		splash_argv[2] = g_strdup_printf("--size=%s", buf_size);

	splash_argv[3] = g_strdup(buf_server);
	splash_argv[4] = 0;
}

static INT_PTR CALLBACK splash_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG)
	{
		/* initialize the server name control */

		SetWindowText(GetDlgItem(hWnd, IDC_SPLASH_SERVER), "example.org");

		/* initialize the screen size control */

		HWND sc = GetDlgItem(hWnd, IDC_SPLASH_SIZE);
		SendMessage(sc, CB_RESETCONTENT, 0, 0);
		SendMessage(sc, CB_ADDSTRING, 0, (LPARAM)"nomap");
		SendMessage(sc, CB_ADDSTRING, 0, (LPARAM)"512x512");
		SendMessage(sc, CB_ADDSTRING, 0, (LPARAM)"resizable");
		SetWindowText(sc, "nomap");

		/* finish up */

		SetFocus(GetDlgItem(hWnd, IDOK));

		return FALSE;
	}

	if (uMsg == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
		case IDOK:
			splash_setargs(hWnd);
			EndDialog(hWnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}

		return FALSE;
	}

	return FALSE;
}

int mcmap_main(int argc, char **argv);

int main(int argc, char **argv)
{
	if (argc <= 1)
	{
		INT_PTR ret = DialogBox(NULL, MAKEINTRESOURCE(IDD_SPLASH), NULL, splash_proc);

		if (ret <= 0)
			return 0;

		return mcmap_main(splash_argc, splash_argv);
	}

	return mcmap_main(argc, argv);
}
