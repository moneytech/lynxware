#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <forms.h>

static FL_FORM *form;
static Window win;
static FL_OBJECT *inpw, *inslt, *outbox;
static FL_OBJECT *mkbutton, *copybutton, *clearbutton, *quitbutton;

#include "xmkhash.xpm"

static char *progname;

void daemonise(void)
{
	pid_t pid, sid;
	int i;

	pid = fork();
	if (pid < 0)
		exit(-1);
	if (pid > 0)
		exit(0);

	sid = setsid();
	if (sid < 0)
		exit(-1);

	close(0);
	close(1);
	close(2);
	for (i = 0; i < 3; i++)
		open("/dev/null", O_RDWR);
}

static void process_entries(void)
{
	const char *pw = fl_get_input(inpw);
	const char *slt = fl_get_input(inslt);
	const char *result = crypt(pw, slt);

	fl_set_input(outbox, result);
}

static void copyclipboard(void)
{
	const char *data = fl_get_input(outbox);
	long len = (long)strlen(data);

	fl_stuff_clipboard(outbox, 0, data, len, NULL);
}

/* A HACK! But no other way to ensure that password is wiped out... */
struct zero_input_hack {
	char *str;
	unsigned int pad1[2];
	int pad2[3];
	int size;
};

static void safe_zero_input(FL_OBJECT *input)
{
	struct zero_input_hack *spec = (struct zero_input_hack *)input->spec;
	memset(spec->str, 0, spec->size);
}

static void clearinput(FL_OBJECT *input)
{
	safe_zero_input(input);
	fl_set_input(input, NULL);
}

static void clearentries(void)
{
	clearinput(inpw);
	clearinput(inslt);
	clearinput(outbox);

	fl_wintitle(win, progname);
	fl_set_focus_object(form, inpw);
}

int main(int argc, char **argv)
{
	FL_OBJECT *called = NULL;

	progname = basename(argv[0]);

	daemonise();

	fl_set_border_width(-1);
	fl_initialize(&argc, argv, "xmkhash", NULL, 0);

	form = fl_bgn_form(FL_BORDER_BOX, 350, 95);

	inpw = fl_add_input(FL_SECRET_INPUT, 5, 5, 200, 25, NULL);
	fl_set_object_return(inpw, FL_RETURN_CHANGED);
	fl_set_object_dblclick(inpw, 0);
	fl_set_input_maxchars(inpw, 64); /* XXX */

	inslt = fl_add_input(FL_NORMAL_INPUT, 210, 5, 135, 25, NULL);
	fl_set_object_return(inslt, FL_RETURN_CHANGED);

	outbox = fl_add_input(FL_NORMAL_INPUT, 5, 35, 340, 25, NULL);
	fl_set_object_return(outbox, FL_RETURN_CHANGED);
	fl_deactivate_object(outbox);

	mkbutton = fl_add_button(FL_NORMAL_BUTTON, 5, 65, 60, 25, "Make");
	fl_set_object_shortcut(mkbutton, "^M", 0);
	copybutton = fl_add_button(FL_NORMAL_BUTTON, 100, 65, 60, 25, "Copy");
	fl_set_object_shortcut(copybutton, "^B", 0);
	clearbutton = fl_add_button(FL_NORMAL_BUTTON, 195, 65, 60, 25, "Clear");
	fl_set_object_shortcut(clearbutton, "^L", 0);
	quitbutton = fl_add_button(FL_NORMAL_BUTTON, 285, 65, 60, 25, "Quit");
	fl_set_object_shortcut(quitbutton, "^[", 0);

	fl_end_form();

	fl_show_form(form, FL_PLACE_CENTER, FL_FULLBORDER, "xmkhash");

	win = fl_winget();

	fl_set_form_icon_data(form, icon);
	fl_set_cursor(win, XC_left_ptr);

	do {
		if (called == mkbutton)
			process_entries();
		else if (called == copybutton)
			copyclipboard();
		else if (called == clearbutton)
			clearentries();
		else if (called == quitbutton) break;
	} while ((called = fl_do_forms()));

	clearentries();

	fl_finish();

	return 0;
}
