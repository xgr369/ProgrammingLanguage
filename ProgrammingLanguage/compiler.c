#include "compiler.h"
#include "vm.h"
#include "parser.h"

inline void init_scope_context(LangC_ScopeContext *psc) {
	stringhashtable_new(&psc->identifierTable, sizeof(int), 10);
	psc->stackSize = 0;
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst);

int compile_call(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, int nReturn) {
	LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
	int stackSizeBeforeCall = psc->stackSize;
	if (compile_expr(src, pnode->value.call.pexpr, pcs, dst)) {
		return 1;
	}
	LangP_AstNode *pexprlist = pnode->value.call.pexprlist;
	int nArg = pexprlist->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pexpr;
		vector_get(&pexprlist->value.nodes, i, &pexpr);
		if (compile_expr(src, pexpr, pcs, dst)) {
			return 1;
		}
	}
	langC_emitop(dst, LANGV_OP_CALL);
	langC_emitinteger(dst, nArg);
	langC_emitinteger(dst, nReturn);
	psc->stackSize = stackSizeBeforeCall + nReturn;
	return 0;
}

int compile_binaryop_strict(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	if (compile_expr(src, pnode->value.binaryExpression.pright, pcs, dst)) {
		return 1;
	}
	langC_emitop(dst, LANGV_OP_BINARYOP);
	langC_emitchar(dst, op);
	LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
	psc->stackSize--;
	return 0;
}

