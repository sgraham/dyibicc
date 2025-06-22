// This file implements the C preprocessor.
//
// The preprocessor takes a list of tokens as an input and returns a
// new list of tokens as an output.
//
// The preprocessing language is designed in such a way that that's
// guaranteed to stop even if there is a recursive macro.
// Informally speaking, a macro is applied only once for each token.
// That is, if a macro token T appears in a result of direct or
// indirect macro expansion of T, T won't be expanded any further.
// For example, if T is defined as U, and U is defined as T, then
// token T is expanded to U and then to T and the macro expansion
// stops at that point.
//
// To achieve the above behavior, we attach for each token a set of
// macro names from which the token is expanded. The set is called
// "hideset". Hideset is initially empty, and every time we expand a
// macro, the macro name is added to the resulting tokens' hidesets.
//
// The above macro expansion algorithm is explained in this document
// written by Dave Prossor, which is used as a basis for the
// standard's wording:
// https://github.com/rui314/chibicc/wiki/cpp.algo.pdf

#include "dyibicc.h"

#include "compincl.h"

#define C(x) compiler_state.preprocess__##x

typedef struct MacroParam MacroParam;
struct MacroParam {
  MacroParam* next;
  char* name;
};

typedef struct MacroArg MacroArg;
struct MacroArg {
  MacroArg* next;
  char* name;
  bool is_va_args;
  Token* tok;
};

typedef struct Macro Macro;

typedef Token* macro_handler_fn(Macro* mac, Token*);

struct Macro {
  char* name;
  bool is_objlike;  // Object-like or function-like
  MacroParam* params;
  char* va_args_name;
  Token* body;
  macro_handler_fn* handler;
  bool handler_advances;
};

// `#if` can be nested, so we use a stack to manage nested `#if`s.
struct CondIncl {
  CondIncl* next;
  enum { IN_THEN, IN_ELIF, IN_ELSE } ctx;
  Token* tok;
  bool included;
};

typedef struct Hideset Hideset;
struct Hideset {
  Hideset* next;
  char* name;
};

static Token* preprocess2(Token* tok);
static Macro* find_macro(Token* tok);

static bool is_hash(Token* tok) {
  return tok->at_bol && equal(tok, "#");
}

// Some preprocessor directives such as #include allow extraneous
// tokens before newline. This function skips such tokens.
static Token* skip_line(Token* tok) {
  if (tok->at_bol)
    return tok;
  warn_tok(tok, "extra token");
  while (tok->at_bol)
    tok = tok->next;
  return tok;
}

static Token* copy_token(Token* tok) {
  Token* t = bumpcalloc(1, sizeof(Token), AL_Compile);
  *t = *tok;
  t->next = NULL;
  return t;
}

static Token* new_eof(Token* tok) {
  Token* t = copy_token(tok);
  t->kind = TK_EOF;
  t->len = 0;
  return t;
}

static Hideset* new_hideset(char* name) {
  Hideset* hs = bumpcalloc(1, sizeof(Hideset), AL_Compile);
  hs->name = name;
  return hs;
}

static Hideset* hideset_union(Hideset* hs1, Hideset* hs2) {
  Hideset head = {0};
  Hideset* cur = &head;

  for (; hs1; hs1 = hs1->next)
    cur = cur->next = new_hideset(hs1->name);
  cur->next = hs2;
  return head.next;
}

static bool hideset_contains(Hideset* hs, char* s, int len) {
  for (; hs; hs = hs->next)
    if (strlen(hs->name) == (size_t)len && !strncmp(hs->name, s, len))
      return true;
  return false;
}

static Hideset* hideset_intersection(Hideset* hs1, Hideset* hs2) {
  Hideset head = {0};
  Hideset* cur = &head;

  for (; hs1; hs1 = hs1->next)
    if (hideset_contains(hs2, hs1->name, (int)strlen(hs1->name)))
      cur = cur->next = new_hideset(hs1->name);
  return head.next;
}

static Token* add_hideset(Token* tok, Hideset* hs) {
  Token head = {0};
  Token* cur = &head;

  for (; tok; tok = tok->next) {
    Token* t = copy_token(tok);
    t->hideset = hideset_union(t->hideset, hs);
    cur = cur->next = t;
  }
  return head.next;
}

// Append tok2 to the end of tok1.
static Token* append(Token* tok1, Token* tok2) {
  if (tok1->kind == TK_EOF)
    return tok2;

  Token head = {0};
  Token* cur = &head;

  for (; tok1->kind != TK_EOF; tok1 = tok1->next)
    cur = cur->next = copy_token(tok1);
  cur->next = tok2;
  return head.next;
}

static Token* skip_cond_incl2(Token* tok) {
  while (tok->kind != TK_EOF) {
    if (is_hash(tok) &&
        (equal(tok->next, "if") || equal(tok->next, "ifdef") || equal(tok->next, "ifndef"))) {
      tok = skip_cond_incl2(tok->next->next);
      continue;
    }
    if (is_hash(tok) && equal(tok->next, "endif"))
      return tok->next->next;
    tok = tok->next;
  }
  return tok;
}

// Skip until next `#else`, `#elif` or `#endif`.
// Nested `#if` and `#endif` are skipped.
static Token* skip_cond_incl(Token* tok) {
  while (tok->kind != TK_EOF) {
    if (is_hash(tok) &&
        (equal(tok->next, "if") || equal(tok->next, "ifdef") || equal(tok->next, "ifndef"))) {
      tok = skip_cond_incl2(tok->next->next);
      continue;
    }

    if (is_hash(tok) &&
        (equal(tok->next, "elif") || equal(tok->next, "else") || equal(tok->next, "endif")))
      break;
    tok = tok->next;
  }
  return tok;
}

