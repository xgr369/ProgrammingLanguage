#include "compiler.h"
#include "vm.h"
#include "parser.h"

int compilebinaryop_strict(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, char op) {
	compileexpr(src, pnode->value.binaryExpression.pleft, pcs, dst);
	compileexpr(src, pnode->value.binaryExpression.pright, pcs, dst);
	langC_emitop(dst, LANGV_OP_BINARYOP);
	langC_emitchar(dst, op);
	pcs->stackSize--;
	return 0;
}

int compileexpr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_LEAF:
		{
			LangP_Token *ptok = pnode->value.ptoken;
			switch (ptok->type) {
				case LANGP_TOK_NUMBER:
				{
					langC_emitchar(dst, LANGV_OP_PUSHNUMBER);
					langC_emitdouble(dst, ptok->value.number);
					pcs->stackSize++;
				} break;
				case LANGP_TOK_IDENTIFIER:
				{
					int len = ptok->value.string.length;
					char *str = malloc(len + 1);
					if (!str) {
						langC_errmsg(pcs, "failed to allocate");
						return 1;
					}
					memcpy(str, src + ptok->value.string.index, len);
					str[len] = '\0';
					int stackIndex;
					if (stringhashtable_get(&pcs->identifierTable, str, &stackIndex) == 0) {
						langC_emitchar(dst, LANGV_OP_PUSHVALUE);
						langC_emitinteger(dst, stackIndex);
					} else {
						langC_emitchar(dst, LANGV_OP_PUSHNIL);
					}
					pcs->stackSize++;
					free(str);
				} break;
			}
		} break;
		case LANGP_AST_NODE_BINARYEXPR:
		{
			if (pnode->value.binaryExpression.op == LANGP_AST_OP_ASSIGN) {
				LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
				LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
				int nVar = pvarlist->value.nodes.length;
				int stackSectionBottom = pcs->stackSize;
				for (int i = 0; i < nVar; i++) {
					LangP_AstNode *pexpr;
					if (vector_get(&pexprlist->value.nodes, i, &pexpr)) {
						langC_emitop(dst, LANGV_OP_PUSHNIL);
						pcs->stackSize++;
					} else {
						if (compileexpr(src, pexpr, pcs, dst)) {
							return 1;
						}
					}
				}
				int stackSectionNewTop = stackSectionBottom;
				for (int i = 0; i < nVar; i++) {
					LangP_AstNode *pvar;
					vector_get(&pvarlist->value.nodes, i, &pvar);
					LangP_Token *ptokVar = pvar->value.ptoken; // assuming an identifier token
					int len = ptokVar->value.string.length;
					char *str = malloc(len + 1);
					if (!str) {
						langC_errmsg(pcs, "failed to allocate");
						return 1;
					}
					memcpy(str, src + ptokVar->value.string.index, len);
					str[len] = '\0';
					int newVar = stringhashtable_containskey(&pcs->identifierTable, str);
					if (newVar) {
						// Record new var
						stringhashtable_put(&pcs->identifierTable, str, &stackSectionNewTop);
					} else {
						// Replace old var
						int stackIndex;
						stringhashtable_get(&pcs->identifierTable, str, &stackIndex);
						if (i == nVar - 1 && stackSectionNewTop == stackSectionBottom + i) {
							langC_emitchar(dst, LANGV_OP_REPLACE);
							langC_emitinteger(dst, stackIndex);
							pcs->stackSize--;
							nVar--; // workaround to disable emission of "POPN"
						} else {
							langC_emitchar(dst, LANGV_OP_COPY);
							langC_emitinteger(dst, stackSectionBottom + i);
							langC_emitinteger(dst, stackIndex);
						}
					}
					free(str);
					if (stackSectionNewTop != stackSectionBottom + i) {
						langC_emitchar(dst, LANGV_OP_COPY);
						langC_emitinteger(dst, stackSectionBottom + i);
						langC_emitinteger(dst, stackSectionNewTop);
					}
					if (newVar) {
						stackSectionNewTop++;
					}
				}
				int stackSectionOldTop = stackSectionBottom + nVar;
				if (stackSectionNewTop != stackSectionOldTop) {
					int n = stackSectionOldTop - stackSectionNewTop;
					langC_emitchar(dst, LANGV_OP_POPN);
					langC_emitinteger(dst, n);
					pcs->stackSize -= n;
				}
				break;
			}
			switch (pnode->value.binaryExpression.op) {
				case LANGP_AST_OP_ADD:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_ADD);
					break;
				case LANGP_AST_OP_SUB:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_SUB);
					break;
				case LANGP_AST_OP_MUL:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_MUL);
					break;
				case LANGP_AST_OP_DIV:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_DIV);
					break;
				case LANGP_AST_OP_EQ:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_EQ);
					break;
				case LANGP_AST_OP_LT:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_LT);
					break;
				case LANGP_AST_OP_GT:
					compilebinaryop_strict(src, pnode, pcs, dst, LANG_OP_GT);
					break;
				default:
					langC_errmsg(pcs, "unknown operation");
					return 1;
			}
		} break;
		case LANGP_AST_NODE_CALL:
			if (compileexpr(src, pnode->value.call.pexpr, pcs, dst)) {
				return 1;
			}
			LangP_AstNode *pexprlist = pnode->value.call.pexprlist;
			int nArg = pexprlist->value.nodes.length;
			for (int i = 0; i < nArg; i++) {
				LangP_AstNode *pexpr;
				vector_get(&pexprlist->value.nodes, i, &pexpr);
				if (compileexpr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
			langC_emitop(dst, LANGV_OP_CALL);
			langC_emitinteger(dst, nArg);
			langC_emitinteger(dst, 1);
			break;
		default:
			langC_errmsg(pcs, "expected LEAF, BINARYEXPR, or CALL node");
			return 1;
	}
	return 0;
}