int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
	LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
	int nVar = pvarlist->value.nodes.length;
	LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
	int stackSizeBeforeAssignment = psc->stackSize;

	LangP_AstNode *pexprFirst;
	vector_get(&pexprlist->value.nodes, 0, &pexprFirst);
	if (pexprlist->value.nodes.length == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
		// Function call
		if (compile_expr(src, pexprFirst->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *pexprlistCall = pexprFirst->value.call.pexprlist;
		int nArg = pexprlistCall->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *pexpr;
			vector_get(&pexprlistCall->value.nodes, i, &pexpr);
			if (compile_expr(src, pexpr, pcs, dst)) {
				return 1;
			}
		}
		langC_emitop(dst, LANGV_OP_CALL);
		langC_emitinteger(dst, nArg);
		langC_emitinteger(dst, nVar);
		psc->stackSize = stackSizeBeforeAssignment + nVar;
	} else {
		// Ordinary assignment
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (vector_get(&pexprlist->value.nodes, i, &pexpr)) {
				langC_emitop(dst, LANGV_OP_PUSHNIL);
				psc->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	// Assign values
	int stackSectionNewTop = stackSizeBeforeAssignment;
	for (int i = 0; i < nVar; i++) {
		LangP_AstNode *pvar;
		vector_get(&pvarlist->value.nodes, i, &pvar);
		LangP_Token *ptokVar = pvar->value.ptoken;
		int len = ptokVar->value.string.length;
		char *str = malloc(len + 1);
		if (!str) {
			free(str);
			langC_errmsg(pcs, "failed to allocate");
			return 1;
		}
		memcpy(str, src + ptokVar->value.string.index, len);
		str[len] = '\0';
		LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
		int newVar = stringhashtable_containskey(&psc->identifierTable, str);
		if (newVar) {
			// Record new var
			stringhashtable_put(&psc->identifierTable, str, &stackSectionNewTop);
		} else {
			// Replace old var
			int stackIndex;
			stringhashtable_get(&psc->identifierTable, str, &stackIndex);
			if (i == nVar - 1 && stackSectionNewTop == stackSizeBeforeAssignment + i) {
				langC_emitchar(dst, LANGV_OP_REPLACE);
				langC_emitinteger(dst, stackIndex);
				LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
				psc->stackSize--;
				nVar--; // workaround to disable emission of "POPN"
			} else {
				langC_emitchar(dst, LANGV_OP_COPY);
				langC_emitinteger(dst, stackSizeBeforeAssignment + i);
				langC_emitinteger(dst, stackIndex);
			}
		}
		free(str);
		if (stackSectionNewTop != stackSizeBeforeAssignment + i) {
			langC_emitchar(dst, LANGV_OP_COPY);
			langC_emitinteger(dst, stackSizeBeforeAssignment + i);
			langC_emitinteger(dst, stackSectionNewTop);
		}
		if (newVar) {
			stackSectionNewTop++;
		}
	}
	int stackSectionOldTop = stackSizeBeforeAssignment + nVar;
	if (stackSectionNewTop != stackSectionOldTop) {
		int n = stackSectionOldTop - stackSectionNewTop;
		langC_emitchar(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, n);
		LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
		psc->stackSize -= n;
	}
	return 0;
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_LEAF:
		{
			LangP_Token *ptok = pnode->value.ptoken;
			switch (ptok->type) {
				case LANGP_TOK_NUMBER:
				{
					langC_emitchar(dst, LANGV_OP_PUSHNUMBER);
					langC_emitdouble(dst, ptok->value.number);
					LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
					psc->stackSize++;
				} break;
				case LANGP_TOK_IDENTIFIER:
				{
					int len = ptok->value.string.length;
					char *str = malloc(len + 1);
					if (!str) {
						free(str);
						langC_errmsg(pcs, "failed to allocate");
						return 1;
					}
					memcpy(str, src + ptok->value.string.index, len);
					str[len] = '\0';
					int stackIndex;
					LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
					if (stringhashtable_get(&psc->identifierTable, str, &stackIndex) == 0) {
						langC_emitchar(dst, LANGV_OP_PUSHVALUE);
						langC_emitinteger(dst, stackIndex);
					} else {
						langC_emitchar(dst, LANGV_OP_PUSHNIL);
					}
					psc->stackSize++;
					free(str);
				} break;
			}
		} break;
		case LANGP_AST_NODE_BINARYEXPR:
		{
			switch (pnode->value.binaryExpression.op) {
				case LANGP_AST_OP_ADD:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_ADD);
					break;
				case LANGP_AST_OP_SUB:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_SUB);
					break;
				case LANGP_AST_OP_MUL:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_MUL);
					break;
				case LANGP_AST_OP_DIV:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_DIV);
					break;
				case LANGP_AST_OP_EQ:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_EQ);
					break;
				case LANGP_AST_OP_LT:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_LT);
					break;
				case LANGP_AST_OP_GT:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_GT);
					break;
				default:
					langC_errmsg(pcs, "unknown operation");
					return 1;
			}
		} break;
		case LANGP_AST_NODE_CALL:
		{
			if (compile_call(src, pnode, pcs, dst, 1)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_CONTROL_FUNCTION:
		{
			langC_emitop(dst, LANGV_OP_JMP);
			langC_emitinteger(dst, 1); // PLACEHOLDER
			size_t blockStart = dst->length;
			LangC_ScopeContext sc;
			init_scope_context(&sc);
			int nArg = pnode->value.controlFunction.pvarlist->value.nodes.length;
			for (int i = 0; i < nArg; i++) {
				LangP_AstNode *pvar;
				vector_get(&pnode->value.controlFunction.pvarlist->value.nodes.data, i, &pvar);
				LangP_Token *ptok = pvar->value.ptoken;
				int len = ptok->value.string.length;
				char *str = malloc(len + 1);
				if (!str) {
					free(str);
					langC_errmsg(pcs, "failed to allocate");
					return 1;
				}
				memcpy(str, src + ptok->value.string.index, len);
				str[len] = '\0';
				stringhashtable_put(&sc.identifierTable, str, &i);
				free(str);
			}
			sc.stackSize = nArg;
			vector_push(&pcs->scopeContexts, &sc);
			if (compile_block(src, pnode->value.controlFunction.pblock, pcs, dst)) {
				return 1;
			}
			vector_pop(&pcs->scopeContexts, NULL);
			langC_emitop(dst, LANGV_OP_END);
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			langC_emitop(dst, LANGV_OP_PUSHLFUNC);
			langC_emitinteger(dst, blockStart);
			LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
			psc->stackSize++;
		} break;
		default:
			langC_errmsg(pcs, "expected LEAF, BINARYEXPR, or CALL node");
			return 1;
	}
	return 0;
}

