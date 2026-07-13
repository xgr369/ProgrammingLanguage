/*
* parser.h / part of lang_P
* Parses tokens into ...?
*/

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "vector.h"

typedef struct {
	char* src;
	Vector *ptokens; // Vector<LangP_Token>
	size_t pos;
	char *msg;
} LangP_ParserState;

typedef enum {
	LANGP_AST_OP_UNKNOWN,
	LANGP_AST_OP_ASSIGN,
	LANGP_AST_OP_ADD,
	LANGP_AST_OP_SUB,
	LANGP_AST_OP_MUL,
	LANGP_AST_OP_DIV,
	LANGP_AST_OP_EQ,
	LANGP_AST_OP_LT,
	LANGP_AST_OP_GT,
} LangP_AstOperation;


typedef enum {
	LANGP_AST_NODE_LEAF,
	LANGP_AST_NODE_BINARYEXPR,
	LANGP_AST_NODE_BLOCK,
	LANGP_AST_NODE_CALL,
	LANGP_AST_NODE_CONTROL_EXTERN,
	LANGP_AST_NODE_CONTROL_FUNCTION,
	LANGP_AST_NODE_CONTROL_IFELSEIF,
	LANGP_AST_NODE_CONTROL_RETURN,
	LANGP_AST_NODE_EXPRLIST,
	LANGP_AST_NODE_VARLIST,
} LangP_AstNodeType;

typedef struct LangP_AstNode LangP_AstNode;

typedef struct {
	LangP_AstOperation op;
	LangP_AstNode *pleft;
	LangP_AstNode *pright;
} LangP_AstBinaryExpression;

typedef struct {
	LangP_AstNode *pexpr;
	LangP_AstNode *pexprlist;
} LangP_AstCall;

typedef struct {
	LangP_AstNode *pvarlist;
	LangP_AstNode *pblock;
} LangP_AstControlFunction;

typedef struct {
	LangP_AstNode *pexpr;
	LangP_AstNode *pblock;
	LangP_AstNode *pnext;
} LangP_AstControlIfElseif;

struct LangP_AstNode {
	LangP_AstNodeType type;
	union {
		/*LEAF*/ LangP_Token *ptoken;
		/*CONTROL_EXTERN: VARLIST, CONTROL_RETURN: EXPRLIST*/ LangP_AstNode *pnode;
		/*BINARYEXPR*/ LangP_AstBinaryExpression binaryExpression;
		/*BLOCK,VARLIST,EXPRLIST*/ Vector nodes; // Vector<LandP_AstNode *>
		/*CONTROL_FUNCTION*/ LangP_AstControlFunction controlFunction;
		/*CONTROL_IFELSEIF*/ LangP_AstControlIfElseif controlIfElseif;
		/*CALL*/ LangP_AstCall call;
	} value;
};

LangP_AstNode *langP_parse(char *src, Vector *ptokens, LangP_ParserState *pps);

#endif // PARSER_H