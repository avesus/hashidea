////////////////////////////////////////////////////////////////
// simple argument parsing with documentation

// options can be typed as int, char, char*, bool, double, or handled by a special function
// positional parameters are always string and must be listed in order
// a special type Rest means all rest are returned as a pointer

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "arg.h"

#define false 0
#define true (!false)
typedef unsigned int bool;

static int verbose = 0;
static char* commandLine;
static const char* pname;

// order is based on how enums are defined in ArgType
static char* type2fmt[] = {
  "",
  "<int>",
  "<char>",
  "<string>",
  "<bool>",
  "<double>",
  "",
  "",
  "" };
  
static void usage(const char* pname, ArgDefs* def);

static void
die(ArgDefs* def, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%s: Usage Error: ", pname);
    vfprintf(stderr, fmt, ap);
    va_end (ap);
    fprintf(stderr, "\n");
    usage(pname, def);
    exit(-1);
}


static const char*
arg2str(ArgOption* desc)
{
  static char buffer[128];
  char* p = buffer;
  char sep = (desc->kind == KindPositional) ? ':' : ' ';

  if (desc->kind == Help) {
    return "[-h]";
  }
  
  if ((desc->kind == KindPositional)&&(desc->required)) {
    *p++ = '<';
  } else if (!desc->required) {
    *p++ = '[';
  }

  switch(desc->type) {
  case Integer:
  case Character:
  case String:
  case Boolean:
  case Double:
    sprintf(p, "%s%c%s", desc->longarg, sep, type2fmt[desc->type]);
    p += strlen(p);
    break;
    
  case Function:
    sprintf(p, "%s%c%s", desc->longarg, sep, (*(argOptionParser)(desc->dest))(ArgGetDesc, NULL));
    p += strlen(p);
    break;
    
  case Help:
  case Toggle:
  case Set:
  case Increment:
    sprintf(p, "%s", desc->longarg);
    p += strlen(p);
    break;
    
  default:
    // new type that we didn't implement yet?
    fprintf(stderr, "%d\n", desc->type);
    assert(0);
  }
  if (desc->kind == KindRest) {
    strcpy(p, "...");
    p += strlen(p);
  }

  if ((desc->kind == KindPositional)&&(desc->required)) {
    *p++ = '>';
  } else if (!desc->required) {
    *p++ = ']';
  }
  *p = 0;
  return buffer;
}


static void 
usage(const char* pname, ArgDefs* def)
{
  fprintf(stderr, "%s: ", pname);
  // print out shorthand for arguments
  ArgOption* args = def->args;
  int i;
  while (args[i].kind != KindEnd) {
    fprintf(stderr, " %s", arg2str(args+i));
    i++;
  }
  fprintf(stderr, "\n%s\n", def->progdoc);

  // Now print individual descriptions
  i = 0;
  while (args[i].kind != KindEnd) {
    fprintf(stderr, "   %20s\t%s\t", arg2str(args+i), args[i].kind == KindHelp ? "Print this message" : args[i].desc);
    switch (args[i].type) {
    case Increment:
    case Integer:
      fprintf(stderr, "(default: %d)", *(int *)(args[i].dest));
      break;

    case Character:
      fprintf(stderr, "(default: %c)", *(char *)(args[i].dest));
      break;

    case String:
      fprintf(stderr, "(default: %s)", (char*)(args[i].dest));
      break;

    case Toggle:
    case Set:
    case Boolean:
      fprintf(stderr, "(default: %s)", *(int *)(args[i].dest) ? "true" : "false");
      break;

    case Double:
      fprintf(stderr, "(default: %lf)", *(double *)(args[i].dest));
      break;

    case Function:
      fprintf(stderr, "(default: %s)", (*(argOptionParser)(args[i].dest))(ArgGetDefault, NULL));
      break;

    case Help:
      break;
    }
    fprintf(stderr, "\n");
    i++;
  }  
}


void
makeCommandline(int argc, char** argv)
{
  int len = 2;
  for (int i=0; i<argc; i++) {
    len += (1 + strlen(argv[i]));
  }
  char* p = (char*)calloc(len, 1);
  len = 0;
  for (int i=0; i<argc; i++) {
    strcpy(p+len, argv[i]);
    len += strlen(argv[i]);
    p[len++] = ' ';
  }
  p[len-1] = 0;
  if (verbose) fprintf(stderr, "[%s]\n", p);
  commandLine = p;
}