// Double-quote a given string and returns it.
static char* quote_string(char* str) {
  int bufsize = 3;
  for (int i = 0; str[i]; i++) {
    if (str[i] == '\\' || str[i] == '"')
      bufsize++;
    bufsize++;
  }

  char* buf = bumpcalloc(1, bufsize, AL_Compile);
  char* p = buf;
  *p++ = '"';
  for (int i = 0; str[i]; i++) {
    if (str[i] == '\\' || str[i] == '"')
      *p++ = '\\';
    *p++ = str[i];
  }
  *p++ = '"';
  *p++ = '\0';
  return buf;
}

static Token* new_str_token(char* str, Token* tmpl) {
  char* buf = quote_string(str);
  return tokenize(new_file(tmpl->file->name, buf));
}

// Copy all tokens until the next newline, terminate them with
// an EOF token and then returns them. This function is used to
// create a new list of tokens for `#if` arguments.
static Token* copy_line(Token** rest, Token* tok) {
  Token head = {0};
  Token* cur = &head;

  for (; tok->kind != TK_EOF && !tok->at_bol; tok = tok->next)
    cur = cur->next = copy_token(tok);

  cur->next = new_eof(tok);
  *rest = tok;
  return head.next;
}

static Token* new_num_token(int val, Token* tmpl) {
  char* buf = format(AL_Compile, "%d\n", val);
  return tokenize(new_file(tmpl->file->name, buf));
}

static Token* read_const_expr(Token** rest, Token* tok) {
  tok = copy_line(rest, tok);

  Token head = {0};
  Token* cur = &head;

  while (tok->kind != TK_EOF) {
    // "defined(foo)" or "defined foo" becomes "1" if macro "foo"
    // is defined. Otherwise "0".
    if (equal(tok, "defined")) {
      Token* start = tok;
      bool has_paren = consume(&tok, tok->next, "(");

      if (tok->kind != TK_IDENT)
        error_tok(start, "macro name must be an identifier");
      Macro* m = find_macro(tok);
      tok = tok->next;

      if (has_paren)
        tok = skip(tok, ")");

      cur = cur->next = new_num_token(m ? 1 : 0, start);
      continue;
    }

    cur = cur->next = tok;
    tok = tok->next;
  }

  cur->next = tok;
  return head.next;
}

// Read and evaluate a constant expression.
static long eval_const_expr(Token** rest, Token* tok) {
  Token* start = tok;
  Token* expr = read_const_expr(rest, tok->next);
  expr = preprocess2(expr);

  if (expr->kind == TK_EOF)
    error_tok(start, "no expression");

  // [https://www.sigbus.info/n1570#6.10.1p4] The standard requires
  // we replace remaining non-macro identifiers with "0" before
  // evaluating a constant expression. For example, `#if foo` is
  // equivalent to `#if 0` if foo is not defined.
  for (Token* t = expr; t->kind != TK_EOF; t = t->next) {
    if (t->kind == TK_IDENT) {
      Token* next = t->next;
      *t = *new_num_token(0, t);
      t->next = next;
    }
  }

  // Convert pp-numbers to regular numbers
  convert_pp_tokens(expr);

  Token* rest2;
  long val = (long)pp_const_expr(&rest2, expr);
  if (rest2->kind != TK_EOF)
    error_tok(rest2, "extra token");
  return val;
}

static CondIncl* push_cond_incl(Token* tok, bool included) {
  CondIncl* ci = bumpcalloc(1, sizeof(CondIncl), AL_Compile);
  ci->next = C(cond_incl);
  ci->ctx = IN_THEN;
  ci->tok = tok;
  ci->included = included;
  C(cond_incl) = ci;
  return ci;
}

static Macro* find_macro(Token* tok) {
  if (tok->kind != TK_IDENT)
    return NULL;
  return hashmap_get2(&C(macros), tok->loc, tok->len);
}

static Macro* add_macro(char* name, bool is_objlike, Token* body) {
  Macro* m = bumpcalloc(1, sizeof(Macro), AL_Compile);
  m->name = name;
  m->is_objlike = is_objlike;
  m->body = body;
  hashmap_put(&C(macros), name, m);
  return m;
}

static MacroParam* read_macro_params(Token** rest, Token* tok, char** va_args_name) {
  MacroParam head = {0};
  MacroParam* cur = &head;

  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");

    if (equal(tok, "...")) {
      *va_args_name = "__VA_ARGS__";
      *rest = skip(tok->next, ")");
      return head.next;
    }

    if (tok->kind != TK_IDENT)
      error_tok(tok, "expected an identifier");

    if (equal(tok->next, "...")) {
      *va_args_name = bumpstrndup(tok->loc, tok->len, AL_Compile);
      *rest = skip(tok->next->next, ")");
      return head.next;
    }

    MacroParam* m = bumpcalloc(1, sizeof(MacroParam), AL_Compile);
    m->name = bumpstrndup(tok->loc, tok->len, AL_Compile);
    cur = cur->next = m;
    tok = tok->next;
  }

  *rest = tok->next;
  return head.next;
}

static Macro* read_macro_definition(Token** rest, Token* tok) {
  if (tok->kind != TK_IDENT)
    error_tok(tok, "macro name must be an identifier");
  char* name = bumpstrndup(tok->loc, tok->len, AL_Compile);
  tok = tok->next;

  if (!tok->has_space && equal(tok, "(")) {
    // Function-like macro
    char* va_args_name = NULL;
    MacroParam* params = read_macro_params(&tok, tok->next, &va_args_name);

    Macro* m = add_macro(name, false, copy_line(rest, tok));
    m->params = params;
    m->va_args_name = va_args_name;
    return m;
  } else {
    // Object-like macro
    return add_macro(name, true, copy_line(rest, tok));
  }
}

