/*
 *	huntbins - program that will sniff directories and
 *	find suspicious files (binaries for example).
 *
 *	This program has long history.
 *
 *	Original idea came to my mind when I began using Linux From Scratch.
 *	First package that contained precompiled unwanted binaries
 *	was module-init-tools (it's module test unit), then I met the
 *	same practice when I compiled VirtualBox (it was
 *	even worse: contained precompiled kBuild binaries that
 *	were executed directly on builder's side and there were no source code for them)
 *
 *	This program's first version was implemented as a
 *	shell script that used find(1) and called file(1) from
 *	file-5.03 and egrep(1) for each found file
 *	It was too slow and resource and execve()-hungry, of course it wasted pids much :-)
 *	Then, after two years of sh version, in late of 2011,
 *	a C version of it was written by me, in hurry.
 *	It had some bugs, used dynamic memory much (even for static buffers)
 *	and did not conformed to standards.
 *	Today I have rewritten it partly from scratch, so it is not so ugly.
 *
 *	I think this program can be used not only for finding unwanted stuff
 *	like precompiled binaries. It can be also used to find pictures,
 *	music files, program sources of any kind; anything that can be
 *	described in libmagic database! Specify pattern files with -beBEp options.
 *
 *	The reason for writing such program was not such thing like
 *	license violation by those who give you GPLed software with precompiled
 *	binaries, but will for clean, built-only-from-source operating system.
 *
 *	To use this program, you must install file package
 *	(that contains libmagic). File-5.03+ is fine.
 *
 *	Compile with: cc huntbins.c -o huntbins -lmagic
 *
 *	12Feb2015: added two but important options:
 *		-u: remove found files
 *		-a: look inside archives that are on the way (maybe dangerous)
 *
 *	Beware of massive comments!
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <regex.h>
#include <ftw.h>
#include <magic.h>
#include <errno.h>
#include <limits.h>

#define MAXDIRDEPTH 256
#define TMPDIRTEMPL "/tmp/.huntbins.XXXXXX"
/* for remove temporaries from /tmp code */
#define RMFRCOMM "exec /bin/rm -fr"

static char *progname;
static char *progfull;
static int gargc;
static char **gargv;
static int gidx;

/*
 * My binaries list!
 * This is a list of binaries that I found often in
 * software archives. Mostly they are ELF's, because author
 * forgot to 'make clean' his package. But there also
 * binaries that placed specially to serve some purpose,
 * or even if they are have no source code.
 *
 * Each binary type description has comment why binary type was blacklisted.
 */
