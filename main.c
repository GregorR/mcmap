#include <stdarg.h>
#include <stdio.h>

#include <gio/gio.h>
#include <SDL.h>

#include "cmd.h"
#include "common.h"
#include "map.h"
#include "protocol.h"
#include "world.h"

/* default command-line options */

struct
{
	gint localport;
	gboolean noansi;
	gint scale;
	gchar *wndsize;
} opt = {
	.localport = 25565,
	.noansi = FALSE,
	.scale = 1,
	.wndsize = 0,
};

/* miscellaneous helper routines */

static void handle_key(SDL_KeyboardEvent *e, int *repaint);
static void handle_mouse(SDL_MouseButtonEvent *e, SDL_Surface *screen);
static void handle_chat(unsigned char *msg, int msglen);

/* proxying thread function to pass packets */

struct proxy_config
{
	GSocket *sock_from, *sock_to;
	int client_to_server;
	GAsyncQueue *q;
	GAsyncQueue *iq;
};

static GAsyncQueue *iq_client = 0;
static GAsyncQueue *iq_server = 0;

gpointer proxy_thread(gpointer data)
{
	struct proxy_config *cfg = data;
	GSocket *sfrom = cfg->sock_from, *sto = cfg->sock_to;
	char *desc = cfg->client_to_server ? "client -> server" : "server -> client";

	packet_state_t state = PACKET_STATE_INIT(cfg->client_to_server ? PACKET_TO_SERVER : PACKET_TO_CLIENT);

	while (1)
	{
		/* read in one packet */

		packet_t *p = packet_read(sfrom, &state);
		if (!p)
		{
			SDL_Event e = { .type = SDL_QUIT };
			SDL_PushEvent(&e);
			return 0;
		}

		/* either write it out or handle if it's a command to us */

		if (cfg->client_to_server
		    && p->id == PACKET_CHAT
		    && p->bytes[3] == '/' && p->bytes[4] == '/')
		{
			g_async_queue_push(cfg->q, packet_dup(p));
		}
		else
		{
			if (!packet_write(sto, p))
				dief("proxy thread (%s) write failed", desc);
		}

		/* write pending injected packets */

		packet_t *ip;
		while ((ip = g_async_queue_try_pop(cfg->iq)) != 0)
		{
			if (!packet_write(sto, ip))
				dief("proxy thread (%s) inject failed", desc);
			packet_free(ip);
		}

		/* communicate interesting chunks back */

		if (!world_running)
			continue;

		switch (p->id)
		{
		case PACKET_LOGIN:
		case PACKET_CHUNK:
		case PACKET_MULTI_SET_BLOCK:
		case PACKET_SET_BLOCK:
		case PACKET_PLAYER_MOVE:
		case PACKET_PLAYER_ROTATE:
		case PACKET_PLAYER_MOVE_ROTATE:
		case PACKET_ENTITY_SPAWN_NAMED:
		case PACKET_ENTITY_SPAWN_OBJECT:
		case PACKET_ENTITY_DESTROY:
		case PACKET_ENTITY_REL_MOVE:
		case PACKET_ENTITY_REL_MOVE_LOOK:
		case PACKET_ENTITY_MOVE:
		case PACKET_ENTITY_ATTACH:
			g_async_queue_push(cfg->q, packet_dup(p));
			break;

		case PACKET_CHAT:
			if (!cfg->client_to_server)
			{
				int msglen;
				unsigned char *msg = packet_string(p, 0, &msglen);
				handle_chat(msg, msglen);
			}
			break;
		}
	}
}

/* main application */

