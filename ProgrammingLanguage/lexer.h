/*
* lexer.h
* Tokenizes source code
*/

#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "conf.h"
#include "list.h"

typedef struct {
    const char *src;
    size_t pos;
    const char *msg;
} LangP_LexerState;

typedef enum {
    LANGP_TOK_IDENTIFIER,
    LANGP_TOK_NUMBER, LANGP_TOK_OPERATOR,
    LANGP_TOK_KEYWORD, LANGP_TOK_STRING,
    LANGP_TOK_SEMICOLON,
    LANGP_TOK_EOF, LANGP_TOK_WHITESPACE,
} LangP_TokenType;

typedef enum {
    LANGP_TOK_KEYWORD_DEBUGGER,
    LANGP_TOK_KEYWORD_ELSE,
    LANGP_TOK_KEYWORD_ELSEIF,
    LANGP_TOK_KEYWORD_FUNCTION,
    LANGP_TOK_KEYWORD_IF,
    LANGP_TOK_KEYWORD_IMPORT,
    LANGP_TOK_KEYWORD_NULL,
    LANGP_TOK_KEYWORD_RETURN,
    LANGP_TOK_KEYWORD_THIS,
    LANGP_TOK_KEYWORD_VAR,
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
    LANGP_TOK_OPERATOR_NOT,
    LANGP_TOK_OPERATOR_LEN,
    LANGP_TOK_OPERATOR_DOT,
    LANGP_TOK_OPERATOR_CBRACELEFT,
    LANGP_TOK_OPERATOR_CBRACERIGHT,
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

List langP_tokenize(const char *src, LangP_LexerState *pls);

#endif // LEXER_H