static char *_bins[] = {

/* common and often seen */
	"((E|O)LF (32|64)-bit|COFF|PE32|Mach-O|PEF)", /* format specific: elf32/64, coff, */
					/* pe32, mach-o, olf(?), pef */
	"(O|N|Z|C|Q)MAGIC|(a|b|m|x)\\.out", /* a.out Linux, generic *.out */
	"current ar archive|^archive$|System V Release 1 ar archive" /* ar archive, not any. */
		/* The 'ar archive' occasionally matches 'Par archive data', so */
		/* a distinction between them is made. */
		/* The purpose of archives today is to contain object files */

		/* Other (platform) archives */
		"|(MIPS|VAX|PDP-11).*archive",
	"(pure|x86|8086|286|386|Dev86|sparc|PDP|(|m|M)(6|8)8(k|K)|68xxx|amd|29k)"
		".*(executable|shared (library|executable)|object file)", /* ... arch specific */
	"(sysV|SVR|Linux[-/]|FreeBSD|DOS|Atari|AmigaOS|BFLT|VMS|RISC OS|PA-RISC|HP s[0-9]*"
		"|Encore|basic-16|SGI|QL Firmware|Plan 9|sun-2|Alliant"
		"|Playstation|Xbox|unknown)"
		".*(executable|shared (library|executable)|object file)", /* ... and os specific */
	"Linux.*kernel", /* any Linux kernel */
	"core file", /* core files, any */
	"BIOS|(F|f)irmware|boot rom", /* bioses */
	"mbr|MBR|Bootloader", /* boot sectors */
	"COM executable (\\(32-bit COMBOOT|COM32R)", /* syslinux boot execs */
	"UPX|ARX|LHarc|LARC|LZEXE|32Lite|aPack|UCEXE|WWPACK"
			"|diet|TinyProg|AIN (1|2)\\.x|Compack"
			"|(s|S)elf-extracting.*archive", /* exe compressors and sfx archives */
	"ROM (data|dump|Image)", /* consoles */
	"Gameboy ROM|Sega Dreamcast VMU game image", /* consoles */

/* misc stuff */
	"byte-compiled", /* python/lisp/Emacs compiled */
	"LISP memory image data|compiled Lisp", /* Lisp 'executables' */
	"PHP script Zend Optimizer data", /* Zendguarded php scripts */
	"(MIPS|Alpha) archive", /* Seen somewhere in old sources */
	"compiled Java class data", /* Java stuff */
	"JAR compressed", /* Jar for java */
	"Apple(Single|Double) encoded Macintosh file", /* Can be seen in archives from Mac OS X */
	"magic binary file", /* Match compiled file(1) itself */
	"GCC precompiled header", /* aka gch */
	"Lua bytecode", /* Lua bytecodes - can be seen in game archives */
	"NetWare Loadable Module", /* NLMs, rarely occured */
	"Quake.*Map file \\(BSP\\)|VBSP", /* Game data without corresponding .map source */
	"LLVM (byte-codes|bitcode)", /* LLVM bytecode (why _bit_ code?) */
	"GNU message catalog", /* .mo's without .po's */
	"libtool library file", /* just get rid of these */
	"Vim swap file", /* Vim's *.swp */
	"Vim encrypted file data", /* VimCrypt~ (encrypted text files with Vim) */
	"Microsoft Visual C library", /* found somewhere in old archives */
	"Dalvik dex file", /* !! NEW NEW NEW !! androidware detection & elimination */
	"MSVC program database ver",
	"timezone data",
	"RISC OS Chunk data, ALF library", /* More of them? */
	"Motorola S-Record; binary data in text format",

/* never-seen things, added by me from Magdir constantly */
/* move to appropriate section when found */
	"SYMMETRY i386 .o",
	"Universal EFI binary",
	"Parrot bytecode", /* Parrot VM (?) */
	"Intel serial flash",
	"Guile Object", /* guile (?) */
	"MS Windows 32bit crash dump",
	"Cyrus sieve bytecode data,",
	"GNU prof performance data",
	"MDMP crash report data",
	"Symbian installation file",

/* libmagic does not know about them */
	"^\\\\0 \\\"\\\\353X\\\\220[A-Z]*LINUX\\\"", /* syslinux binary objects (.bss) */
	"^\\\\0 \\\"TPUQ\\\"", /* turbo pasquiel 7 various binary objects (*.tpu) */
	"^\\\\0 \\\"ANDROID!\\\"",
	NULL
};

/*
 * File extensions.
 * This is my file extensions section. It has definitions
 * of file extensions that are often contain binaries or unwanted
 * contents (PE32 or DOS executables, raw x86 code, bioses)
 */