int main(int argc, char **argv)
{
	/* command line option grokking */

	static GOptionEntry gopt_entries[] = {
		{ "nocolor", 'c', 0, G_OPTION_ARG_NONE, &opt.noansi, "Disable ANSI color escapes" },
		{ "port", 'p', 0, G_OPTION_ARG_INT, &opt.localport, "Local port to listen at", "P" },
		{ "size", 's', 0, G_OPTION_ARG_STRING, &opt.wndsize, "Fixed-size window size", "WxH" },
		{ "scale", 'x', 0, G_OPTION_ARG_INT, &opt.scale, "Zoom factor", "N" },
		{ NULL }
	};

	GOptionContext *gopt = g_option_context_new("host[:port]");
	GError *gopt_error = 0;

	g_option_context_add_main_entries(gopt, gopt_entries, 0);
	if (!g_option_context_parse(gopt, &argc, &argv, &gopt_error))
	{
		die(gopt_error->message);
	}

	if (argc != 2)
	{
		gchar *usage = g_option_context_get_help(gopt, TRUE, 0);
		fputs(usage, stderr);
		return 1;
	}

	if (opt.localport < 1 || opt.localport > 65535)
	{
		dief("Invalid port number: %d", opt.localport);
	}

	if (opt.scale < 1 || opt.scale > 64)
	{
		dief("Unreasonable scale factor: %d", opt.scale);
	}

	int wnd_w = 512, wnd_h = 512;

	if (opt.wndsize)
	{
		if (sscanf(opt.wndsize, "%dx%d", &wnd_w, &wnd_h) != 2
		    || wnd_w < 0 || wnd_h < 0)
		{
			dief("Invalid window size: %s", opt.wndsize);
		}
	}

	/* initialization stuff */

	g_thread_init(0);
	g_type_init();

	iq_client = g_async_queue_new_full(packet_free);
	iq_server = g_async_queue_new_full(packet_free);

	/* build up the world model */

	world_init();

	GAsyncQueue *packetq = g_async_queue_new_full(packet_free);
	g_thread_create(world_thread, packetq, FALSE, 0);

	/* wait for a client to connect to us */

	log_print("[INFO] Waiting for connection...");

	GSocketListener *listener = g_socket_listener_new();

	if (!g_socket_listener_add_inet_port(listener, opt.localport, 0, 0))
	{
		die("Unable to set up sockets.");
		return 1;
	}

	GSocketConnection *conn_cli = g_socket_listener_accept(listener, 0, 0, 0);

	if (!conn_cli)
	{
		die("Client never connected.");
		return 1;
	}

	/* connect to the minecraft server side */

	log_print("[INFO] Connecting to %s...", argv[1]);

	GSocketClient *client = g_socket_client_new();

	GSocketConnection *conn_srv = g_socket_client_connect_to_host(client, argv[1], 25565, 0, 0);

	if (!conn_srv)
	{
		die("Unable to connect to server.");
		return 1;
	}

	/* start the proxying threads */

	log_print("[INFO] Starting up...");

	GSocket *sock_cli = g_socket_connection_get_socket(conn_cli);
	GSocket *sock_srv = g_socket_connection_get_socket(conn_srv);

	struct proxy_config proxy_client_server = {
		.sock_from = sock_cli,
		.sock_to = sock_srv,
		.client_to_server = 1,
		.q = packetq,
		.iq = iq_server
	};

	struct proxy_config proxy_server_client = {
		.sock_from = sock_srv,
		.sock_to = sock_cli,
		.client_to_server = 0,
		.q = packetq,
		.iq = iq_client
	};

	g_thread_create(proxy_thread, &proxy_client_server, FALSE, 0);
	g_thread_create(proxy_thread, &proxy_server_client, FALSE, 0);

	/* start the user interface side */

	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0)
	{
		die("Failed to initialize SDL.");
		return 1;
	}

	SDL_Surface *screen = SDL_SetVideoMode(wnd_w, wnd_h, 32, SDL_SWSURFACE|(opt.wndsize ? 0 : SDL_RESIZABLE));
	if (!screen)
	{
		dief("Failed to set video mode: %s", SDL_GetError());
		return 1;
	}

	SDL_WM_SetCaption("mcmap", "mcmap");
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	map_init(screen);
	map_setscale(opt.scale, 0);

	while (1)
	{
		int repaint = 0;

		/* process pending events, coalesce repaints */

		SDL_Event e;

		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				SDL_Quit();
				return 0;

			case SDL_KEYDOWN:
				handle_key(&e.key, &repaint);
				break;

			case SDL_MOUSEBUTTONDOWN:
				handle_mouse(&e.button, screen);
				break;

			case MCMAP_EVENT_REPAINT:
				repaint = 1;
				break;
			}
		}

		/* repaint dirty bits if necessary */

		if (repaint)
			map_draw(screen);

		/* wait for something interesting to happen */

		SDL_WaitEvent(0);
	}
}