static MacroArg* read_macro_arg_one(Token** rest, Token* tok, bool read_rest) {
  Token head = {0};
  Token* cur = &head;
  int level = 0;

  for (;;) {
    if (level == 0 && equal(tok, ")"))
      break;
    if (level == 0 && !read_rest && equal(tok, ","))
      break;

    if (tok->kind == TK_EOF)
      error_tok(tok, "premature end of input");

    if (equal(tok, "("))
      level++;
    else if (equal(tok, ")"))
      level--;

    cur = cur->next = copy_token(tok);
    tok = tok->next;
  }

  cur->next = new_eof(tok);

  MacroArg* arg = bumpcalloc(1, sizeof(MacroArg), AL_Compile);
  arg->tok = head.next;
  *rest = tok;
  return arg;
}

static MacroArg* read_macro_args(Token** rest, Token* tok, MacroParam* params, char* va_args_name) {
  Token* start = tok;
  tok = tok->next->next;

  MacroArg head = {0};
  MacroArg* cur = &head;

  MacroParam* pp = params;
  for (; pp; pp = pp->next) {
    if (cur != &head)
      tok = skip(tok, ",");
    cur = cur->next = read_macro_arg_one(&tok, tok, false);
    cur->name = pp->name;
  }

  if (va_args_name) {
    MacroArg* arg;
    if (equal(tok, ")")) {
      arg = bumpcalloc(1, sizeof(MacroArg), AL_Compile);
      arg->tok = new_eof(tok);
    } else {
      if (pp != params)
        tok = skip(tok, ",");
      arg = read_macro_arg_one(&tok, tok, true);
    }
    arg->name = va_args_name;
    ;
    arg->is_va_args = true;
    cur = cur->next = arg;
  } else if (pp) {
    error_tok(start, "too many arguments");
  }

  skip(tok, ")");
  *rest = tok;
  return head.next;
}

static MacroArg* find_arg(MacroArg* args, Token* tok) {
  for (MacroArg* ap = args; ap; ap = ap->next)
    if ((size_t)tok->len == strlen(ap->name) && !strncmp(tok->loc, ap->name, tok->len))
      return ap;
  return NULL;
}

// Concatenates all tokens in `tok` and returns a new string.
static char* join_tokens(Token* tok, Token* end) {
  // Compute the length of the resulting token.
  int len = 1;
  for (Token* t = tok; t != end && t->kind != TK_EOF; t = t->next) {
    if (t != tok && t->has_space)
      len++;
    len += t->len;
  }

  char* buf = bumpcalloc(1, len, AL_Compile);

  // Copy token texts.
  int pos = 0;
  for (Token* t = tok; t != end && t->kind != TK_EOF; t = t->next) {
    if (t != tok && t->has_space)
      buf[pos++] = ' ';
    strncpy(buf + pos, t->loc, t->len);
    pos += t->len;
  }
  buf[pos] = '\0';
  return buf;
}

// Concatenates all tokens in `arg` and returns a new string token.
// This function is used for the stringizing operator (#).
static Token* stringize(Token* hash, Token* arg) {
  // Create a new string token. We need to set some value to its
  // source location for error reporting function, so we use a macro
  // name token as a template.
  char* s = join_tokens(arg, NULL);
  return new_str_token(s, hash);
}

// Concatenate two tokens to create a new token.
static Token* paste(Token* lhs, Token* rhs) {
  // Paste the two tokens.
  char* buf = format(AL_Compile, "%.*s%.*s", lhs->len, lhs->loc, rhs->len, rhs->loc);

  // Tokenize the resulting string.
  Token* tok = tokenize(new_file(lhs->file->name, buf));
  if (tok->next->kind != TK_EOF)
    error_tok(lhs, "pasting forms '%s', an invalid token", buf);
  return tok;
}

static bool has_varargs(MacroArg* args) {
  for (MacroArg* ap = args; ap; ap = ap->next)
    if (!strcmp(ap->name, "__VA_ARGS__"))
      return ap->tok->kind != TK_EOF;
  return false;
}