#define _s "[^/]\\." /* skip file paths and start with dot */
static char *_exts[] = {
	_s "(exe|ocx|sys|vxd|drv|o)$", /* common executables and objects */
	_s "(.*rom|bin|dtb|mbr)$", /* bios rom (zrom) and common binary objects */
	_s "(gch)$", /* precompiled gcc headers */
	_s "(jar|class)$", /* just get rid of them */
	_s "(py(c|o))$", /* Python byte-compiled */
	_s "(qm)$", /* Qt compiled translation file */
	_s "((|g)mo)$", /* gnu message catalog */
	_s "(deb|rpm|msi|dmg|run)$", /* various software packages */
	_s "(bsp|nav)$", /* game objects */
	_s "(c32|com32|bss)$", /* syslinux bootloader objects */
	_s "(nes|snes|smc|smd|swc|gb|gba|gbc|sgb|ufo"
	"|sfc|gd3|gd7|dx2|mgd|mgh|0(4|5|6|7)8"
	"|usa|eur|jap|aus|bsx)$", /* console emulators images */
	_s "(tpu|dcu)$", /* tp7 and delphie stuff */
	_s "(dex|apk)$", /* get rid of androidware */
	_s "(ncb|srec)$",
	"[^/],(ff(a|8)|fe6|fd9)$", /* RISCWARE */

/* achtung file names */
	"(autorun\\.inf|thumbs\\.db|\\.exe\\.manifest)$", /* get rid of Windows garbage */
	"(\\.DS_Store)$", /* get rid of OS X garbage */
	"((|g)pxe|sys|iso|ext)linux\\.0$", /* syslinux binary objects */
	"(stage[0-9])-[^\\./]*$", /* grubbie binaries */
	NULL
};

/* White Listed binaries */
static char *_wbin[] = {
	"ASCII text|Unicode text",
	"script text|text executable", /* most text executable scripts */
	NULL
};

/* White Listed file extensions */
static char *_wext[] = {
	_s "(qpf)$", /* Qt fonts, found in Qt4 source package */

/* whitelisted file names */
	"/\\.git/", /* don't match anything from git local repos */
	"/(|GNU)(m|M)akefile.*$", /* any makefiles */
	NULL
};

/* Pair matching: match both file extension and type */
static char *_pairs[] = {
	_s "(com)$", "(DOS|COM) executable|^data$", /* most COM files for DOS, */
					/* often false reported for !.com files */
					/* (.com also means DCL command file) */
	_s "(obj)$", "8086 relocatable", /* MS object files */
					/* false reported as 3D objects */
					/* most MS .obj's have COFF pattern, this is for */
					/* rarely seen .obj's */
	_s "(cab|.*_)$", "Microsoft Cabinet archive data", /* MS cabinet archives */
	NULL
};

/* Pair exceptions */
static char *_pext[] = {
	_s "(js)$", "Emacs v18 byte-compiled Lisp data", /* seen in mozilla-release (ff12) */
	_s "(tex)$", "VAX COFF executable not stripped", /* android apk contents misdetection */
	NULL
};
#undef _s

/*
 * File-5.10 added octal representation of unknown files,
 *        so it's our task to determine them:
 * - Looking for two or more octal numbers. Zero files output
 *        one octal (string "\0"), two or more are our clients!
 * - Matching the '\NNN " ... "' sequence
 */
static char *datatext[] = {
	"^data$", /* match "data" */
	"^(\\\\[0-9]*.*\\\\[0-9]*|\\\\[0-9]*\\ \\\".*\\\")",
	NULL
};

/* Always-false pattern */
static char *falsetext[] = {"^$", NULL};

/*
 * to implement archive looking, we need a sane list of
 * archiver command lines wired to certain file types
 * define archiver line with two strings:
 * - file type
 * - command line passed to system() with "%s" being replaced by item name
 * Nobody can rely on file extension since it can be omitted.
 *
 * Those /dev/null redirections are for quiet operation.
 * Sometimes huntbins can face corrupted or fake data which cannot
 * be decompressed, and error messages are polluting screen.
 * The only error message which is meaningful is "Unsupported archive".
 */
