/* regex.library 1.0 (GFOpenRegex): POSIX regular expressions for AmigaOS
 * 3.x, with Perl syntax as an option. docs/regex.library.md is the
 * contract; the calls are src/rx_api.c behind the entry points below, in
 * include/sfd/regex_lib.sfd's order. No floating point, no C library:
 * memory from AllocVec, nothing kept between calls but the caller's own
 * regex_t, so any number of tasks may call it at once.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <libraries/regex.h>
#include "../src/rx_core.h"
#include "../src/rx_api.h"

#define REG(r, decl) register decl __asm(#r)
#define LIB_VERSION 1
#define LIB_REVISION 0

struct RXLibBase {
	struct Library lib;
	BPTR seglist;
};

struct ExecBase *SysBase;

/* Run as a program, the library does nothing. This must stay the first code
 * in the file (-fno-toplevel-reorder). */
int start(void) { return -1; }

static const char lib_name[] = REGEXNAME;
static const char lib_id[] =
	"regex.library 1.0 (06.11.2026) GFOpenRegex, AmiCradle Team\r\n";
static const char lib_ver[] __attribute__((used)) =
	"$VER: regex.library 1.0 (06.11.2026) GFOpenRegex, AmiCradle Team";

static void *env_alloc(struct rx_env *env, rx_u32 size)
{
	(void)env;
	return AllocVec(size ? size : 1, MEMF_ANY);
}

static void env_free(struct rx_env *env, void *mem)
{
	(void)env;
	if (mem) FreeVec(mem);
}

static struct rx_env env = { env_alloc, env_free, 0 };

static struct Library *lib_init(REG(d0, struct RXLibBase *base), REG(a0, BPTR seglist),
                                REG(a6, struct ExecBase *sys));
static struct Library *lib_open(REG(a6, struct RXLibBase *base));
static BPTR lib_close(REG(a6, struct RXLibBase *base));
static BPTR lib_expunge(REG(a6, struct RXLibBase *base));
static ULONG lib_null(void);

/* ---- The calls, in their registers (regex_lib.sfd) ---------------------- */

static LONG L_RegComp(REG(a0, regex_t *preg), REG(a1, CONST_STRPTR pattern), REG(d0, LONG cflags))
{ return rx_api_comp(&env, preg, (const char *)pattern, cflags); }

static LONG L_RegExec(REG(a0, const regex_t *preg), REG(a1, CONST_STRPTR string),
                      REG(d0, ULONG nmatch), REG(a2, regmatch_t *pmatch), REG(d1, LONG eflags))
{ return rx_api_exec(&env, preg, (const char *)string, nmatch, pmatch, eflags); }

static ULONG L_RegError(REG(d0, LONG errcode), REG(a0, const regex_t *preg),
                        REG(a1, STRPTR buf), REG(d1, ULONG size))
{ return rx_api_error(errcode, preg, (char *)buf, size); }

static VOID L_RegFree(REG(a0, regex_t *preg))
{ rx_api_free(&env, preg); }

static LONG L_RegNComp(REG(a0, regex_t *preg), REG(a1, CONST_STRPTR pattern),
                       REG(d0, ULONG length), REG(d1, LONG cflags))
{ return rx_api_ncomp(&env, preg, (const char *)pattern, length, cflags); }

static LONG L_RegNExec(REG(a0, const regex_t *preg), REG(a1, CONST_STRPTR string),
                       REG(d0, ULONG length), REG(d1, ULONG nmatch),
                       REG(a2, regmatch_t *pmatch), REG(d2, LONG eflags))
{ return rx_api_nexec(&env, preg, (const char *)string, length, nmatch, pmatch, eflags); }

static const APTR lib_vectors[] = {
	(APTR)lib_open, (APTR)lib_close, (APTR)lib_expunge, (APTR)lib_null,
	(APTR)L_RegComp, (APTR)L_RegExec, (APTR)L_RegError, (APTR)L_RegFree,
	(APTR)L_RegNComp, (APTR)L_RegNExec,
	(APTR)-1
};

static const struct {
	ULONG size;
	const APTR *vectors;
	APTR data;
	APTR init;
} lib_inittable = {
	sizeof(struct RXLibBase), lib_vectors, NULL, (APTR)lib_init
};

const struct Resident lib_romtag = {
	RTC_MATCHWORD, (struct Resident *)&lib_romtag, (APTR)(&lib_romtag + 1),
	RTF_AUTOINIT, LIB_VERSION, NT_LIBRARY, 0,
	(char *)lib_name, (char *)lib_id, (APTR)&lib_inittable
};

/* ---- Library housekeeping ------------------------------------------------- */

static struct Library *lib_init(REG(d0, struct RXLibBase *base), REG(a0, BPTR seglist),
                                REG(a6, struct ExecBase *sys))
{
	SysBase = sys;
	base->seglist = seglist;
	base->lib.lib_Node.ln_Type = NT_LIBRARY;
	base->lib.lib_Node.ln_Name = (char *)lib_name;
	base->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	base->lib.lib_Version = LIB_VERSION;
	base->lib.lib_Revision = LIB_REVISION;
	base->lib.lib_IdString = (APTR)lib_id;
	return &base->lib;
}

static struct Library *lib_open(REG(a6, struct RXLibBase *base))
{
	base->lib.lib_OpenCnt++;
	base->lib.lib_Flags &= ~LIBF_DELEXP;
	return &base->lib;
}

static BPTR lib_close(REG(a6, struct RXLibBase *base))
{
	if (base->lib.lib_OpenCnt)
		base->lib.lib_OpenCnt--;
	if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP))
		return lib_expunge(base);
	return 0;
}

static BPTR lib_expunge(REG(a6, struct RXLibBase *base))
{
	BPTR seglist;
	if (base->lib.lib_OpenCnt) {
		base->lib.lib_Flags |= LIBF_DELEXP;
		return 0;
	}
	seglist = base->seglist;
	Remove(&base->lib.lib_Node);
	FreeMem((UBYTE *)base - base->lib.lib_NegSize,
	        base->lib.lib_NegSize + base->lib.lib_PosSize);
	return seglist;
}

static ULONG lib_null(void) { return 0; }