// Replace func-like macro parameters with given arguments.
static Token* subst(Token* tok, MacroArg* args) {
  Token head = {0};
  Token* cur = &head;

  while (tok->kind != TK_EOF) {
    // "#" followed by a parameter is replaced with stringized actuals.
    if (equal(tok, "#")) {
      MacroArg* arg = find_arg(args, tok->next);
      if (!arg)
        error_tok(tok->next, "'#' is not followed by a macro parameter");
      cur = cur->next = stringize(tok, arg->tok);
      tok = tok->next->next;
      continue;
    }

    // [GNU] If __VA_ARG__ is empty, `,##__VA_ARGS__` is expanded
    // to the empty token list. Otherwise, its expaned to `,` and
    // __VA_ARGS__.
    if (equal(tok, ",") && equal(tok->next, "##")) {
      MacroArg* arg = find_arg(args, tok->next->next);
      if (arg && arg->is_va_args) {
        if (arg->tok->kind == TK_EOF) {
          tok = tok->next->next->next;
        } else {
          cur = cur->next = copy_token(tok);
          tok = tok->next->next;
        }
        continue;
      }
    }

    if (equal(tok, "##")) {
      if (cur == &head)
        error_tok(tok, "'##' cannot appear at start of macro expansion");

      if (tok->next->kind == TK_EOF)
        error_tok(tok, "'##' cannot appear at end of macro expansion");

      MacroArg* arg = find_arg(args, tok->next);
      if (arg) {
        if (arg->tok->kind != TK_EOF) {
          *cur = *paste(cur, arg->tok);
          for (Token* t = arg->tok->next; t->kind != TK_EOF; t = t->next)
            cur = cur->next = copy_token(t);
        }
        tok = tok->next->next;
        continue;
      }

      *cur = *paste(cur, tok->next);
      tok = tok->next->next;
      continue;
    }

    MacroArg* arg = find_arg(args, tok);

    if (arg && equal(tok->next, "##")) {
      Token* rhs = tok->next->next;

      if (arg->tok->kind == TK_EOF) {
        MacroArg* arg2 = find_arg(args, rhs);
        if (arg2) {
          for (Token* t = arg2->tok; t->kind != TK_EOF; t = t->next)
            cur = cur->next = copy_token(t);
        } else {
          cur = cur->next = copy_token(rhs);
        }
        tok = rhs->next;
        continue;
      }

      for (Token* t = arg->tok; t->kind != TK_EOF; t = t->next)
        cur = cur->next = copy_token(t);
      tok = tok->next;
      continue;
    }

    // If __VA_ARG__ is empty, __VA_OPT__(x) is expanded to the
    // empty token list. Otherwise, __VA_OPT__(x) is expanded to x.
    if (equal(tok, "__VA_OPT__") && equal(tok->next, "(")) {
      MacroArg* arg2 = read_macro_arg_one(&tok, tok->next->next, true);
      if (has_varargs(args))
        for (Token* t = arg2->tok; t->kind != TK_EOF; t = t->next)
          cur = cur->next = t;
      tok = skip(tok, ")");
      continue;
    }

    // Handle a macro token. Macro arguments are completely macro-expanded
    // before they are substituted into a macro body.
    if (arg) {
      Token* t = preprocess2(arg->tok);
      t->at_bol = tok->at_bol;
      t->has_space = tok->has_space;
      for (; t->kind != TK_EOF; t = t->next)
        cur = cur->next = copy_token(t);
      tok = tok->next;
      continue;
    }

    // Handle a non-macro token.
    cur = cur->next = copy_token(tok);
    tok = tok->next;
    continue;
  }

  cur->next = tok;
  return head.next;
}

static bool file_exists_in_builtins(char* path) {
  if (C(builtin_includes_map).capacity == 0) {
    for (size_t i = 0; i < sizeof(compiler_includes) / sizeof(compiler_includes[0]); ++i) {
      hashmap_put(&C(builtin_includes_map), compiler_includes[i].path,
                  (void*)&compiler_include_blob[compiler_includes[i].offset]);
    }
  }

  return hashmap_get(&C(builtin_includes_map), path) != NULL;
}

static bool file_exists(char* path) {
  struct stat st;

  if (file_exists_in_builtins(path))
    return true;

  return !stat(path, &st);
}

// If tok is a macro, expand it and return true.
// Otherwise, do nothing and return false.
static bool expand_macro(Token** rest, Token* tok) {
  if (hideset_contains(tok->hideset, tok->loc, tok->len))
    return false;

  Macro* m = find_macro(tok);
  if (!m)
    return false;

  // Built-in dynamic macro application such as __LINE__
  if (m->handler) {
    *rest = m->handler(m, tok);
    if (!m->handler_advances) {
      (*rest)->next = tok->next;
    }
    return true;
  }

  // Object-like macro application
  if (m->is_objlike) {
    Hideset* hs = hideset_union(tok->hideset, new_hideset(m->name));
    Token* body = add_hideset(m->body, hs);
    for (Token* t = body; t->kind != TK_EOF; t = t->next)
      t->origin = tok;
    *rest = append(body, tok->next);
    (*rest)->at_bol = tok->at_bol;
    (*rest)->has_space = tok->has_space;
    return true;
  }

  // If a funclike macro token is not followed by an argument list,
  // treat it as a normal identifier.
  if (!equal(tok->next, "("))
    return false;

  // Function-like macro application
  Token* macro_token = tok;
  MacroArg* args = read_macro_args(&tok, tok, m->params, m->va_args_name);
  Token* rparen = tok;

  // Tokens that consist a func-like macro invocation may have different
  // hidesets, and if that's the case, it's not clear what the hideset
  // for the new tokens should be. We take the interesection of the
  // macro token and the closing parenthesis and use it as a new hideset
  // as explained in the Dave Prossor's algorithm.
  Hideset* hs = hideset_intersection(macro_token->hideset, rparen->hideset);
  hs = hideset_union(hs, new_hideset(m->name));

  Token* body = subst(m->body, args);
  body = add_hideset(body, hs);
  for (Token* t = body; t->kind != TK_EOF; t = t->next)
    t->origin = macro_token;
  *rest = append(body, tok->next);
  (*rest)->at_bol = macro_token->at_bol;
  (*rest)->has_space = macro_token->has_space;
  return true;
}

IMPLSTATIC char* search_include_paths(char* filename) {
  if (filename[0] == '/')
    return filename;

  char* cached = hashmap_get(&C(include_path_cache), filename);
  if (cached)
    return cached;

  // Search a file from the include paths.
  for (int i = 0; i < (int)user_context->num_include_paths; i++) {
    char* path = format(AL_Compile, "%s/%s", user_context->include_paths[i], filename);
    if (!file_exists(path))
      continue;
    hashmap_put(&C(include_path_cache), filename, path);
    C(include_next_idx) = i + 1;
    return path;
  }
  return NULL;
}

static char* search_include_next(char* filename) {
  for (; C(include_next_idx) < (int)user_context->num_include_paths; C(include_next_idx)++) {
    char* path =
        format(AL_Compile, "%s/%s", user_context->include_paths[C(include_next_idx)], filename);
    if (file_exists(path))
      return path;
  }
  return NULL;
}