static const char *archivers[] = {
	"tar archive", "tar -xf \"%s\" &>/dev/null",
	"cpio archive", "exec cpio -idu -F \"%s\" &>/dev/null",
	"gzip compressed data", "gzip -dc \"%s\" 2>/dev/null | tar -x &>/dev/null",
	"bzip2 compressed data", "bzip2 -dc \"%s\" 2>/dev/null | tar -x &>/dev/null",
	"XZ compressed data", "xz -dc \"%s\" 2>/dev/null | tar -x &>/dev/null",
	"Zip archive data", "exec unzip -qq \"%s\" &>/dev/null",
	"Zip multi-volume archive data", "exec unzip -qq \"%s\" &>/dev/null",
	"(7-zip|ARJ|RAR|Microsoft Cabinet|Zoo|ASE) archive data", "exec 7z x \"%s\" &>/dev/null",
	/* say we can't support it (yet) */
	"archive data", "echo %s: Unsupported archive >&2",
	NULL
};

static char **bins = _bins;
static char **exts = _exts;
static char **wbin = _wbin;
static char **wext = _wext;
static char **pairs = _pairs;
static char **pext = _pext;

static magic_t mgc;
static char **names; static int nindex;

static int matchdata;
static int icase;
static int quiet;
static int mgcflags = MAGIC_DEVICES;
static int concat;
static int removethem;
static int lookarchives;
static char *mgcdb;

static int progret;
static char cwd[PATH_MAX];
static char pathbuf[PATH_MAX], cmdbuf[PATH_MAX];

static void usage(void)
{
	printf("usage: %s [-auUNCiIzq] [-bepBEP inputfile]"
	       	" [-o outfile] [-l mgcdb] ...\n", progname);
	printf(" huntbins - program that finds binaries in directories\n\n");
	printf("Options:\n");
	printf("  -a: (try to) look into archives too (!! may be dangerous)\n");
	printf("  -U: looks for 'data' files\n");
	printf("  -N: resets all internal pattern lists to unmatch pattern ('^$')\n");
	printf("  -i: make all matches case insensitive\n");
	printf("  -I: make all matches case sensitive (even file extensions)\n");
	printf("  -bepBEP inputfile: load patterns from file (or stdin, if '-'):\n");
	printf("  \tb: binaries pattern, e: file extensions pattern,\n");
	printf("  \tB: binaries white list, E: extensions white list\n");
	printf("  \tp: pair patterns (odd lines: file extensions,"
	       	" even lines: matching pattern)\n");
	printf("  \tP: pair patterns exceptions, order is same as for -p\n");
	printf("  -C: combine embedded patterns and input patterns, oppose to -N\n");
	printf("  -o outfile: output list of files to outfile (default is stdout)\n");
	printf("  -z: see into compressed (.gz/.bz2/.xz ...) files too\n");
	printf("  -l mgcdb: load alternate magic database\n");
	printf("  -u: remove found files\n");
	printf("  -q: be quiet: don't output any filenames, only return status\n");
	printf("  ...: directory(ies) or file(s) to search\n\n");
	printf("Return values:\n");
	printf("  0: success, no binaries\n");
	printf("  1: failure, there are binaries\n");
	printf("  2: directory, filename, input or output file cannot"
	       	" be found/created etc.\n");
	printf("  3: invalid parameters passed\n\n");
	exit(3);
}

static void xerror(const char *reason)
{
	char buf[256] = {0};

	snprintf(buf, sizeof(buf), "%s: %s", progname, reason);
	if (errno && reason)
		perror(buf);
	else
		fprintf(stderr, "%s\n", buf);
	exit(2);
}

static int match(const char *string, const char *pattern, int icase)
{
	int status = 0, flags = 0;
	regex_t r;

	flags = REG_EXTENDED | REG_NOSUB;
	if (icase) flags |= REG_ICASE;

	if (regcomp(&r, pattern, flags) != 0) return 0;

	status = regexec(&r, string, (size_t)0, NULL, 0);

	regfree(&r);

	if (status) return 0;
	return 1;
}

