#include "lexer.h"
#include <stdlib.h>
#include <string.h>

static inline char peek(LangP_ParserState *pps) {
    return pps->src[pps->pos];
}

static inline void consume(LangP_ParserState *pps) {
    pps->pos++;
}

/* whitespace */
void read_whitespace(LangP_ParserState *pps, LangP_Token *ptok) {
    while (isspace(peek(pps))) {
        ptok->value.string.length++;
        consume(pps);
    }
    ptok->type = LANGP_TOK_WHITESPACE;
    return 0;
}

#define tok_equals(pps, ptok, l) ((ptok)->value.string.length == sizeof("" l) - 1 && strncmp((pps)->src + (ptok)->value.string.index, l, sizeof(l) - 1) == 0)

void read_keyword_or_identifier(LangP_ParserState *pps, LangP_Token *ptok) {
    while (isalnum(peek(pps)) || peek(pps) == '_') {
        ptok->value.string.length++;
        consume(pps);
    }
    if (tok_equals(pps, ptok, "if")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_IF;
    } else if (tok_equals(pps, ptok, "var")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_VAR;
    } else if (tok_equals(pps, ptok, "null")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_NULL;
    } else if (tok_equals(pps, ptok, "this")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_THIS;
    } else if (tok_equals(pps, ptok, "else")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSE;
    } else if (tok_equals(pps, ptok, "while")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_WHILE;
    } else if (tok_equals(pps, ptok, "elseif")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSEIF;
    } else if (tok_equals(pps, ptok, "export")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_EXPORT;
    } else if (tok_equals(pps, ptok, "import")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_IMPORT;
    } else if (tok_equals(pps, ptok, "return")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_RETURN;
    } else if (tok_equals(pps, ptok, "debugger")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_DEBUGGER;
    } else if (tok_equals(pps, ptok, "function")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_FUNCTION;
    } else {
        ptok->type = LANGP_TOK_IDENTIFIER;
    }
    return 0;
}

int read_number(LangP_ParserState *pps, LangP_Token *ptok) {
    while (isdigit(peek(pps))) {
        ptok->value.string.length++;
        consume(pps);
    }
    ptok->type = LANGP_TOK_NUMBER;
    char buf[64];
    if (ptok->value.string.length >= sizeof(buf)) {
        langP_errmsg(pps, "number too long");
        return 1;
    }
    memcpy(buf, pps->src + ptok->value.string.index, ptok->value.string.length);
    buf[ptok->value.string.length] = '\0';
    ptok->value.number = atof(buf);
    return 0;
}

int read_string(LangP_ParserState *pps, LangP_Token *ptok) {
    ptok->value.string.index++;
    consume(pps);
    while (peek(pps) != '"') {
        if (peek(pps) == '\n' || peek(pps) == '\0') {
            langP_errmsg(pps, "unclosed string");
            return 1;
        }
        ptok->value.string.length++;
        consume(pps);
    }
    consume(pps);
    ptok->type = LANGP_TOK_STRING;
    return 0;
}

int try_read_operator(LangP_ParserState *pps, LangP_Token *ptok) {
    char c = peek(pps);
    consume(pps);
    if (c == '=') {
        if (peek(pps) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_EQ;
            consume(pps);
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
        if (peek(pps) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_LE;
            consume(pps);
        } else {
            ptok->value.tag = LANGP_TOK_OPERATOR_LT;
        }
    } else if (c == '>') {
        if (peek(pps) == '=') {
            ptok->value.tag = LANGP_TOK_OPERATOR_GE;
            consume(pps);
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
    } else if (c == '[') {
        ptok->value.tag = LANGP_TOK_OPERATOR_SBRACELEFT;
    } else if (c == ']') {
        ptok->value.tag = LANGP_TOK_OPERATOR_SBRACERIGHT;
    } else if (c == '{') {
        ptok->value.tag = LANGP_TOK_OPERATOR_CBRACELEFT;
    } else if (c == '}') {
        ptok->value.tag = LANGP_TOK_OPERATOR_CBRACERIGHT;
    } else if (c == ':') {
        ptok->value.tag = LANGP_TOK_OPERATOR_COLON;
    } else {
        return 1;
    }
    ptok->type = LANGP_TOK_OPERATOR;
    return 0;
}

/* get next LangP_Token */
int read_token(LangP_ParserState *pps, LangP_Token *ptok) {
    ptok->value.string.index = pps->pos;
    ptok->value.string.length = 0;

    char c = peek(pps);

    if (c == '\0') {
        ptok->type = LANGP_TOK_EOF;
        consume(pps);
        return 0;
    }

    if (c == ';') {
        ptok->type = LANGP_TOK_SEMICOLON;
        consume(pps);
        return 0;
    }

    if (isspace(c)) {
        read_whitespace(pps, ptok);
        return 0;
    }

    if (isalpha(c) || c == '_') {
        read_keyword_or_identifier(pps, ptok);
        return 0;
    }

    if (isdigit(c)) {
        return read_number(pps, ptok);
    }

    if (c == '"') {
        return read_string(pps, ptok);
    }

    int result = try_read_operator(pps, ptok);
    if (!result) {
        return 0;
    }

    langP_errmsg(pps, "unknown token");
    return 1;
}

LangM_List langP_tokenize(LangP_ParserState *pps) {
    pps->pos = 0;
    LangP_Token tok;
    LangM_List tokens;
    langM_list_init(&tokens, sizeof(LangP_Token), 10);
    for (;;) {
        if (read_token(pps, &tok)) {
            break;
        }
        if (tok.type != LANGP_TOK_WHITESPACE) {
            langM_list_push(&tokens, &tok);
        }
        if (tok.type == LANGP_TOK_EOF) {
            break;
        }
    }
    return tokens;
}