/* helper routine implementations */

static void handle_key(SDL_KeyboardEvent *e, int *repaint)
{
	switch (e->keysym.sym)
	{
	case SDLK_1:
		map_setmode(MAP_MODE_SURFACE, 0);
		*repaint = 1;
		break;

	case SDLK_2:
		map_setmode(MAP_MODE_CROSS, MAP_FLAG_FOLLOW_Y);
		*repaint = 1;
		break;

	case SDLK_3:
		map_setmode(MAP_MODE_CROSS, 0);
		*repaint = 1;
		break;

	case SDLK_4:
		map_setmode(MAP_MODE_TOPO, 0);
		*repaint = 1;
		break;

	case SDLK_UP:
		map_update_alt(+1, 1);
		break;

	case SDLK_DOWN:
		map_update_alt(-1, 1);
		break;

	case SDLK_PAGEUP:
		map_setscale(+1, 1);
		break;

	case SDLK_PAGEDOWN:
		map_setscale(-1, 1);
		break;

	default:
		break;
	}
}

static void handle_mouse(SDL_MouseButtonEvent *e, SDL_Surface *screen)
{
	if (e->button == SDL_BUTTON_RIGHT)
	{
		/* teleport */
		int x, z;
		map_getpos(screen, e->x, e->y, &x, &z);
		cmd_goto(x, z);
	}
}

static void handle_chat(unsigned char *msg, int msglen)
{
	if (opt.noansi)
	{
		log_print("[CHAT] %s", msg);
		return;
	}

	static char *colormap[16] =
	{
		"30",   "34",   "32",   "36",   "31",   "35",   "33",   "37",
		"30;1", "34;1", "32;1", "36;1", "31;1", "35;1", "33;1", "0"
	};
	unsigned char *p = msg;
	GString *s = g_string_new("");

	while (msglen > 0)
	{
		if (msglen >= 3 && p[0] == 0xc2 && p[1] == 0xa7)
		{
			unsigned char cc = p[2];
			int c = -1;

			if (cc >= '0' && cc <= '9') c = cc - '0';
			else if (cc >= 'a' && cc <= 'f') c = cc - 'a' + 10;

			if (c >= 0 && c <= 15)
			{
				g_string_append_printf(s, "\x1b[%sm", colormap[c]);
				p += 3;
				msglen -= 3;
				continue;
			}
		}

		g_string_append_c(s, *p++);
		msglen--;
	}

	gchar *str = g_string_free(s, FALSE);
	log_print("[CHAT] %s\x1b[0m", str);
	g_free(str);
}

/* common.h functions */

void inject_to_client(packet_t *p)
{
	g_async_queue_push(iq_client, p);
}

void inject_to_server(packet_t *p)
{
	g_async_queue_push(iq_server, p);
}

static void print_timestamp(void)
{
	char stamp[sizeof("HH:MM:SS ")];
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	strftime(stamp, sizeof(stamp),  "%H:%M:%S ", tm);
	fwrite(stamp, sizeof(stamp)-1, 1, stdout);
}

void log_print(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	print_timestamp();
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);
}

void chat(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	log_print("[INFO] %s", msg);

	static const char prefix[4] = { 0xc2, 0xa7, 'b', 0 };
	char *cmsg = g_strjoin("", prefix, msg, NULL);

	inject_to_client(packet_new(PACKET_TO_ANY, PACKET_CHAT, cmsg));

	g_free(cmsg);
	g_free(msg);
}

void do_die(char *file, int line, int is_stop, char *fmt, ...)
{
	print_timestamp();
	printf("[DIED] %s:%d: ", file, line);

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	putchar('\n');

	if (is_stop)
	{
		world_running = 0;
		g_thread_exit(0);
		/* never reached */
	}

	exit(1);
}