static int getmgcfiletype(magic_t cookie, const char *mgcpath,
			 const char *file, char *output, size_t len)
{
	const char *type = NULL;

	if (cookie == NULL) return 0;

	if (magic_load(cookie, mgcpath) != 0) {
		char tmp[PATH_MAX] = {0};

		strcpy(tmp, "can't load database ");
		strncat(tmp, mgcpath, PATH_MAX - 20);
		xerror(tmp);
	}

	type = magic_file(cookie, file);

	strncpy(output, type, len);
	return 1;
}

static int filematch(const char *name, const struct stat *st, int type, struct FTW *ftwbuf)
{
	char buf[1024] = {0};
	int add = 0;
	int i;

	if (access(name, R_OK) != 0) { progret = 2; if (!quiet) perror(name); goto _ret; }

	if (type == FTW_F) {
		/* test file extensions */
		for (i = 0; exts[i]; i++) {
			if (match(name, exts[i], (icase == 0 || icase == 1) ? 1 : 0)) {
				add = 1;
				break;
			}
		}

		for (i = 0; wext[i]; i++) {
			if (match(name, wext[i], (icase == 0 || icase == 2) ? 0 : 1))
				goto _ret;
		}

		if (add) goto _add;

		/* not found early? then look into file contents */
		if (getmgcfiletype(mgc, mgcdb, name, buf, sizeof(buf)-1)) {
			for (i = 0; bins[i]; i++) {
				if (match(buf, bins[i],
					(icase == 0 || icase == 2) ? 0 : 1)) {
						add = 1;
						break;
				}
			}

			if (pairs != falsetext) {
				for (i = 0; pairs[i]; i += 2) {
					if (match(buf, pairs[i+1],
						(icase == 0 || icase == 2) ? 0 : 1)
					&& match(name, pairs[i],
						(icase == 0 || icase == 1) ? 1 : 0)) {
							add = 1;
							break;
					}
				}
			}

			if (pext != falsetext) {
				for (i = 0; pext[i]; i += 2) {
					if (match(buf, pext[i+1],
						(icase == 0 || icase == 2) ? 0 : 1)
					&& match(name, pext[i],
						(icase == 0 || icase == 1) ? 1 : 0)) {
							add = 0;
							break;
					}
				}
			}

			for (i = 0; wbin[i]; i++) {
				if (match(buf, wbin[i],
					(icase == 0 || icase == 2) ? 0 : 1)) {
						add = 0;
						break;
				}
			}

			if (matchdata) {
				for (i = 0; datatext[i]; i++) {
					if (match(buf, datatext[i], 0)) {
						add = 1;
						break;
					}
				}
			}

			if (add) goto _add;
		}

		/* still not found? maybe it's an archive we need to handle */
		if (lookarchives) {
			char templ[256], *s;
			int st = 0;

			memset(templ, 0, sizeof(templ));
			memset(pathbuf, 0, sizeof(pathbuf));
			memset(cmdbuf, 0, sizeof(cmdbuf));

			for (i = 0; archivers[i]; i += 2) {
				if (match(buf, archivers[i],
					(icase == 0 || icase == 2) ? 0 : 1)) {
					if (!realpath(name, pathbuf)) {
						perror(name);
						goto _ret;
					}
					strcpy(templ, TMPDIRTEMPL);
					s = mkdtemp(templ);
					if (!s) { progret = 2; perror(templ); goto _ret; }
					if (chdir(s) == -1) {
						progret = 2;
						perror(s);
						goto _rmdtemp;
					}
					snprintf(cmdbuf, sizeof(cmdbuf), archivers[i+1],
						pathbuf);
					st = system(cmdbuf);
					if (st != 0)
					/* if not extracted, then nothing interesting here... */
					if (st != 0) goto _rmdtemp;
					/*
					 * currently huntbins cannot map all it's options
					 * properly here, and remove not needed ones.
					 * I simply will pass -q to child huntbins here.
					 * I am sorry if you'd expected different behavior.
					 */
					st = system("exec huntbins -q");
					/* Yes we found it!! */
					if (st != 0) add = 1;

_rmdtemp:
					if (chdir(cwd) == -1) xerror(cwd);

					/* safely remove our temporaries */
					memset(cmdbuf, 0, sizeof(cmdbuf));
					snprintf(cmdbuf, sizeof(cmdbuf), "%s %s",
						RMFRCOMM, s);
					st = system(cmdbuf);
					if (st != 0) {
						progret = 2;
						fprintf(stderr,
							"cannot remove %s, system(\"%s\") = %d\n",
								s, cmdbuf, st);
					}
					if (add) goto _add;
				}
			}
		}
	}

	goto _ret;

_add:
	if (quiet) { progret = 1; goto _ret; }
	if (nindex) names = realloc(names, (nindex + 1) * sizeof(char *));
	names[nindex] = malloc(strlen(name) + 1); memset(names[nindex], 0, strlen(name) + 1);
	strncpy(names[nindex], name, strlen(name) + 1);
	nindex++;

_ret:
	memset(buf, 0, sizeof(buf));
	return 0;
}