// Read an #include argument.
static char* read_include_filename(Token** rest, Token* tok, bool* is_dquote, bool can_expand) {
  // Pattern 1: #include "foo.h"
  if (tok->kind == TK_STR) {
    // A double-quoted filename for #include is a special kind of
    // token, and we don't want to interpret any escape sequences in it.
    // For example, "\f" in "C:\foo" is not a formfeed character but
    // just two non-control characters, backslash and f.
    // So we don't want to use token->str.
    *is_dquote = true;
    *rest = skip_line(tok->next);
    return bumpstrndup(tok->loc + 1, tok->len - 2, AL_Compile);
  }

  // Pattern 2: #include <foo.h>
  if (equal(tok, "<")) {
    // Reconstruct a filename from a sequence of tokens between
    // "<" and ">".
    Token* start = tok;

    // Find closing ">".
    for (; !equal(tok, ">"); tok = tok->next)
      if (tok->at_bol || tok->kind == TK_EOF)
        error_tok(tok, "expected '>'");

    *is_dquote = false;
    *rest = skip_line(tok->next);
    return join_tokens(start->next, tok);
  }

  // Pattern 3: #include FOO
  // In this case FOO must be macro-expanded to either
  // a single string token or a sequence of "<" ... ">".
  if (can_expand && tok->kind == TK_IDENT) {
    Token* tok2 = preprocess2(copy_line(rest, tok));
    return read_include_filename(&tok2, tok2, is_dquote, /*can_expand=*/false);
  }

  error_tok(tok, "expected a filename");
}

// Detect the following "include guard" pattern.
//
//   #ifndef FOO_H
//   #define FOO_H
//   ...
//   #endif
static char* detect_include_guard(Token* tok) {
  // Detect the first two lines.
  if (!is_hash(tok) || !equal(tok->next, "ifndef"))
    return NULL;
  tok = tok->next->next;

  if (tok->kind != TK_IDENT)
    return NULL;

  char* macro = bumpstrndup(tok->loc, tok->len, AL_Compile);
  tok = tok->next;

  if (!is_hash(tok) || !equal(tok->next, "define") || !equal(tok->next->next, macro))
    return NULL;

  // Read until the end of the file.
  while (tok->kind != TK_EOF) {
    if (!is_hash(tok)) {
      tok = tok->next;
      continue;
    }

    if (equal(tok->next, "endif") && tok->next->next->kind == TK_EOF)
      return macro;

    if (equal(tok, "if") || equal(tok, "ifdef") || equal(tok, "ifndef"))
      tok = skip_cond_incl(tok->next);
    else
      tok = tok->next;
  }
  return NULL;
}

static Token* include_file(Token* tok, char* path, Token* filename_tok) {
  // Check for "#pragma once"
  if (hashmap_get(&C(pragma_once), path))
    return tok;

  // If we read the same file before, and if the file was guarded
  // by the usual #ifndef ... #endif pattern, we may be able to
  // skip the file without opening it.
  char* guard_name = hashmap_get(&C(include_guards), path);
  if (guard_name && hashmap_get(&C(macros), guard_name))
    return tok;

  Token* tok2;
  char* builtin_include_contents = hashmap_get(&C(builtin_includes_map), path);
  if (builtin_include_contents) {
    tok2 = tokenize_filecontents(path, builtin_include_contents);
  } else {
    tok2 = tokenize_file(path);
  }
  if (!tok2)
    error_tok(filename_tok, "%s: cannot open file: %s", path, strerror(errno));

  guard_name = detect_include_guard(tok2);
  if (guard_name)
    hashmap_put(&C(include_guards), path, guard_name);

  return append(tok2, tok);
}

// Read #line arguments
static void read_line_marker(Token** rest, Token* tok) {
  Token* start = tok;
  tok = preprocess2(copy_line(rest, tok));
  convert_pp_tokens(tok);

  if (tok->kind != TK_NUM || tok->ty->kind != TY_INT)
    error_tok(tok, "invalid line marker");
  start->file->line_delta = (int)(tok->val - start->line_no);

  tok = tok->next;
  if (tok->kind == TK_EOF)
    return;

  if (tok->kind != TK_STR)
    error_tok(tok, "filename expected");
  start->file->display_name = tok->str;
}

