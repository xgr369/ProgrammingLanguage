#include "compiler.h"
#include "vm.h"

static inline langC_emitop(CharVector *pbc, char c) {
	charvector_push(pbc, c);
}
#define langC_emitchar(pbc, c) (charvector_push(pbc, c))
#define langC_emitbyte(pbc, c) (charvector_push(pbc, c))
#define langC_emitliteral(pbc, l) (charvector_pusharray(pbc, "" l, sizeof(l) - 1))
static inline langC_emitdouble(CharVector *pbc, double d) {
	charvector_pusharray(pbc, &d, sizeof(double));
}
static inline langC_emitinteger(CharVector *pbc, int i) {
	charvector_pusharray(pbc, &i, sizeof(int));
}
static inline langC_writeinteger(CharVector *pbc, int index, int i) {
	charvector_setarray(pbc, index, &i, sizeof(int));
}
static inline langC_emitlstring(CharVector *pbc, const char *src, int l) {
	charvector_pusharray(pbc, &l, sizeof(int));
	charvector_pusharray(pbc, src, l);
}
static inline langC_emitptr(CharVector *pbc, size_t ptr) {
	charvector_pusharray(pbc, &ptr, sizeof(size_t));
}
#define langC_errmsg(pc, _msg) (pc->msg = _msg)

void push_scope_context(Vector *scopeContexts, size_t stackSize, LangC_ScopeType type) {
	LangC_ScopeContext sc;
	stringhashtable_new(&sc.identifierTable, sizeof(int), 10);
	sc.stackSize = stackSize;
	sc.type = type;
	stringhashtable_new(&sc.upvalueTable, sizeof(int), 10);
	vector_new(&sc.upvalues, sizeof(LangC_UpvalDesc), 10);
	vector_push(scopeContexts, &sc);
}

void pop_scope_context(Vector *scopeContexts) {
	LangC_ScopeContext *psc = vector_at(scopeContexts, scopeContexts->length - 1);
	stringhashtable_free(&psc->identifierTable);
	stringhashtable_free(&psc->upvalueTable);
	vector_free(&psc->upvalues);
	vector_pop(scopeContexts, NULL);
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst);

int compile_call(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, int nReturn) {
	LangC_ScopeContext *psc = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
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
	psc = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	psc->stackSize = stackSizeBeforeCall + nReturn;
	return 0;
}

int compile_tailcall(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	if (compile_expr(src, pnode->value.call.pexpr, pcs, dst)) {
		return 1;
	}
	LangP_AstNode *pexprlistCall = pnode->value.call.pexprlist;
	int nArg = pexprlistCall->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pexpr;
		vector_get(&pexprlistCall->value.nodes, i, &pexpr);
		if (compile_expr(src, pexpr, pcs, dst)) {
			return 1;
		}
	}
	langC_emitop(dst, LANGV_OP_TAILCALL);
	langC_emitinteger(dst, nArg);
}

int compile_binaryop_strict(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	if (compile_expr(src, pnode->value.binaryExpression.pright, pcs, dst)) {
		return 1;
	}
	langC_emitop(dst, LANGV_OP_BINARYOP);
	langC_emitbyte(dst, op);
	LangC_ScopeContext *psc = psc = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	psc->stackSize--;
	return 0;
}

int compile_unaryop(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	langC_emitop(dst, LANGV_OP_UNARYOP);
	langC_emitbyte(dst, op);
	return 0;
}