int compile_statement(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
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
					free(str);
					langC_errmsg(pcs, "failed to allocate");
					return 1;
				}
				memcpy(str, src + ptokVar->value.string.index, len);
				str[len] = '\0';
				langC_emitop(dst, LANGV_OP_LOADEXTERNVALUE);
				langC_emitlstring(dst, str, len);
				LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
				psc->stackSize++;
				int newVar = stringhashtable_containskey(&psc->identifierTable, str);
				if (newVar) {
					// Record new var
					int stackIndex = psc->stackSize - 1;
					stringhashtable_put(&psc->identifierTable, str, &stackIndex);
				} else {
					// Replace old var
					int stackIndex;
					stringhashtable_get(&psc->identifierTable, str, &stackIndex);
					langC_emitchar(dst, LANGV_OP_REPLACE);
					langC_emitinteger(dst, stackIndex);
					psc->stackSize--;
				}
			}
		} break;
		case LANGP_AST_NODE_CONTROL_IFELSEIF: // TODO: implement elseif
		{
			if (compile_expr(src, pnode->value.controlIfElseif.pexpr, pcs, dst)) {
				return 1;
			}
			langC_emitop(dst, LANGV_OP_JMPZ);
			langC_emitinteger(dst, 1); // PLACEHOLDER
			LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
			psc->stackSize--;
			int stackFrameStart = psc->stackSize;
			size_t blockStart = dst->length;
			if (compile_block(src, pnode->value.controlIfElseif.pblock, pcs, dst)) {
				return 1;
			}
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			psc->stackSize = stackFrameStart;
		} break;
		case LANGP_AST_NODE_CONTROL_RETURN:
		{
			LangP_AstNode *pexprlist = pnode->value.pnode;
			int nReturn = pexprlist->value.nodes.length;
			for (int i = 0; i < nReturn; i++) {
				LangP_AstNode *pexpr;
				vector_get(&pexprlist->value.nodes, i, &pexpr);
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
			langC_emitop(dst, LANGV_OP_RETURN);
			langC_emitinteger(dst, nReturn);
			// decrement stack size?
		} break;
		case LANGP_AST_NODE_BINARYEXPR:
			if (pnode->value.binaryExpression.op != LANGP_AST_OP_ASSIGN) {
				langC_errmsg(pcs, "only assignments are allowed as statements");
				return 1;
			}
			if (compile_assignment(src, pnode, pcs, dst)) {
				return 1;
			}
			break;
		case LANGP_AST_NODE_CALL:
			if (compile_call(src, pnode, pcs, dst, 0)) {
				return 1;
			}
			break;
		default:
			langC_errmsg(pcs, "expected control, binary expression, or call");
			return 1;
	}
	return 0;
}

int compile_block(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	if (pnode->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "expected BLOCK node");
		return 1;
	}
	for (int i = 0; i < pnode->value.nodes.length; i++) {
		LangP_AstNode *psubnode;
		vector_get(&pnode->value.nodes, i, &psubnode);
		if (compile_statement(src, psubnode, pcs, dst))
			return 1;
	}
	return 0;
}

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, CharVector *dst) {
	if (!root || root->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "invalid root node");
		return 1;
	}
	vector_new(&pcs->scopeContexts, sizeof(LangC_ScopeContext), 1);
	LangC_ScopeContext sc;
	init_scope_context(&sc);
	vector_push(&pcs->scopeContexts, &sc);
	pcs->msg = NULL;
	return compile_block(src, root, pcs, dst);
}