// Visit all tokens in `tok` while evaluating preprocessing
// macros and directives.
static Token* preprocess2(Token* tok) {
  Token head = {0};
  Token* cur = &head;

  while (tok->kind != TK_EOF) {
    // If it is a macro, expand it.
    if (expand_macro(&tok, tok))
      continue;

    // Pass through if it is not a "#".
    if (!is_hash(tok)) {
      tok->line_delta = tok->file->line_delta;
      tok->filename = tok->file->display_name;
      cur = cur->next = tok;
      tok = tok->next;
      continue;
    }

    Token* start = tok;
    tok = tok->next;

    if (equal(tok, "include")) {
      bool is_dquote;
      char* filename = read_include_filename(&tok, tok->next, &is_dquote, true);

      if (filename[0] != '/' && is_dquote) {
        char* path = format(AL_Compile, "%s/%s", dirname(bumpstrdup(start->file->name, AL_Compile)),
                            filename);
        if (file_exists(path)) {
          tok = include_file(tok, path, start->next->next);
          continue;
        }
      }

      char* path = search_include_paths(filename);
      tok = include_file(tok, path ? path : filename, start->next->next);
      continue;
    }

    if (equal(tok, "include_next")) {
      bool ignore;
      char* filename = read_include_filename(&tok, tok->next, &ignore, true);
      char* path = search_include_next(filename);
      tok = include_file(tok, path ? path : filename, start->next->next);
      continue;
    }

    if (equal(tok, "define")) {
      read_macro_definition(&tok, tok->next);
      continue;
    }

    if (equal(tok, "undef")) {
      tok = tok->next;
      if (tok->kind != TK_IDENT)
        error_tok(tok, "macro name must be an identifier");
      undef_macro(bumpstrndup(tok->loc, tok->len, AL_Compile));
      tok = skip_line(tok->next);
      continue;
    }

    if (equal(tok, "if")) {
      long val = eval_const_expr(&tok, tok);
      push_cond_incl(start, val);
      if (!val)
        tok = skip_cond_incl(tok);
      continue;
    }

    if (equal(tok, "ifdef")) {
      bool defined = find_macro(tok->next);
      push_cond_incl(tok, defined);
      if (tok->next->kind == TK_EOF) {
        error_tok(tok, "unterminated #ifdef");
      }
      tok = skip_line(tok->next->next);
      if (!defined)
        tok = skip_cond_incl(tok);
      continue;
    }

    if (equal(tok, "ifndef")) {
      bool defined = find_macro(tok->next);
      push_cond_incl(tok, !defined);
      if (tok->next->kind == TK_EOF) {
        error_tok(tok, "unterminated #ifndef");
      }
      tok = skip_line(tok->next->next);
      if (defined)
        tok = skip_cond_incl(tok);
      continue;
    }

    if (equal(tok, "elif")) {
      if (!C(cond_incl) || C(cond_incl)->ctx == IN_ELSE)
        error_tok(start, "stray #elif");
      C(cond_incl)->ctx = IN_ELIF;

      if (!C(cond_incl)->included && eval_const_expr(&tok, tok))
        C(cond_incl)->included = true;
      else
        tok = skip_cond_incl(tok);
      continue;
    }

    if (equal(tok, "else")) {
      if (!C(cond_incl) || C(cond_incl)->ctx == IN_ELSE)
        error_tok(start, "stray #else");
      C(cond_incl)->ctx = IN_ELSE;
      tok = skip_line(tok->next);

      if (C(cond_incl)->included)
        tok = skip_cond_incl(tok);
      continue;
    }

    if (equal(tok, "endif")) {
      if (!C(cond_incl))
        error_tok(start, "stray #endif");
      C(cond_incl) = C(cond_incl)->next;
      tok = skip_line(tok->next);
      continue;
    }

    if (equal(tok, "line")) {
      read_line_marker(&tok, tok->next);
      continue;
    }

    if (tok->kind == TK_PP_NUM) {
      read_line_marker(&tok, tok);
      continue;
    }

    if (equal(tok, "pragma") && equal(tok->next, "once")) {
      hashmap_put(&C(pragma_once), tok->file->name, (void*)1);
      tok = skip_line(tok->next->next);
      continue;
    }

    if (equal(tok, "pragma")) {
      do {
        tok = tok->next;
      } while (!tok->at_bol);
      continue;
    }

    if (equal(tok, "error"))
      error_tok(tok, "error");

    // `#`-only line is legal. It's called a null directive.
    if (tok->at_bol)
      continue;

    error_tok(tok, "invalid preprocessor directive");
  }

  cur->next = tok;
  return head.next;
}

IMPLSTATIC void define_macro(char* name, char* buf) {
  Token* tok = tokenize(new_file("<built-in>", buf));
  add_macro(name, true, tok);
}

IMPLSTATIC void undef_macro(char* name) {
  hashmap_delete(&C(macros), name);
}

IMPLSTATIC void define_function_macro(char* buf, macro_handler_fn* fn) {
  Token* tok = tokenize(new_file("<built-in>", buf));
  Token* rest = tok;
  Macro* m = read_macro_definition(&rest, tok);
  m->handler = fn;
  m->handler_advances = true;
}

static Macro* add_builtin(char* name, macro_handler_fn* fn) {
  Macro* m = add_macro(name, true, NULL);
  m->handler = fn;
  return m;
}

static Token* file_macro(Macro* m, Token* tmpl) {
  (void)m;
  while (tmpl->origin)
    tmpl = tmpl->origin;
  return new_str_token(tmpl->file->display_name, tmpl);
}

static Token* line_macro(Macro* m, Token* tmpl) {
  (void)m;
  while (tmpl->origin)
    tmpl = tmpl->origin;
  int i = tmpl->line_no + tmpl->file->line_delta;
  return new_num_token(i, tmpl);
}

// __COUNTER__ is expanded to serial values starting from 0.
static Token* counter_macro(Macro* m, Token* tmpl) {
  (void)m;
  return new_num_token(C(counter_macro_i)++, tmpl);
}

// __TIMESTAMP__ is expanded to a string describing the last
// modification time of the current file. E.g.
// "Fri Jul 24 01:32:50 2020"
static Token* timestamp_macro(Macro* m, Token* tmpl) {
  (void)m;
  return new_str_token("Mon May 02 01:23:45 1977", tmpl);
}

static Token* base_file_macro(Macro* m, Token* tmpl) {
  (void)m;
  return new_str_token(compiler_state.main__base_file, tmpl);
}

// __DATE__ is expanded to the current date, e.g. "May 17 2020".
static char* format_date(struct tm* tm) {
  (void)tm;
  return "\"May 02 1977\"";
}

// __TIME__ is expanded to the current time, e.g. "13:34:03".
static char* format_time(struct tm* tm) {
  (void)tm;
  return "\"01:23:45\"";
}

static void append_to_container_tokens(Token* to_add) {
  // Must be maintained in order that the files were instantiated because (e.g.)
  // header guards will cause different structs to be defined, and they might be
  // later referenced by a reinclude.
  tokenptrarray_push(&C(container_tokens), to_add, AL_Compile);
}

static char* format_container_type_as_ident(Token* ma) {
  if (ma->kind == TK_EOF) {
    return "";
  } else if (ma->kind == TK_IDENT) {
    return format(AL_Compile, "%.*s%s", ma->len, ma->loc, format_container_type_as_ident(ma->next));
  } else if (ma->kind == TK_PUNCT) {
    assert(ma->loc[0] == '*' && ma->len == 1);
    return format(AL_Compile, "$STAR$%s", format_container_type_as_ident(ma->next));
  } else {
    unreachable();
  }
}

