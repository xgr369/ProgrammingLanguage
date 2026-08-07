/*
* parser.h
* Parses tokens into an abstract syntax tree
*/

#ifndef PARSER_H
#define PARSER_H

#include "conf.h"
#include "lexer.h"
#include "list.h"

typedef struct {
	char* src;
	List *ptokens; // List<LangP_Token>
	size_t pos;
	char *msg;
} LangP_ParserState;

typedef enum {
	LANGP_AST_OP_UNKNOWN,
	LANGP_AST_OP_ADD,
	LANGP_AST_OP_SUB,
	LANGP_AST_OP_MUL,
	LANGP_AST_OP_DIV,
	LANGP_AST_OP_EQ,
	LANGP_AST_OP_LT,
	LANGP_AST_OP_GT,
	LANGP_AST_OP_LE,
	LANGP_AST_OP_GE,
	LANGP_AST_OP_NEG,
	LANGP_AST_OP_NOT,
	LANGP_AST_OP_LEN,
} LangP_AstOperation;


typedef enum {
	LANGP_AST_NODE_ASSIGNMENT,
	LANGP_AST_NODE_BINARYEXPR,
	LANGP_AST_NODE_BLOCK,
	LANGP_AST_NODE_CALL,
	LANGP_AST_NODE_CONTROL_FUNCTION,
	LANGP_AST_NODE_CONTROL_IFELSEIF,
	LANGP_AST_NODE_CONTROL_ELSE,
	LANGP_AST_NODE_CONTROL_IMPORT,
	LANGP_AST_NODE_CONTROL_RETURN,
	LANGP_AST_NODE_EXPRLIST,
	LANGP_AST_NODE_FIELDEXPR,
	LANGP_AST_NODE_LEAF,
	LANGP_AST_NODE_UNARYEXPR,
	LANGP_AST_NODE_VARLIST,
} LangP_AstNodeType;

typedef struct LangP_AstNode LangP_AstNode;

typedef struct {
	LangP_AstNode *pvarlist;
	LangP_AstNode *pexprlist;
} LangP_AstAssignment;

typedef struct {
	LangP_AstOperation op;
	LangP_AstNode *pleft;
	LangP_AstNode *pright;
} LangP_AstBinaryExpression;

typedef struct {
	LangP_AstOperation op;
	LangP_AstNode *pinner;
} LangP_AstUnaryExpression;

typedef struct {
	LangP_AstNode *pexpr;
	LangP_AstNode *pexprlist;
} LangP_AstCall;

typedef struct {
	LangP_AstNode *pblock;
} LangP_AstControlElse;

typedef struct {
	LangP_AstNode *pparamlist;
	LangP_AstNode *pblock;
} LangP_AstControlFunction;

typedef struct {
	LangP_AstNode *pexpr;
	LangP_AstNode *pblock;
	LangP_AstNode *pnext;
} LangP_AstControlIfElseif;

typedef struct {
	LangP_AstNode *pparent;
	LangP_AstNode *pchild;
} LangP_AstFieldExpression;

struct LangP_AstNode {
	LangP_AstNodeType type;
	union {
		/*LEAF*/ LangP_Token *ptoken;
		/*CONTROL_IMPORT: VARLIST, CONTROL_RETURN: EXPRLIST*/ LangP_AstNode *pnode;
		/*BINARYEXPR*/ LangP_AstBinaryExpression binaryExpression;
		/*UNARYEXPR*/ LangP_AstUnaryExpression unaryExpression;
		/*BLOCK,VARLIST,EXPRLIST*/ List nodes; // List<LangP_AstNode *>
		/*CONTROL_ELSE*/ LangP_AstControlElse controlElse;
		/*CONTROL_FUNCTION*/ LangP_AstControlFunction controlFunction;
		/*CONTROL_IFELSEIF*/ LangP_AstControlIfElseif controlIfElseif;
		/*CALL*/ LangP_AstCall call;
		/*FIELDEXPR*/ LangP_AstFieldExpression fieldExpression;
		/*ASSIGNMENT*/ LangP_AstAssignment assignment;
	} value;
};

LangP_AstNode *langP_parse(char *src, List *ptokens, LangP_ParserState *pps);
void langP_free(LangP_AstNode *pnode);

#endif // PARSER_H