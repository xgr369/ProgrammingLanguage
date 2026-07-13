/*
* lexer.h / part of lang_P
* Tokenizes lang source
*/

#ifndef LEXER_H
#define LEXER_H

#include "vector.h"

typedef struct {
    const char *src;
    size_t pos;
    char *msg;
} LangP_LexerState;

typedef enum {
    LANGP_TOK_IDENTIFIER,
    LANGP_TOK_NUMBER, LANGP_TOK_OPERATOR,
    LANGP_TOK_KEYWORD,
    LANGP_TOK_EOF, LANGP_TOK_WHITESPACE,
} LangP_TokenType;

typedef enum {
    LANGP_TOK_KEYWORD_ELSE,
    LANGP_TOK_KEYWORD_ELSEIF,
    LANGP_TOK_KEYWORD_END,
    LANGP_TOK_KEYWORD_EXTERN,
    LANGP_TOK_KEYWORD_FUNCTION,
    LANGP_TOK_KEYWORD_IF,
    LANGP_TOK_KEYWORD_RETURN,
    LANGP_TOK_KEYWORD_THEN,
    LANGP_TOK_OPERATOR_ASSIGN,
    LANGP_TOK_OPERATOR_EQ,
    LANGP_TOK_OPERATOR_NEQ,
    LANGP_TOK_OPERATOR_LT,
    LANGP_TOK_OPERATOR_GT,
    LANGP_TOK_OPERATOR_LE,
    LANGP_TOK_OPERATOR_GE,
    LANGP_TOK_OPERATOR_ADD,
    LANGP_TOK_OPERATOR_SUB,
    LANGP_TOK_OPERATOR_MUL,
    LANGP_TOK_OPERATOR_DIV,
    LANGP_TOK_OPERATOR_EXP,
    LANGP_TOK_OPERATOR_COMMA,
    LANGP_TOK_OPERATOR_PARLEFT,
    LANGP_TOK_OPERATOR_PARRIGHT,
} LangP_TokenTag;

typedef struct {
    LangP_TokenType type;
    union {
        struct {
            size_t index;
            size_t length;
        } string;
        double number;
        LangP_TokenTag tag;
    } value;
} LangP_Token;

void langP_printtok(const char *src, LangP_Token *ptok);
Vector langP_tokenize(char *src, LangP_LexerState *pls);

#endif // LEXER_H