static char* format_container_type_as_template_arg(Token* ma) {
  if (ma->kind == TK_EOF) {
    return "";
  } else if (ma->kind == TK_IDENT) {
    return format(AL_Compile, "%.*s%s", ma->len, ma->loc,
                  format_container_type_as_template_arg(ma->next));
  } else if (ma->kind == TK_PUNCT) {
    assert(ma->loc[0] == '*' && ma->len == 1);
    return format(AL_Compile, "*%s", format_container_type_as_template_arg(ma->next));
  } else {
    unreachable();
  }
}

static Token* container_vec_setup(Macro* m, Token* tok) {
  Token* rparen;
  MacroArg* args = read_macro_args(&rparen, tok, m->params, m->va_args_name);

  char* key_as_arg = format_container_type_as_template_arg(args->tok);
  char* key_as_ident = format_container_type_as_ident(args->tok);

  char* key = format(AL_Compile, "type:vec,arg:%s", key_as_ident);
  if (!hashmap_get(&C(container_included), key)) {
    append_to_container_tokens(preprocess(
        tokenize(new_file(tok->file->name, format(AL_Compile,
                                                  "#define __dyibicc_internal_include__ 1\n"
                                                  "#define i_key %s\n"
                                                  "#define i_type _Vec$%s\n"
                                                  "#include <_vec.h>\n"
                                                  "#undef __dyibicc_internal_include__\n",
                                                  key_as_arg, key_as_ident)))));

    hashmap_put(&C(container_included), key, (void*)1);
  }

  Token* ret = tokenize(new_file(tok->file->name, format(AL_Compile, "_Vec$%s", key_as_ident)));
  ret->next = rparen->next;
  return ret;
}

static Token* container_map_setup(Macro* m, Token* tok) {
  Token* rparen;
  MacroArg* args = read_macro_args(&rparen, tok, m->params, m->va_args_name);

  char* key_as_arg = format_container_type_as_template_arg(args->tok);
  char* key_as_ident = format_container_type_as_ident(args->tok);
  char* val_as_arg = format_container_type_as_template_arg(args->next->tok);
  char* val_as_ident = format_container_type_as_ident(args->next->tok);

  char* key = format(AL_Compile, "type:map,arg:%s,arg:%s", key_as_ident, val_as_ident);

  if (!hashmap_get(&C(container_included), key)) {
    append_to_container_tokens(preprocess(tokenize(
        new_file(tok->file->name, format(AL_Compile,
                                         "#define __dyibicc_internal_include__ 1\n"
                                         "#define i_key %s\n"
                                         "#define i_val %s\n"
                                         "#define i_type _Map$%s$%s\n"
                                         "#include <_map.h>\n"
                                         "#undef __dyibicc_internal_include__\n",
                                         key_as_arg, val_as_arg, key_as_ident, val_as_ident)))));

    hashmap_put(&C(container_included), key, (void*)1);
  }

  Token* ret = tokenize(
      new_file(tok->file->name, format(AL_Compile, "_Map$%s$%s", key_as_ident, val_as_ident)));
  ret->next = rparen->next;
  return ret;
}

IMPLSTATIC void init_macros(void) {
  // Define predefined macros
  define_macro("_LP64", "1");
  define_macro("__C99_MACRO_WITH_VA_ARGS", "1");
  define_macro("__LP64__", "1");
  define_macro("__SIZEOF_DOUBLE__", "8");
  define_macro("__SIZEOF_FLOAT__", "4");
  define_macro("__SIZEOF_INT__", "4");
  define_macro("__SIZEOF_LONG_LONG__", "8");
  define_macro("__SIZEOF_POINTER__", "8");
  define_macro("__SIZEOF_PTRDIFF_T__", "8");
  define_macro("__SIZEOF_SHORT__", "2");
  define_macro("__SIZEOF_SIZE_T__", "8");
  define_macro("__SIZE_TYPE__", "unsigned long");
  define_macro("__STDC_HOSTED__", "1");
  define_macro("__STDC_NO_COMPLEX__", "1");
  define_macro("__STDC_UTF_16__", "1");
  define_macro("__STDC_UTF_32__", "1");
  define_macro("__STDC_VERSION__", "201112L");
  define_macro("__STDC__", "1");
  define_macro("__USER_LABEL_PREFIX__", "");
  define_macro("__alignof__", "_Alignof");
  define_macro("__amd64", "1");
  define_macro("__amd64__", "1");
  define_macro("__const__", "const");
  define_macro("__dyibicc__", "1");
  define_macro("__inline__", "inline");
  define_macro("__signed__", "signed");
  define_macro("__typeof__", "typeof");
  define_macro("__volatile__", "volatile");
  define_macro("__x86_64", "1");
  define_macro("__x86_64__", "1");
#if X64WIN
  define_macro("__SIZEOF_LONG__", "4");
  define_macro("__SIZEOF_LONG_DOUBLE__", "8");
  define_macro("__cdecl", "");
  define_macro("__stdcall", "");
  define_macro("__inline", "");
  define_macro("__forceinline", "");
  define_macro("__unaligned", "");
  define_macro("__alignof", "_Alignof");
  define_macro("__int8", "char");
  define_macro("__int16", "short");
  define_macro("__int32", "int");
  define_macro("__ptr32", "");  // Possibly wrong and needs to truncate?
  define_macro("__ptr64", "");
  define_macro("_M_X64", "100");
  define_macro("_M_AMD64", "100");
  define_macro("_AMD64_", "1");
  define_macro("_WIN32", "1");
  define_macro("WIN32", "1");
  define_macro("_WIN64", "1");
  // Without this, structs like OVERLAPPED don't use anon unions, so most user
  // code will break.
  define_macro("_MSC_EXTENSIONS", "1");
  // VS2008, arbitrarily. Has to be defined to something as a lot of code uses
  // it to indicate "is_windows", and 2008 was one of my favourites.
  define_macro("_MSC_VER", "1500");
  define_macro("_NO_CRT_STDIO_INLINE", "1");
  define_macro("_CRT_DECLARE_NONSTDC_NAMES", "1");
  define_macro("__WINT_TYPE__", "unsigned short");
  define_function_macro("__pragma(_)\n", NULL);
  define_function_macro("__declspec(_)\n", NULL);
#elif defined(__APPLE__)
  define_macro("__SIZEOF_LONG__", "8");
  define_macro("__SIZEOF_LONG_DOUBLE__", "16");
  define_macro("__APPLE__", "1");
  define_macro("__GNUC__", "4");
  define_macro("__GNUC_MINOR__", "2");
  define_macro("__GNUC_PATCHLEVEL__", "1");
  define_function_macro("__asm(_)\n", NULL);
  define_function_macro("__asm__(_)\n", NULL);
#else
  define_macro("__SIZEOF_LONG__", "8");
  define_macro("__SIZEOF_LONG_DOUBLE__", "16");
  define_macro("__ELF__", "1");
  define_macro("linux", "1");
  define_macro("unix", "1");
  define_macro("__unix", "1");
  define_macro("__unix__", "1");
  define_macro("__linux", "1");
  define_macro("__linux__", "1");
  define_macro("__gnu_linux__", "1");
#endif

  add_builtin("__FILE__", file_macro);
  add_builtin("__LINE__", line_macro);
  add_builtin("__COUNTER__", counter_macro);
  add_builtin("__TIMESTAMP__", timestamp_macro);
  add_builtin("__BASE_FILE__", base_file_macro);

  define_function_macro("$vec(T)", container_vec_setup);
  define_function_macro("$map(K,V)", container_map_setup);

  time_t now = time(NULL);
  struct tm* tm = localtime(&now);
  define_macro("__DATE__", format_date(tm));
  define_macro("__TIME__", format_time(tm));
}