static int
assignArg(ArgOption* desc, int argc, char** argv, ArgDefs* def)
{
  switch (desc->type) {
  case Increment:
    if (verbose) printf("incremement %s\n", desc->longarg);
    {
      int* p = (int*)desc->dest;
      (*p)++;
    }
    break;

  case Function:
    // call function with pointer to argv.  Expect # of argv consumed as return result.
    if (verbose) printf("calling function %s\n", desc->longarg);
    {
      argOptionParser aop = (argOptionParser)desc->dest;
      const char* ret = (*aop)(argc, argv);
      long int consumed = (long int)ret;
      return (int)consumed;
    }
    break;

  case String:
    if (verbose) printf("Saving %s to %s\n", argv[1], desc->longarg);
    {
      char** p = (char**)desc->dest;
      *p = argv[1];
      return 1;
    }
    break;

  case Set:
    if (verbose) printf("Setting %s to 1\n", desc->longarg);
    {
      int* p = (int*)desc->dest;
      *p = 1;
    }
    break;

  case Toggle:
    if (verbose) printf("toggeling %s\n", desc->longarg);
    {
      int* p = (int*)desc->dest;
      *p = !(*p);
    }
    break;

  case Double:
    if (verbose) printf("double -> [%s] = %lf\n", desc->longarg, atof(argv[1]));
    {
      double* p = (double*)desc->dest;
      *p = atof(argv[1]);
      return 1;
    }
    break;

  case Integer:
    if (verbose) printf("int -> [%s] = %d\n", desc->longarg, atoi(argv[1]));
    {
      int* p = (int*)desc->dest;
      *p = atoi(argv[1]);
      return 1;
    }
    break;

  case Help:
    usage(pname, def);
    exit(-1);

  default:
    fprintf(stderr, "NIY: type\n");
    exit(-1);
  }
  return 0;
}

static const char*
kind2str(ArgKind k)
{
  switch (k) {
  case KindEnd: return "End";
  case KindHelp: return "Help";
  case KindPositional: return "Positional";
  case KindOption: return "Option";
  case KindRest: return "Rest";
  default:
    assert(0);
  }
  return "Unknown";
}


void
checkArgDef(ArgDefs* def)
{
  // optional/help come before postional before rest
  int state = KindOption;
  ArgOption* desc = def->args;  
  int hashelp = 0;
  
  for (int i=0; desc[i].kind != KindEnd; i++) {
    if (desc[i].kind > KindEnd)
      die(def, "Bad kind - no KindEnd?");
    if (desc[i].kind != state) {
      if ((state == KindOption) && (desc[i].kind == KindHelp)&&(desc[i].longarg != NULL)) {
	hashelp = 1;
	continue;
      }
      if (desc[i].kind < state)
	die(def, "Bad order of arg defs: %s comes before last of %s", kind2str(state),  kind2str(desc[i].kind));
      state = desc[i].kind;
    }
  }
  if (!hashelp) die(def, "No help string");
}

int 
parseArgs(int argc, char** argv, ArgDefs* def)
{
  // get program name and commandline as a string
  pname = argv[0];
  makeCommandline(argc, argv);
  argv++; argc--;

  checkArgDef(def);

  // process args
  ArgOption* desc = def->args;  
  if (verbose) fprintf(stderr, "Processing args for %s: %d\n", pname, argc);
  int maxarg = argc;
  int i;
  for (i=0; i<argc; i++) {
    char* arg = argv[i];
    if (verbose) fprintf(stderr, "%d -> [%s]\n", i, arg);
    if ((arg[0] == '-')&&(desc[i].kind < KindPositional)) {
      // Handle options
      bool ok = false;
      for (int j=0; desc[j].kind != KindEnd; j++) {
	if (strcmp(desc[j].longarg, arg) == 0) {
	  // process it
	  ok = true;
	  int consumed = assignArg(desc+j, argc-i, argv+i, def);
	  i += consumed;
	}
      }
      if (!ok) 
	die(def, "Do not understand the flag [%s]\n", arg);
    } else {
      // No more options
      break;
    }
  }
  // ok, now we handle positional args, we handle them in the order they are declared
  int baseArg = i;
  int baseDestOffset;
  for (baseDestOffset=0; desc[baseDestOffset].kind != KindEnd; baseDestOffset++) {
    if ((desc[baseDestOffset].kind == KindPositional)||(desc[baseDestOffset].kind == KindRest)) break;
  }
  // base is first positional arg we are passed, j is first descriptor for positional arg
  if (verbose) 
    fprintf(stderr, "start pos: j=%d %s kind=%d\n", baseDestOffset, desc[baseDestOffset].longarg, desc[baseDestOffset].type);
  int j=0;			/* positional offset */
  while ( (desc[baseDestOffset+j].kind == KindPositional) && ((baseArg+j) < argc) ) {
    if (verbose) printf("%d: %s\n", j, baseArg+desc[baseDestOffset+j].longarg);
    int consumed = assignArg(desc+baseDestOffset+j, argc-(baseArg+j), argv+baseArg+j, def);
    j += consumed;
  }
  // check that we used all the arguments and don't have any extra
  if (desc[baseDestOffset+j].type == KindPositional)
    die(def, "Expected more arguments, only given %d", j);
  else if ((desc[baseDestOffset+j].type == KindEnd)&&((baseArg+j) < argc)) {
    die(def, "Too many arguments, given %d", j);
  }
  // see if we have a variable number of args at end
  if (desc[baseDestOffset+j].type == KindRest)
    die(def, "Haven't implemented Rest args yets");
  return 0;
}


