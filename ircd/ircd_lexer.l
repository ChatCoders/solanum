/* src/ircd_lexer.l
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 */

%option case-insensitive
%option noyywrap
%option nounput

%{
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <errno.h>
#include <limits.h>

#define WE_ARE_MEMORY_C

#include "stdinc.h"
#include "ircd_defs.h"
#include "defaults.h"
#include "logger.h"
#include "s_conf.h"
#include "newconf.h"

#include "ircd_parser.h"

int yylex(void);

#define MAX_INCLUDE_DEPTH 10

YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr=0;
int lineno = 1;
void ccomment(void);
void cinclude(void);
void hashcomment(void);
int ieof(void);
int lineno_stack[MAX_INCLUDE_DEPTH];
char conffile_stack[MAX_INCLUDE_DEPTH][BUFSIZE];
char conffilebuf[BUFSIZE+1];
char *current_file = conffilebuf;

FILE *inc_fbfile_in[MAX_INCLUDE_DEPTH];

char yy_linebuf[16384];

#undef YY_INPUT

#define YY_FATAL_ERROR(msg) conf_yy_fatal_error(msg)

#define YY_INPUT(buf,result,max_size) \
  if (!(result = conf_fgets(buf, max_size, conf_fbfile_in))) \
    YY_FATAL_ERROR("input in flex scanner failed");
%}

ws        [ \t]*
digit     [0-9]
comment   #.*
qstring   \"[^\"\n]*[\"\n]
string    [a-zA-Z_\~\:][a-zA-Z0-9_\:]*
include   \.include{ws}(\<.*\>|\".*\")

%%
{include}       { cinclude(); }
"/*"            { ccomment(); }
\n.*            { rb_strlcpy(yy_linebuf, yytext+1, sizeof(yy_linebuf)); lineno++; yyless(1); }

{ws}            ;
{comment}       { hashcomment(); }

{digit}+        { yylval.number = atoi(yytext); return NUMBER; }

{qstring}	{
                  if(yytext[yyleng-2] == '\\')
                    {
                      yyless(yyleng-1); /* return last quote */
                      yymore();         /* append next string */
                    }
                  else
                    {
                      rb_strlcpy(yylval.string, yytext + 1, 1024);
                      if(yylval.string[yyleng-2] != '"')
                        ilog(L_MAIN, "Unterminated character string");
                      else
                        {
                          int i,j;
                          yylval.string[yyleng-2] = '\0'; /* remove close
                                                           *  quote
                                                           */

                          for (j=i=0 ;yylval.string[i] != '\0'; i++,j++)
                            {
                              if (yylval.string[i] != '\\')
                                {
                                  yylval.string[j] = yylval.string[i];
                                }
                              else
                                {
                                  i++;
                                  if (yylval.string[i] == '\0') /* XXX
                                                                 * should not
                                                                 * happen
                                                                 */
                                    {
                                      ilog(L_MAIN,
                                           "Unterminated character string");
                                      break;
                                    }
                                  yylval.string[j] = yylval.string[i];
                                }
                            }
                          yylval.string[j] = '\0';
                          return QSTRING;
                        }
                    }
                }


loadmodule	{ return LOADMODULE; }
{string}        {
		  rb_strlcpy(yylval.string, yytext, 1024);
                  yylval.string[yyleng] = '\0';
                  return STRING;
                }

\.\.		{ return TWODOTS; }
.               { return yytext[0]; }
<<EOF>>         { if (ieof()) yyterminate(); }
%%

/* C-comment ignoring routine -kre*/
void ccomment()
{
  int c;

  /* log(L_NOTICE, "got comment"); */
  while (1)
    {
      while ((c = input()) != '*' && c != EOF)
        if (c == '\n') ++lineno;
      if (c == '*')
        {
          while ((c = input()) == '*');
          if (c == '/')
            break;
          if (c == '\n') ++lineno;
        }
      if (c == EOF)
        {
          YY_FATAL_ERROR("EOF in comment");
          /* XXX hack alert this disables
           * the stupid unused function warning
           * gcc generates
           */
          yy_fatal_error("EOF in comment");
          break;
        }
    }
}

void cinclude(void)
{
  char *c;
  if ((c = strchr(yytext, '<')) == NULL)
    *strchr(c = strchr(yytext, '"') + 1, '"') = 0;
  else
    *strchr(++c, '>') = 0;

  /* do stacking and co. */
  if (include_stack_ptr >= MAX_INCLUDE_DEPTH)
    conf_report_error("Includes nested too deep (max is %d)", MAX_INCLUDE_DEPTH);
  else
  {
    FILE *tmp_fbfile_in;

    tmp_fbfile_in = fopen(c, "r");

    if (tmp_fbfile_in == NULL)
    {
      /* if its not found in PREFIX, look in IRCD_PATH_ETC */
      char fnamebuf[BUFSIZE];

      snprintf(fnamebuf, sizeof(fnamebuf), "%s/%s", ircd_paths[IRCD_PATH_ETC], c);
      tmp_fbfile_in = fopen(fnamebuf, "r");

      /* wasnt found there either.. error. */
      if(tmp_fbfile_in == NULL)
      {
        conf_report_error("Include %s: %s.", c, strerror(errno));
        return;
      }
    }
    lineno_stack[include_stack_ptr] = lineno;
    lineno = 1;
    inc_fbfile_in[include_stack_ptr] = conf_fbfile_in;
    strcpy(conffile_stack[include_stack_ptr], c);
    current_file = conffile_stack[include_stack_ptr];
    include_stack[include_stack_ptr++] = YY_CURRENT_BUFFER;
    conf_fbfile_in = tmp_fbfile_in;
    yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  }
}

int ieof(void)
{
  if (include_stack_ptr)
    fclose(conf_fbfile_in);
  if (--include_stack_ptr < 0)
  {
    /* We will now exit the lexer - restore init values if we get /rehash
     * later and reenter lexer -kre */
    include_stack_ptr = 0;
    lineno = 1;
    return 1;
  }
  /* switch buffer */
  yy_delete_buffer(YY_CURRENT_BUFFER);
  lineno = lineno_stack[include_stack_ptr];
  conf_fbfile_in = inc_fbfile_in[include_stack_ptr];

  if(include_stack_ptr)
    current_file = conffile_stack[include_stack_ptr];
  else
    current_file = conffilebuf;

  yy_switch_to_buffer(include_stack[include_stack_ptr]);
  return 0;
}

/* #-comment style, look for #include */
#define INCLUDE "#include"

void hashcomment(void)
{
  if (strlen(yytext) < sizeof(INCLUDE) - 1)
    return;

  if (!rb_strncasecmp(yytext, INCLUDE, sizeof(INCLUDE) - 1))
      yyerror("You probably meant '.include', skipping");
}