int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
	LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
	int nVar = pvarlist->value.nodes.length;
	int nExpr = pexprlist->value.nodes.length;
	LangC_ScopeContext *pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	int stackBase = pscCurr->stackSize;

	LangP_AstNode *pexpr;
	vector_get(&pexprlist->value.nodes, 0, &pexpr);
	if (nExpr == 1 && pexpr->type == LANGP_AST_NODE_CALL) {
		if (compile_expr(src, pexpr->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *pparamlist = pexpr->value.call.pexprlist;
		int nArg = pparamlist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *pparam;
			vector_get(&pparamlist->value.nodes, i, &pparam);
			if (compile_expr(src, pparam, pcs, dst)) {
				return 1;
			}
		}
		langC_emitop(dst, LANGV_OP_CALL);
		langC_emitinteger(dst, nArg);
		langC_emitinteger(dst, nVar);
		pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
		pscCurr->stackSize = stackBase + nVar;
	} else {
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (vector_get(&pexprlist->value.nodes, i, &pexpr)) {
				langC_emitop(dst, LANGV_OP_PUSHNIL);
				pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
				pscCurr->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	// Assign values
	pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	int stackSectionNewTop = stackBase;
	for (int i = 0; i < nVar; i++) {
		LangP_AstNode *pvar;
		vector_get(&pvarlist->value.nodes, i, &pvar);
		LangP_Token *ptokVar = pvar->value.ptoken;
		int len = ptokVar->value.string.length;
		char *identifier = malloc(len + 1);
		if (!identifier) {
			free(identifier);
			langC_errmsg(pcs, "failed to allocate");
			return 1;
		}
		memcpy(identifier, src + ptokVar->value.string.index, len);
		identifier[len] = '\0';

		int isDefinedVar = 0;
		int isUpvalue = 0;
		LangC_ScopeContext *pscOther;
		for (int i = pcs->scopeContexts.length - 1;; i--) {
			pscOther = vector_at(&pcs->scopeContexts, i);
			isDefinedVar = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
			if (isDefinedVar || i == 0) {
				break;
			}
			if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
				isUpvalue = 1;
			}
		}
		if (!isDefinedVar) {
			stringhashtable_put(&pscCurr->identifierTable, identifier, &stackSectionNewTop);
		} else if (isUpvalue) {
			int upvalueIndex;
			if (stringhashtable_get(&pscCurr->upvalueTable, identifier, &upvalueIndex) == 1) {
				upvalueIndex = pscCurr->upvalues.length;
				LangC_UpvalDesc ud;
				ud.type = 0; // for now
				stringhashtable_get(&pscOther->identifierTable, identifier, &ud.index);
				stringhashtable_put(&pscCurr->upvalueTable, identifier, &upvalueIndex);
				vector_push(&pscCurr->upvalues, &ud);
				nVar--; // workaround to disable emission of "POPN"
			}
			langC_emitop(dst, LANGV_OP_SETUPVALUE);
			langC_emitinteger(dst, upvalueIndex);
			pscCurr->stackSize--;
		} else {
			int stackIndex;
			stringhashtable_get(&pscOther->identifierTable, identifier, &stackIndex);
			if (i == nVar - 1 && stackSectionNewTop == stackBase + i) {
				langC_emitop(dst, LANGV_OP_SETLOCAL);
				langC_emitinteger(dst, stackIndex);
				pscCurr->stackSize--;
				nVar--; // workaround to disable emission of "POPN"
			} else {
				langC_emitop(dst, LANGV_OP_COPY);
				langC_emitinteger(dst, stackBase + i);
				langC_emitinteger(dst, stackIndex);
			}
		}
		free(identifier);
		if (stackSectionNewTop != stackBase + i) {
			langC_emitop(dst, LANGV_OP_COPY);
			langC_emitinteger(dst, stackBase + i);
			langC_emitinteger(dst, stackSectionNewTop);
		}
		if (!isDefinedVar) {
			stackSectionNewTop++;
		}
	}
	int stackSectionOldTop = stackBase + nVar;
	if (stackSectionNewTop != stackSectionOldTop) {
		int n = stackSectionOldTop - stackSectionNewTop;
		langC_emitop(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, n);
		pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
		pscCurr->stackSize -= n;
	}
	return 0;
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_LEAF:
		{
			LangC_ScopeContext *pscCurr = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
			LangP_Token *ptok = pnode->value.ptoken;
			switch (ptok->type) {
				case LANGP_TOK_NUMBER:
				{
					langC_emitop(dst, LANGV_OP_PUSHNUMBER);
					langC_emitdouble(dst, ptok->value.number);
					pscCurr->stackSize++;
				} break;
				case LANGP_TOK_STRING:
				{
					langC_emitop(dst, LANGV_OP_PUSHLSTRING);
					langC_emitlstring(dst, src + ptok->value.string.index, ptok->value.string.length);
					pscCurr->stackSize++;
				} break;
				case LANGP_TOK_IDENTIFIER:
				{
					int len = ptok->value.string.length;
					char *identifier = malloc(len + 1);
					if (!identifier) {
						free(identifier);
						langC_errmsg(pcs, "failed to allocate");
						return 1;
					}
					memcpy(identifier, src + ptok->value.string.index, len);
					identifier[len] = '\0';

					int isDefinedVar = 0;
					int isUpvalue = 0;
					LangC_ScopeContext *pscOther;
					for (int i = pcs->scopeContexts.length - 1;; i--) {
						pscOther = vector_at(&pcs->scopeContexts, i);
						isDefinedVar = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
						if (isDefinedVar || i == 0) {
							break;
						}
						if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
							isUpvalue = 1;
						}
					}
					if (!isDefinedVar) {
						langC_emitop(dst, LANGV_OP_PUSHNIL);
						pscCurr->stackSize++;
					} else if (isUpvalue) {
						int upvalueIndex;
						if (stringhashtable_get(&pscCurr->upvalueTable, identifier, &upvalueIndex) == 1) {
							upvalueIndex = pscCurr->upvalues.length;
							LangC_UpvalDesc ud;
							ud.type = 0; // useless for now
							stringhashtable_get(&pscOther->identifierTable, identifier, &ud.index);
							stringhashtable_put(&pscCurr->upvalueTable, identifier, &upvalueIndex);
							vector_push(&pscCurr->upvalues, &ud);
						}
						langC_emitop(dst, LANGV_OP_GETUPVALUE);
						langC_emitinteger(dst, upvalueIndex);
						pscCurr->stackSize++;
					} else {
						int stackIndex;
						stringhashtable_get(&pscOther->identifierTable, identifier, &stackIndex);
						langC_emitop(dst, LANGV_OP_GETLOCAL);
						langC_emitinteger(dst, stackIndex);
						pscCurr->stackSize++;
					} 
					free(identifier);
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
				case LANGP_AST_OP_LE:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_LE);
					break;
				case LANGP_AST_OP_GE:
					compile_binaryop_strict(src, pnode, pcs, dst, LANG_OP_GE);
					break;
				default:
					langC_errmsg(pcs, "unknown operation");
					return 1;
			}
		} break;
		case LANGP_AST_NODE_UNARYEXPR:
		{
			switch (pnode->value.unaryExpression.op) {
				case LANGP_AST_OP_NEG:
					compile_unaryop(src, pnode, pcs, dst, LANG_OP_NEG);
					break;
				case LANGP_AST_OP_NOT:
					compile_unaryop(src, pnode, pcs, dst, LANG_OP_NOT);
					break;
				case LANGP_AST_OP_LEN:
					compile_unaryop(src, pnode, pcs, dst, LANG_OP_LEN);
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
			push_scope_context(&pcs->scopeContexts, 0, LANGC_SCOPE_TYPE_FUNCTION);
			LangC_ScopeContext *pscInner = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
			int nArg = pnode->value.controlFunction.pvarlist->value.nodes.length;
			for (int i = 0; i < nArg; i++) {
				LangP_AstNode *pvar;
				vector_get(&pnode->value.controlFunction.pvarlist->value.nodes, i, &pvar);
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
				stringhashtable_put(&pscInner->identifierTable, str, &i);
				free(str);
			}
			pscInner->stackSize = nArg;
			if (compile_block(src, pnode->value.controlFunction.pblock, pcs, dst)) {
				return 1;
			}
			langC_emitop(dst, LANGV_OP_END);
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			
			pscInner = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
			langC_emitop(dst, LANGV_OP_PUSHLCLOSURE);
			langC_emitptr(dst, blockStart);
			langC_emitinteger(dst, pscInner->upvalues.length);
			for (int i = 0; i < pscInner->upvalues.length; i++) {
				LangC_UpvalDesc *pud = vector_at(&pscInner->upvalues, i);
				langC_emitbyte(dst, pud->type);
				langC_emitinteger(dst, pud->index);
			}
			pop_scope_context(&pcs->scopeContexts);

			LangC_ScopeContext *pscOuter = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
			pscOuter->stackSize++;
		} break;
		default:
			langC_errmsg(pcs, "expected LEAF, BINARYEXPR, or CALL node");
			return 1;
	}
	return 0;
}