static int scompare(const void *p1, const void *p2)
{
	const char *m1 = *(const char **) p1;
	const char *m2 = *(const char **) p2;

	return strcmp(m1, m2);
}

static void sort(char **array, size_t arraylen)
{
	if (arraylen == 0) return;

	qsort(array, arraylen, sizeof(char *), scompare);
}

static void unique(char **array, size_t arraylen)
{
	int i;

	if (arraylen == 0) return;

	for (i = 0; i < arraylen - 1; i++) {
		if (array[i] && array[i+1]) {
			if (!strncmp(array[i], array[i+1], strlen(array[i+1]))) {
				free(array[i]);
				array[i] = NULL;
			}
		}
	}
}

static void loadlist(const char *filename, char ***what, int concat, int even)
{
	FILE *f = NULL;
	char buf[4096] = {0}; size_t l = 0;
	char **old = NULL;
	int i;

	if (!strcmp(filename, "-")) { f = stdin; goto _noopen; }
	f = fopen(filename, "rb");
	if (!f) xerror(filename);

_noopen:
	if (concat) old = what[0];

	what[0] = NULL;
	what[0] = malloc(sizeof(char *));

	for (i = 0;; i++) {
		if (feof(f)) break;
		if (fgets(buf, sizeof(buf)-1, f) == NULL) break;
		l = strlen(buf) > sizeof(buf)-1 ? sizeof(buf)-1 : strlen(buf);
		if (buf[l - 1] == '\n')
			buf[l - 1] = '\0';
		l--;

		what[0] = realloc(what[0], (sizeof(char *) * (i+1)));
		what[0][i] = malloc(l); memset(what[0][i], 0, l);
		strncpy(what[0][i], buf, l);
	}

	fclose(f);

	if (even && !!(i % 2)) {
		what[0] = realloc(what[0], (sizeof(char *) * (i+1)));
		what[0][i] = malloc(1); what[0][i][0] = 0;
		i++;
	}

	if (concat) {
		int x;

		for (x = 0; old[x]; x++, i++) {
			what[0] = realloc(what[0], (sizeof(char *) * (i+1)));
			what[0][i] = malloc(strlen(old[x]) + 1);
			memset(what[0][i], 0, strlen(old[x]) + 1);
			strncpy(what[0][i], old[x], strlen(old[x]));
		}
	}

	if (i) {
		what[0] = realloc(what[0], (sizeof(char *) * (i+1)));
		what[0][i] = NULL;
	}
	else {
		free(what[0]);
		what[0] = falsetext;
	}
}


