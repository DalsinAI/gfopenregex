/* MIT-licensed AmigaDOS Regex command adapter to upstream BSD PCRE2.
 * Preserves old command's line, wildcard and return-code handling. */
#ifndef GF_REGEX_PCRE2_COMPAT_H
#define GF_REGEX_PCRE2_COMPAT_H
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#define REG_PERL 0x1000
#define REG_EXTENDED 0x2000
#define REG_ICASE 0x4000
#define REG_UTF8 0x8000
#define REG_NOSUB 0x10000
#define REG_DOTALL 0x20000
#define REG_MULTILINE 0x40000
#define REG_UNGREEDY 0x80000
#define REG_UCP 0x100000
#define REG_LITERAL 0x200000
#define REG_STARTEND 0x01
#define REG_NOTBOL 0x02
#define GFREG_MATCH_ERROR -2
#define GFREG_NOMATCH 1
#define GFREG_COMPILE_ERROR 2
typedef struct { long rm_so,rm_eo; } regmatch_t;
typedef struct {
 pcre2_code *code;
 pcre2_match_data *data;
 pcre2_match_context *context;
 int error_code;
 PCRE2_SIZE error_offset;
 uint32_t match_limit;
} regex_t;
static int gfreg_match_limit(regex_t *re,uint32_t limit) {re->match_limit=limit;return 0;}
static int regcomp(regex_t *re,const char *pat,int flags) {
 uint32_t o=0;
 if(flags&REG_ICASE)o|=PCRE2_CASELESS;
 if(flags&REG_UTF8)o|=PCRE2_UTF;
 if(flags&REG_DOTALL)o|=PCRE2_DOTALL;
 if(flags&REG_MULTILINE)o|=PCRE2_MULTILINE;
 if(flags&REG_UNGREEDY)o|=PCRE2_UNGREEDY;
 if(flags&REG_UCP)o|=PCRE2_UCP;
 if(flags&REG_LITERAL)o|=PCRE2_LITERAL;
 re->code=pcre2_compile((PCRE2_SPTR)pat,PCRE2_ZERO_TERMINATED,o,&re->error_code,&re->error_offset,NULL);
 if(!re->code)return GFREG_COMPILE_ERROR;
 re->data=pcre2_match_data_create_from_pattern(re->code,NULL);
 re->context=pcre2_match_context_create(NULL);
 if(!re->data||!re->context)return GFREG_COMPILE_ERROR;
 if(re->match_limit)pcre2_set_match_limit(re->context,re->match_limit);
 return 0;
}
static int regexec(regex_t *re,const char *line,size_t nm,regmatch_t *m,int flags) {
 PCRE2_SIZE start=0,len=strlen(line),*v;
 uint32_t opts=(flags&REG_NOTBOL)?PCRE2_NOTBOL:0;
 int rc;
 if(flags&REG_STARTEND && nm && m) {start=(PCRE2_SIZE)m[0].rm_so;len=(PCRE2_SIZE)m[0].rm_eo;}
 if(start>len)return GFREG_NOMATCH;
 rc=pcre2_match(re->code,(PCRE2_SPTR)line,len,start,opts,re->data,re->context);
 if(rc==PCRE2_ERROR_NOMATCH)return GFREG_NOMATCH;
 if(rc<0){re->error_code=rc;return GFREG_MATCH_ERROR;}
 if(nm&&m) {v=pcre2_get_ovector_pointer(re->data);m[0].rm_so=(long)v[0];m[0].rm_eo=(long)v[1];}
 return 0;
}
static size_t regerror(int err,const regex_t *re,char *buf,size_t size) {
 int rc;
 if(!size)return 0;
 if(err==GFREG_COMPILE_ERROR || err==GFREG_MATCH_ERROR){
  rc=pcre2_get_error_message(re->error_code,(PCRE2_UCHAR*)buf,size);
  if(rc>=0)return (size_t)rc;
 }
 strncpy(buf,"PCRE2 regular expression error",size-1);buf[size-1]=0;return strlen(buf);
}
static void regfree(regex_t *re){
 if(re->data)pcre2_match_data_free(re->data);
 if(re->context)pcre2_match_context_free(re->context);
 if(re->code)pcre2_code_free(re->code);
 memset(re,0,sizeof(*re));
}
#endif
