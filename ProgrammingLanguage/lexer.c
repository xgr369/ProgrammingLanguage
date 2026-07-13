#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "lexer.h"

void langP_printtok(const char *src, LangP_Token *ptok) {
    switch (ptok->type) {
        case LANGP_TOK_IDENTIFIER:
            printf("identifier string=%.*s", ptok->value.string.length, src + ptok->value.string.index);
            break;
        case LANGP_TOK_EOF:
            printf("EOF");
            break;
        case LANGP_TOK_NUMBER:
            printf("number number=%f", ptok->value.number);
            break;
        case LANGP_TOK_OPERATOR:
            printf("operator op=%d", ptok->value.tag);
            break;
        case LANGP_TOK_WHITESPACE:
            printf("whitespace");
            break;
        case LANGP_TOK_KEYWORD:
            printf("keyword tag=%d", ptok->value.tag);
            break;
        default:
            printf("unknown");
    }
}

static inline char peek(LangP_LexerState *pls) {
    return pls->src[pls->pos];
}

static inline void consume(LangP_LexerState *pls) {
    pls->pos++;
}

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

/* read identifier or keyword */
void read_identifier(LangP_LexerState *pls, LangP_Token *ptok) {
    while (isalnum(peek(pls)) || peek(pls) == '_') {
        ptok->value.string.length++;
        consume(pls);
    }
    if (tok_equals(pls, ptok, "else")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSE;
    } else if (tok_equals(pls, ptok, "elseif")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_ELSEIF;
    } else if (tok_equals(pls, ptok, "if")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_IF;
    } else if (tok_equals(pls, ptok, "then")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_THEN;
    } else if (tok_equals(pls, ptok, "end")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_END;
    } else if (tok_equals(pls, ptok, "extern")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_EXTERN;
    } else if (tok_equals(pls, ptok, "function")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_FUNCTION;
    } else if (tok_equals(pls, ptok, "return")) {
        ptok->type = LANGP_TOK_KEYWORD;
        ptok->value.tag = LANGP_TOK_KEYWORD_RETURN;
    } else {
        ptok->type = LANGP_TOK_IDENTIFIER;
    }
    return 0;
}

/* read number */
int read_number(LangP_LexerState *pls, LangP_Token *ptok) {
    while (isdigit(peek(pls))) {
        ptok->value.string.length++;
        consume(pls);
    }
    ptok->type = LANGP_TOK_NUMBER;
    char buf[64];
    if (ptok->value.string.length >= sizeof(buf)) {
        pls->msg = "number too long";
        return 1;
    }
    memcpy(buf, pls->src + ptok->value.string.index, ptok->value.string.length);
    buf[ptok->value.string.length] = '\0';
    ptok->value.number = atof(buf);
    return 0;
}

int isoperatorstartchar(char c) {
    if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '<' || c == '>' || c == ',' || c == '(' || c == ')') {
        return 1;
    }
    return 0;
}

/* read operator */
void read_operator(LangP_LexerState *pls, LangP_Token *ptok) {
    /*while (isoperator(peek(pls))) {
        ptok->value.string.length++;
        consume(pls);
    }*/
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
        ptok->value.tag = LANGP_TOK_OPERATOR_LT;
    } else if (c == '>') {
        ptok->value.tag = LANGP_TOK_OPERATOR_GT;
    } else if (c == ',') {
        ptok->value.tag = LANGP_TOK_OPERATOR_COMMA;
    } else if (c == '(') {
        ptok->value.tag = LANGP_TOK_OPERATOR_PARLEFT;
    } else if (c == ')') {
        ptok->value.tag = LANGP_TOK_OPERATOR_PARRIGHT;
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
        return 0;
    }

    if (isspace(c)) {
        read_whitespace(pls, ptok);
        return 0;
    }

    if (isalpha(c) || c == '_') {
        read_identifier(pls, ptok);
        return 0;
    }

    if (isdigit(c)) {
        return read_number(pls, ptok);
    }

    if (isoperatorstartchar(c)) {
        read_operator(pls, ptok);
        return 0;
    }

    pls->msg = "Unknown token type";
    return 1;
}

Vector langP_tokenize(char *src, LangP_LexerState *pls) {
    pls->src = src;
    pls->pos = 0;
    pls->msg = NULL;
    LangP_Token tok;
    Vector tokens;
    vector_new(&tokens, sizeof(LangP_Token), 10);
    do {
        if (read_token(pls, &tok)) {
            break;
        }
        if (tok.type != LANGP_TOK_WHITESPACE) {
            vector_push(&tokens, &tok);
        }
    } while (tok.type != LANGP_TOK_EOF);

    return tokens;
}