int compile_statement_ifelseif(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	if (compile_expr(src, pnode->value.controlIfElseif.pexpr, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscOuter = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	langC_emitop(dst, LANGV_OP_JMPZ);
	size_t pcJmpnzWrite = dst->length;
	langC_emitinteger(dst, 1); // PLACEHOLDER
	pscOuter->stackSize--;

	size_t pcJmpnzFrom = dst->length;

	push_scope_context(&pcs->scopeContexts, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL);
	if (compile_block(src, pnode->value.controlIfElseif.pblock, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscInner = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	if (pscInner->stackSize != pscOuter->stackSize) {
		langC_emitop(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(&pcs->scopeContexts);

	LangP_AstNode *pnext = pnode->value.controlIfElseif.pnext;
	if (pnext != NULL) {
		langC_emitop(dst, LANGV_OP_JMP);
		size_t pcJmpWrite = dst->length;
		langC_emitinteger(dst, 1); // PLACEHOLDER
		size_t pcJmpFrom = dst->length;
		langC_writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
		if (compile_statement_ifelseif(src, pnext, pcs, dst)) {
			return 1;
		}
		langC_writeinteger(dst, pcJmpWrite, dst->length - pcJmpFrom);
	} else {
		langC_writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
	}
	return 0;
}

int compile_statement(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_CONTROL_IMPORT:
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
				langC_emitop(dst, LANGV_OP_IMPORT);
				langC_emitlstring(dst, str, len);
				LangC_ScopeContext *psc = vector_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
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
					langC_emitop(dst, LANGV_OP_SETLOCAL);
					langC_emitinteger(dst, stackIndex);
					psc->stackSize--;
				}
			}
		} break;
		case LANGP_AST_NODE_CONTROL_IFELSEIF:
		{
			if (compile_statement_ifelseif(src, pnode, pcs, dst)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_CONTROL_RETURN:
		{
			LangP_AstNode *pexprlist = pnode->value.pnode;
			int nReturn = pexprlist->value.nodes.length;
			LangP_AstNode *pexprFirst;
			vector_get(&pexprlist->value.nodes, 0, &pexprFirst);
			if (nReturn == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
				// Tail call
				if (compile_tailcall(src, pexprFirst, pcs, dst)) {
					return 1;
				}
			} else {
				// Ordinary return
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
			}
		} break;
		case LANGP_AST_NODE_BINARYEXPR:
			if (pnode->value.binaryExpression.op != LANGP_AST_OP_ASSIGN) {
				langC_errmsg(pcs, "expected control, assignment, or call");
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
			langC_errmsg(pcs, "expected control, assignment, or call");
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
	push_scope_context(&pcs->scopeContexts, 0, LANGC_SCOPE_TYPE_NORMAL);
	pcs->msg = NULL;
	return compile_block(src, root, pcs, dst);
}

void langC_free(LangC_CompilerState *pcs) {
	// todo: free all parts of psc's
	for (int i = 0; i < pcs->scopeContexts.length; i++) {
		LangC_ScopeContext *psc = vector_at(&pcs->scopeContexts, i);
		stringhashtable_free(&psc->identifierTable);
	}
	vector_free(&pcs->scopeContexts);
}


/*int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharVector *dst) {
	LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
	LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
	int nVar = pvarlist->value.nodes.length;
	LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
	int stackSizeBeforeAssignment = psc->stackSize;

	LangP_AstNode *pexprFirst;
	vector_get(&pexprlist->value.nodes, 0, &pexprFirst);
	if (pexprlist->value.nodes.length == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
		// Single call
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
				langC_emitchar(dst, LANGV_OP_SETLOCAL);
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
}*/