typedef enum {
  STR_NONE,
  STR_UTF8,
  STR_UTF16,
  STR_UTF32,
  STR_WIDE,
} StringKind;

static StringKind get_string_kind(Token* tok) {
  if (!strcmp(tok->loc, "u8"))
    return STR_UTF8;

  switch (tok->loc[0]) {
    case '"':
      return STR_NONE;
    case 'u':
      return STR_UTF16;
    case 'U':
      return STR_UTF32;
    case 'L':
      return STR_WIDE;
  }
  unreachable();
}

// Concatenate adjacent string literals into a single string literal
// as per the C spec.
static void join_adjacent_string_literals(Token* tok) {
  // First pass: If regular string literals are adjacent to wide
  // string literals, regular string literals are converted to a wide
  // type before concatenation. In this pass, we do the conversion.
  for (Token* tok1 = tok; tok1->kind != TK_EOF;) {
    if (tok1->kind != TK_STR || tok1->next->kind != TK_STR) {
      tok1 = tok1->next;
      continue;
    }

    StringKind kind = get_string_kind(tok1);
    Type* basety = tok1->ty->base;

    for (Token* t = tok1->next; t->kind == TK_STR; t = t->next) {
      StringKind k = get_string_kind(t);
      if (kind == STR_NONE) {
        kind = k;
        basety = t->ty->base;
      } else if (k != STR_NONE && kind != k) {
        error_tok(t, "unsupported non-standard concatenation of string literals");
      }
    }

    if (basety->size > 1)
      for (Token* t = tok1; t->kind == TK_STR; t = t->next)
        if (t->ty->base->size == 1)
          *t = *tokenize_string_literal(t, basety);

    while (tok1->kind == TK_STR)
      tok1 = tok1->next;
  }

  // Second pass: concatenate adjacent string literals.
  for (Token* tok1 = tok; tok1->kind != TK_EOF;) {
    if (tok1->kind != TK_STR || tok1->next->kind != TK_STR) {
      tok1 = tok1->next;
      continue;
    }

    Token* tok2 = tok1->next;
    while (tok2->kind == TK_STR)
      tok2 = tok2->next;

    int len = tok1->ty->array_len;
    for (Token* t = tok1->next; t != tok2; t = t->next)
      len = len + t->ty->array_len - 1;

    char* buf = bumpcalloc(tok1->ty->base->size, len, AL_Compile);

    int i = 0;
    for (Token* t = tok1; t != tok2; t = t->next) {
      memcpy(buf + i, t->str, t->ty->size);
      i = i + t->ty->size - t->ty->base->size;
    }

    *tok1 = *copy_token(tok1);
    tok1->ty = array_of(tok1->ty->base, len, tok1);
    tok1->str = buf;
    tok1->next = tok2;
    tok1 = tok2;
  }
}

// Entry point function of the preprocessor.
IMPLSTATIC Token* preprocess(Token* tok) {
  tok = preprocess2(tok);
  if (C(cond_incl))
    error_tok(C(cond_incl)->tok, "unterminated conditional directive");
  convert_pp_tokens(tok);
  join_adjacent_string_literals(tok);

  for (Token* t = tok; t; t = t->next)
    t->line_no += t->line_delta;
  return tok;
}

IMPLSTATIC Token* add_container_instantiations(Token* tok) {
  // Reverse order is important. They were appended as included, so we need to
  // maintain that order here.
  for (int i = C(container_tokens).len - 1; i >= 0; --i) {
    Token* to_add = C(container_tokens).data[i];
    Token* cur = to_add;
    while (cur->next->kind != TK_EOF)
      cur = cur->next;
    cur->next = tok;
    tok = to_add;
  }
  return tok;
}