int main(int argc, char **argv)
{
	gargc = argc; gargv = argv;
	progname = basename(*argv), progfull = *argv;
	FILE *out = stdout;
	int i;

	int idx = 0;
	int c = 0;

	setenv("LC_ALL", "C", 1);
	setenv("LANG", "C", 1);

	opterr = 0;
	while ((c = getopt(argc, argv, "UiINCzqo:b:e:B:E:p:P:l:ua")) != -1) {
		switch (c) {
			case 'o':
				fclose(out);
				out = fopen(optarg, "wb");
				if (!out) xerror(optarg);
				break;
			case 'U': matchdata = 1; break;
			case 'N':
				concat = 0;
				/* "falsify"... */
				bins = falsetext;
				exts = falsetext;
				wbin = falsetext;
				wext = falsetext;
				pairs = falsetext;
				pext = falsetext;
				break;
			case 'C':
				concat = 1;
				bins = _bins;
				exts = _exts;
				wbin = _wbin;
				wext = _wext;
				pairs = _pairs;
				pext = _pext;
				break;
			case 'i':
				icase = 1;
				break;
			case 'I':
				icase = 2;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'b':
				loadlist(optarg, &bins, concat, 0);
				break;
			case 'e':
				loadlist(optarg, &exts, concat, 0);
				break;
			case 'B':
				loadlist(optarg, &wbin, concat, 0);
				break;
			case 'E':
				loadlist(optarg, &wext, concat, 0);
				break;
			case 'p':
				loadlist(optarg, &pairs, concat, 1);
				break;
			case 'P':
				loadlist(optarg, &pext, concat, 1);
				break;
			case 'l':
				mgcdb = optarg;
				break;
			case 'z':
				mgcflags ^= MAGIC_COMPRESS;
				break;
			case 'a':
				lookarchives = 1;
				break;
			case 'u':
				removethem = 1;
				break;
			default: usage(); break;
		}
	}

	if (!quiet) names = malloc(sizeof(char *));

	mgc = magic_open(mgcflags);

	gidx = idx = optind;

	if (!getcwd(cwd, sizeof(cwd)))
		xerror("getcwd");

	if (argv[idx]) {
		for (i = idx; i < argc; i++) {
			if (access(argv[i], F_OK) != 0) { progret = 2; perror(argv[i]); continue; }
			nftw(argv[i], filematch, MAXDIRDEPTH, FTW_PHYS);
		}
	}
	else
		nftw(".", filematch, MAXDIRDEPTH, FTW_PHYS);

	sort(names, nindex);
	unique(names, nindex);

/* print out names here, or output names to file, optionally removing them */
	for (i = 0; i < nindex; i++) {
		if (names[i]) {
			if (!quiet) fprintf(out, "%s\n", names[i]);
			if (removethem) {
				if (unlink(names[i]) == -1) {
					perror(names[i]);
				}
			}
			free(names[i]);
			names[i] = NULL;
		}
	}

	if (names) { free(names); names = NULL; }
	magic_close(mgc);
	fclose(out);
	if (bins != _bins && bins != falsetext) {
		for (i = 0; bins[i]; i++) { free(bins[i]); bins[i] = NULL; }
		free(bins); bins = NULL;
	}
	if (exts != _exts && exts != falsetext) {
		for (i = 0; exts[i]; i++) { free(exts[i]); exts[i] = NULL; }
		free(exts); exts = NULL;
	}
	if (wbin != _wbin && wbin != falsetext) {
		for (i = 0; wbin[i]; i++) { free(wbin[i]); wbin[i] = NULL; }
		free(wbin); wbin = NULL;
	}
	if (wext != _wext && wext != falsetext) {
		for (i = 0; wext[i]; i++) { free(wext[i]); wext[i] = NULL; }
		free(wext); wext = NULL;
	}
	if (pairs != _pairs && pairs != falsetext) {
		for (i = 0; pairs[i]; i++) { free(pairs[i]); pairs[i] = NULL; }
		free(pairs); pairs = NULL;
	}
	if (pext != _pext && pext != falsetext) {
		for (i = 0; pext[i]; i++) { free(pext[i]); pext[i] = NULL; }
		free(pext); pext = NULL;
	}
	if (nindex) progret = 1;

	return progret;
}
