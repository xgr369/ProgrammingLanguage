#include "lexer.h"
#include <stdlib.h>
#include <string.h>

static inline char peek(LangP_LexerState *pls) {
    return pls->src[pls->pos];
}

static inline void consume(LangP_LexerState *pls) {
    pls->pos++;
}
#define langL_errmsg(pls, _msg) (pls->msg = _msg)

/* whitespace */
void read_whitespace(LangP_LexerState *pls, LangP_Token *ptok) {
    while (isspace(peek(pls))) {
        ptok->value.string.length++;
        consume(pls);
    }
    ptok->type = LANGP_TOK_WHITESPACE;
    return 0;
}

#define tok_equals(pls, ptok, l) ((ptok)->value.string.length == sizeof("" l) - 1 && strncmp((pls)->src + (ptok)->value.string.index, l, sizeof(l) - 1) == 0)

void read_keyword_or_identifier(LangP_LexerState *pls, LangP_Token *ptok) {
    while (isalnum(peek(pls)) || peek(pls) == '_') {
        ptok->value.string.length++;
        consume(pls);
    }
    if (tok_equals(pls, ptok, "if")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_IF;
    } else if (tok_equals(pls, ptok, "var")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_VAR;
    } else if (tok_equals(pls, ptok, "null")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_NULL;
    } else if (tok_equals(pls, ptok, "this")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_THIS;
    } else if (tok_equals(pls, ptok, "else")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSE;
    } else if (tok_equals(pls, ptok, "elseif")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSEIF;
    } else if (tok_equals(pls, ptok, "import")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_IMPORT;
    } else if (tok_equals(pls, ptok, "return")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_RETURN;
    } else if (tok_equals(pls, ptok, "debugger")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_DEBUGGER;
    } else if (tok_equals(pls, ptok, "function")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_FUNCTION;
    } else {
        ptok->type = LANGP_TOK_IDENTIFIER;
    }
    return 0;
}

int read_number(LangP_LexerState *pls, LangP_Token *ptok) {
    while (isdigit(peek(pls))) {
        ptok->value.string.length++;
        consume(pls);
    }
    ptok->type = LANGP_TOK_NUMBER;
    char buf[64];
    if (ptok->value.string.length >= sizeof(buf)) {
        langL_errmsg(pls, "number too long");
        return 1;
    }
    memcpy(buf, pls->src + ptok->value.string.index, ptok->value.string.length);
    buf[ptok->value.string.length] = '\0';
    ptok->value.number = atof(buf);
    return 0;
}

int read_string(LangP_LexerState *pls, LangP_Token *ptok) {
    ptok->value.string.index++;
    consume(pls);
    while (peek(pls) != '"') {
        if (peek(pls) == '\0') {
            langL_errmsg(pls, "unclosed string");
            return 1;
        }
        ptok->value.string.length++;
        consume(pls);
    }
    consume(pls);
    ptok->type = LANGP_TOK_STRING;
    return 0;
}

int try_read_operator(LangP_LexerState *pls, LangP_Token *ptok) {
    char c = peek(pls);
    consume(pls);
    if (c == '=') {
        if (peek(pls) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_EQ;
            consume(pls);
        } else {
            ptok->value.tag = LANGP_TOK_OPERATOR_ASSIGN;
        }
    } else if (c == '+') {
        ptok->value.tag = LANGP_TOK_OPERATOR_ADD;
    } else if (c == '-') {
        ptok->value.tag = LANGP_TOK_OPERATOR_SUB;
    } else if (c == '*') {
        ptok->value.tag = LANGP_TOK_OPERATOR_MUL;
    } else if (c == '/') {
        ptok->value.tag = LANGP_TOK_OPERATOR_DIV;
    } else if (c == '^') {
        ptok->value.tag = LANGP_TOK_OPERATOR_EXP;
    } else if (c == '<') {
        if (peek(pls) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_LE;
            consume(pls);
        } else {
            ptok->value.tag = LANGP_TOK_OPERATOR_LT;
        }
    } else if (c == '>') {
        if (peek(pls) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_GE;
            consume(pls);
        } else {
            ptok->value.tag = LANGP_TOK_OPERATOR_GT;
        }
    } else if (c == ',') {
        ptok->value.tag = LANGP_TOK_OPERATOR_COMMA;
    } else if (c == '(') {
        ptok->value.tag = LANGP_TOK_OPERATOR_PARLEFT;
    } else if (c == ')') {
        ptok->value.tag = LANGP_TOK_OPERATOR_PARRIGHT;
    } else if (c == '!') {
        ptok->value.tag = LANGP_TOK_OPERATOR_NOT;
    } else if (c == '#') {
        ptok->value.tag = LANGP_TOK_OPERATOR_LEN;
    } else if (c == '.') {
        ptok->value.tag = LANGP_TOK_OPERATOR_DOT;
    } else if (c == '{') {
        ptok->value.tag = LANGP_TOK_OPERATOR_CBRACELEFT;
    } else if (c == '}') {
        ptok->value.tag = LANGP_TOK_OPERATOR_CBRACERIGHT;
    } else {
        return 1;
    }
    ptok->type = LANGP_TOK_OPERATOR;
    return 0;
}

/* get next LangP_Token */
int read_token(LangP_LexerState *pls, LangP_Token *ptok) {
    ptok->value.string.index = pls->pos;
    ptok->value.string.length = 0;

    char c = peek(pls);

    if (c == '\0') {
        ptok->type = LANGP_TOK_EOF;
        consume(pls);
        return 0;
    }

    if (c == ';') {
        ptok->type = LANGP_TOK_SEMICOLON;
        consume(pls);
        return 0;
    }

    if (isspace(c)) {
        read_whitespace(pls, ptok);
        return 0;
    }

    if (isalpha(c) || c == '_') {
        read_keyword_or_identifier(pls, ptok);
        return 0;
    }

    if (isdigit(c)) {
        return read_number(pls, ptok);
    }

    if (c == '"') {
        return read_string(pls, ptok);
    }

    int result = try_read_operator(pls, ptok);
    if (!result) {
        return 0;
    }

    assert(0);
    return 1;
}

List langP_tokenize(const char *src, LangP_LexerState *pls) {
    pls->src = src;
    pls->pos = 0;
    pls->msg = NULL;
    LangP_Token tok;
    List tokens;
    list_init(&tokens, sizeof(LangP_Token), 10);
    do {
        if (read_token(pls, &tok)) {
            break;
        }
        if (tok.type != LANGP_TOK_WHITESPACE) {
            list_push(&tokens, &tok);
        }
    } while (tok.type != LANGP_TOK_EOF);

    return tokens;
}