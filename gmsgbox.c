#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <gtk/gtk.h>

typedef void (*sighandler_t)(int);

#define _s(x) (sizeof(x)/sizeof(*x))

struct buttons {
	char *str;
	int id;
};

static struct buttons btns[64];
static int btnsidx;
static GtkWidget *dlg;
static int mtype = GTK_MESSAGE_INFO, btype = GTK_BUTTONS_OK;
static char *title = "", *text = "", *p;
static size_t l;
static int def, z, r;
static suseconds_t timeout;

static void timeouthandl(int unused)
{
	exit(-1);
}

static void usage(void)
{
	printf("usage: gmsgbox [-b id:btntxt] [-d id] [-z id] [-t time] [-NiwqenykKcC] [title] [text]\n\n");
	printf("  -b id:btntxt: add new button with btntxt on it and returning id\n");
	printf("  -d id: set default button to id it returns\n");
	printf("  -z id: return 0 on this id\n");
	printf("  -t time: set timeout (in microseconds) after which dialog will close\n");
	printf("  -N: set dialog type to 0\n");
	printf("  -i: set dialog type to INFO\n");
	printf("  -w: set dialog type to WARN\n");
	printf("  -q: set dialog type to QUESTION\n");
	printf("  -e: set dialog type to ERROR\n");
	printf("  -n: set button type to 0\n");
	printf("  -y: set button type to YESNO\n");
	printf("  -k: set button type to OK\n");
	printf("  -c: set button type to CANCEL\n");
	printf("  -K: set button type to OKCANCEL\n");
	printf("  -C: set button type to CLOSE\n");
	printf("\n");
	printf("this environment variables are listened:\n");
	printf("  GMSGBOX_TITLE sets title of msgbox\n");
	printf("  GMSGBOX_TEXT sets text of msgbox\n");
	printf("environment variables override any texts passed in argv\n");
	printf("internally, all response numbers are negative\n");
	printf("\n");
	exit(2);
}

int main(int argc, char **argv)
{
	int c;
	struct itimerval timer;

	opterr = 0;
	while ((c = getopt(argc, argv, "b:d:z:t:NiwqenykKcC")) != -1) {
		switch (c) {
			case 'b':
				if (btnsidx > _s(btns)) break;
				p = strchr(optarg, ':');
				if (p) {
					*p = 0; p++;
					l = strlen(p);
					btns[btnsidx].str = malloc(l+1);
					if (!btns[btnsidx].str) break;
					memset(btns[btnsidx].str, 0, l+1);
					strncpy(btns[btnsidx].str, p, l);
					btns[btnsidx].id = -atoi(optarg);
					btnsidx++;
				}
				break;
			case 'd':
				def = -atoi(optarg);
				break;
			case 'z':
				z = atoi(optarg);
				break;
			case 't':
				timeout = (useconds_t)atol(optarg);
				break;
			case 'N':
				mtype = 0;
				break;
			case 'i':
				mtype |= GTK_MESSAGE_INFO;
				break;
			case 'w':
				mtype |= GTK_MESSAGE_WARNING;
				break;
			case 'q':
				mtype |= GTK_MESSAGE_QUESTION;
				break;
			case 'e':
				mtype |= GTK_MESSAGE_ERROR;
				break;
			case 'n':
				btype = 0;
				break;
			case 'y':
				btype |= GTK_BUTTONS_YES_NO;
				break;
			case 'k':
				btype |= GTK_BUTTONS_OK;
				break;
			case 'K':
				btype |= GTK_BUTTONS_OK_CANCEL;
				break;
			case 'c':
				btype |= GTK_BUTTONS_CANCEL;
				break;
			case 'C':
				btype |= GTK_BUTTONS_CLOSE;
				break;
			default: usage(); break;
		}
	}

	if (*(argv+optind)) { title = *(argv+optind); optind++; }
	if (*(argv+optind)) { text = *(argv+optind); optind++; }

	if (getenv("GMSGBOX_TITLE")) title = getenv("GMSGBOX_TITLE");
	if (getenv("GMSGBOX_TEXT")) text = getenv("GMSGBOX_TEXT");

	if (timeout) {
		signal(SIGALRM, &timeouthandl);
		memset(&timer, 0, sizeof(struct itimerval));
		timer.it_value.tv_sec = (timeout / 1000000);
		timer.it_value.tv_usec = timeout - (timer.it_value.tv_sec * 1000000);
		setitimer(ITIMER_REAL, &timer, NULL);
	}
	gtk_init(&argc, &argv);
	dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, mtype, btype, title);
	gtk_window_set_title(GTK_WINDOW(dlg), title);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", text);
	for (l = 0; l < btnsidx; l++)
		gtk_dialog_add_button(GTK_DIALOG(dlg), btns[l].str, (gint)btns[l].id);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), (gint)def);
	r = -(gint)gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(GTK_WIDGET(dlg));

	return r == z ? 0 : r;
}
