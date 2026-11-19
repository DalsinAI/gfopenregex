/* MIT: PCRE2-backed regex.library (v2) POSIX ABI adapter for AmigaOS.
 * Upstream PCRE2 is BSD-3-Clause WITH PCRE2-exception. */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <regex.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#define RX2_MAGIC 0x52784732
struct rx2_code {pcre2_code *code; pcre2_general_context *gen; pcre2_match_context *context; ULONG limit;};
/* PCRE2's defaults reference libc allocation functions; these entry points
 * are linked into the library so it never needs a per-process libc init. */
void *malloc(size_t n){ULONG *p=(ULONG *)AllocVec(n+sizeof(ULONG),MEMF_ANY);if(!p)return 0;*p=(ULONG)n;return p+1;}
void free(void *m){if(m)FreeVec(((ULONG *)m)-1);}
static size_t rx2len(const char *p){size_t n=0;while(p&&p[n])n++;return n;}
static void *rx2alloc(PCRE2_SIZE n,void *u){(void)u;return malloc(n);}
static void rx2free(void *p,void *u){(void)u;free(p);}
static ULONG rx2options(LONG f) {
 ULONG o=0;
 if(f & REG_ICASE)o|=PCRE2_CASELESS;
 if(f & REG_UTF8)o|=PCRE2_UTF;
 if(f & REG_DOTALL)o|=PCRE2_DOTALL;
 if(f & REG_MULTILINE)o|=PCRE2_MULTILINE;
 if(f & REG_NEWLINE)o|=PCRE2_MULTILINE;
 if(f & REG_NOSPEC)o|=PCRE2_LITERAL;
 if(f & REG_UNGREEDY)o|=PCRE2_UNGREEDY;
 if(f & REG_UCP)o|=PCRE2_UCP;
 if(f & REG_LITERAL)o|=PCRE2_LITERAL;
 return o;
}
LONG rx2ncomp(regex_t *re,const char *pattern,ULONG n,LONG flags){
 struct rx2_code *b;int err;PCRE2_SIZE off;uint32_t captures=0;
 if(!re||!pattern)return REG_INVARG;
 re->re_magic=0;re->re_g=0;
 b=malloc(sizeof(*b));if(!b)return REG_ESPACE;
 memset(b,0,sizeof(*b));b->gen=pcre2_general_context_create(rx2alloc,rx2free,NULL);
 if(!b->gen){free(b);return REG_ESPACE;}
 pcre2_compile_context *cc=pcre2_compile_context_create(b->gen);
 if(!cc){pcre2_general_context_free(b->gen);free(b);return REG_ESPACE;}
 b->code=pcre2_compile((PCRE2_SPTR)pattern,n,rx2options(flags),&err,&off,cc);
 pcre2_compile_context_free(cc);
 if(!b->code){pcre2_general_context_free(b->gen);free(b);return REG_BADPAT;}
 b->context=pcre2_match_context_create(b->gen);
 if(!b->context){pcre2_code_free(b->code);pcre2_general_context_free(b->gen);free(b);return REG_ESPACE;}
 b->limit=1000000;pcre2_set_match_limit(b->context,b->limit);
 pcre2_pattern_info(b->code,PCRE2_INFO_CAPTURECOUNT,&captures);
 re->re_g=(struct re_guts *)b;re->re_magic=RX2_MAGIC;re->re_nsub=captures;
 return 0;
}
LONG rx2comp(regex_t *re,const char *pattern,LONG flags){
 ULONG n;
 if(!re||!pattern)return REG_INVARG;
 if(flags & REG_PEND){if(!re->re_endp || re->re_endp<pattern)return REG_INVARG;n=(ULONG)(re->re_endp-pattern);}
 else n=rx2len(pattern);
 return rx2ncomp(re,pattern,n,flags);
}
LONG rx2nexec(const regex_t *re,const char *subject,ULONG len,ULONG nm,regmatch_t *out,LONG flags){
 struct rx2_code *b;pcre2_match_data *md;PCRE2_SIZE *ov;ULONG i,start=0,end=len;int r;ULONG opts=0;
 if(!re||re->re_magic!=RX2_MAGIC||!re->re_g)return REG_BADPAT;
 if(!subject)return REG_INVARG;
 if(flags&REG_STARTEND){if(!out||out[0].rm_so<0||out[0].rm_eo<out[0].rm_so)return REG_INVARG;start=out[0].rm_so;end=out[0].rm_eo;}
 if(start>end||end>len)return REG_INVARG;
 b=(struct rx2_code*)re->re_g;
 md=pcre2_match_data_create_from_pattern(b->code,b->gen);if(!md)return REG_ESPACE;
 if(flags&REG_NOTBOL)opts|=PCRE2_NOTBOL;
 if(flags&REG_NOTEOL)opts|=PCRE2_NOTEOL;
 r=pcre2_match(b->code,(PCRE2_SPTR)subject,end,start,opts,md,b->context);
 if(r>=0 && nm && out){ov=pcre2_get_ovector_pointer(md);for(i=0;i<nm;i++){
 if(i<(ULONG)r && ov[i*2]!=PCRE2_UNSET){out[i].rm_so=ov[i*2];out[i].rm_eo=ov[i*2+1];}
 else {out[i].rm_so=-1;out[i].rm_eo=-1;}
 }}
 pcre2_match_data_free(md);
 if(r==PCRE2_ERROR_NOMATCH)return REG_NOMATCH;
 return r>=0?0:(r==PCRE2_ERROR_MATCHLIMIT||r==PCRE2_ERROR_DEPTHLIMIT ? REG_ESPACE:REG_BADPAT);
}
LONG rx2exec(const regex_t *re,const char *subject,ULONG nm,regmatch_t *out,LONG flags){
 if(!subject)return REG_INVARG;
 return rx2nexec(re,subject,(flags&REG_STARTEND && out)?out[0].rm_eo:rx2len(subject),nm,out,flags);
}
ULONG rx2error(LONG err,const regex_t *re,char *buf,ULONG size){
 const char *message="unknown regular expression error";ULONG n=0,i;
 (void)re;
 switch(err){case 0:message="no error";break;case REG_NOMATCH:message="no match";break;case REG_BADPAT:message="invalid PCRE2 pattern or match error";break;case REG_ESPACE:message="PCRE2 allocation or match limit exceeded";break;case REG_INVARG:message="invalid argument";break;}
 n=rx2len(message)+1;
 if(buf&&size){for(i=0;i+1<size&&i<n-1;i++)buf[i]=message[i];buf[i]=0;}
 return n;
}
void rx2release(regex_t *re){struct rx2_code*b;if(!re||re->re_magic!=RX2_MAGIC)return;b=(struct rx2_code*)re->re_g;
 if(b){if(b->context)pcre2_match_context_free(b->context);if(b->code)pcre2_code_free(b->code);if(b->gen)pcre2_general_context_free(b->gen);free(b);}
 re->re_magic=0;re->re_g=0;
}
LONG rx2limit(regex_t *re,ULONG limit){struct rx2_code*b;if(!re||re->re_magic!=RX2_MAGIC||!limit)return REG_INVARG;
 b=(struct rx2_code*)re->re_g;b->limit=limit;return pcre2_set_match_limit(b->context,limit)==0?0:REG_INVARG;
}