int compilestatement(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_CONTROL_EXTERN:
		{
			LangP_AstNode *pvarlist = pnode->value.pnode;
			int nVar = pvarlist->value.nodes.length;
			for (int i = 0; i < nVar; i++) {
				LangP_AstNode *pvar;
				vector_get(&pvarlist->value.nodes, i, &pvar);
				LangP_Token *ptokVar = pvar->value.ptoken; // assuming an identifier token
				int len = ptokVar->value.string.length;
				char *str = malloc(len + 1);
				if (!str) {
					langC_errmsg(pcs, "failed to allocate");
					return 1;
				}
				memcpy(str, src + ptokVar->value.string.index, len);
				str[len] = '\0';
				langC_emitop(dst, LANGV_OP_LOADEXTERNVALUE);
				langC_emitlstring(dst, str, len);
				pcs->stackSize++;
				int newVar = stringhashtable_containskey(&pcs->identifierTable, str);
				if (newVar) {
					// Record new var
					int stackIndex = pcs->stackSize - 1;
					stringhashtable_put(&pcs->identifierTable, str, &stackIndex);
				} else {
					// Replace old var
					int stackIndex;
					stringhashtable_get(&pcs->identifierTable, str, &stackIndex);
					langC_emitchar(dst, LANGV_OP_REPLACE);
					langC_emitinteger(dst, stackIndex);
					pcs->stackSize--;
				}
			}
			return 0;
		}
		case LANGP_AST_NODE_CONTROL_IF:
		{
			compileexpr(src, pnode->value.controlIf.pexpr, pcs, dst);
			langC_emitop(dst, LANGV_OP_JMPNZ);
			langC_emitinteger(dst, 1); // PLACEHOLDER
			pcs->stackSize--;
			size_t blockStart = dst->length;
			if (compileblock(src, pnode->value.controlIf.pblock, pcs, dst)) {
				return 1;
			}
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			return 0;
		}
		default:
			return compileexpr(src, pnode, pcs, dst);
	}
}

int compileblock(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	if (pnode->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "expected BLOCK node");
		return 1;
	}
	for (int i = 0; i < pnode->value.nodes.length; i++) {
		LangP_AstNode *psubnode;
		vector_get(&pnode->value.nodes, i, &psubnode);
		if (compilestatement(src, psubnode, pcs, dst))
			return 1;
	}
	return 0;
}

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, CharVector *dst) {
	if (!root || root->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "invalid root node");
		return 1;
	}
	stringhashtable_new(&pcs->identifierTable, sizeof(int), 10);
	pcs->stackSize = 0;
	pcs->msg = NULL;
	return compileblock(src, root, pcs, dst);
}