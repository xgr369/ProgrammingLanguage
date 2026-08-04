#include "compiler.h"
#include "vm.h"

static inline langC_emitop(CharList *pbc, char c) {
	charlist_push(pbc, c);
}
#define langC_emitchar(pbc, c) (charlist_push(dst, c))
#define langC_emitbyte(pbc, c) (charlist_push(pbc, c))
#define langC_emitliteral(pbc, l) (charlist_pusharray(pbc, "" l, sizeof(l) - 1))
static inline langC_emitdouble(CharList *pbc, double d) {
	charlist_pusharray(pbc, &d, sizeof(double));
}
static inline langC_emitinteger(CharList *pbc, int i) {
	charlist_pusharray(pbc, &i, sizeof(int));
}
static inline langC_writeinteger(CharList *pbc, int index, int i) {
	charlist_setarray(pbc, index, &i, sizeof(int));
}
static inline langC_emitlstring(CharList *pbc, const char *src, int l) {
	charlist_pusharray(pbc, &l, sizeof(int));
	charlist_pusharray(pbc, src, l);
}
static inline langC_emitptr(CharList *pbc, size_t ptr) {
	charlist_pusharray(pbc, &ptr, sizeof(size_t));
}
#define langC_errmsg(pcs, _msg) (pcs->msg = _msg)

void push_scope_context(LangC_CompilerState *pcs, size_t stackSize, LangC_ScopeType type) {
	LangC_ScopeContext *psc = malloc(sizeof(LangC_ScopeContext));
	if (!psc) {
		goto push_scope_context_error;
	}
	if (stringhashtable_new(&psc->identifierTable, sizeof(int), 10)) {
		goto push_scope_context_error;
	}
	psc->stackSize = stackSize;
	psc->type = type;
	if (stringhashtable_new(&psc->upvalueTable, sizeof(int), 10)) {
		goto push_scope_context_error;
	}
	if (list_new(&psc->upvalues, sizeof(LangC_UpvalDesc), 10)) {
		goto push_scope_context_error;
	}
	if (list_push(&pcs->scopeContexts, &psc)) {
		goto push_scope_context_error;
	}
	return 0;
	push_scope_context_error:
	langC_errmsg(pcs, "scope initialization failed");
	return 1;
}

void free_scope_context(LangC_ScopeContext *psc) {
	stringhashtable_free(&psc->identifierTable);
	stringhashtable_free(&psc->upvalueTable);
	list_free(&psc->upvalues);
}

void pop_scope_context(LangC_CompilerState *pcs) {
	LangC_ScopeContext *psc;
	list_pop(&pcs->scopeContexts, &psc);
	free_scope_context(psc);
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst);

int compile_call(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst, int nReturn) {
	LangC_ScopeContext *psc;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
	int sizeB = psc->stackSize;
	if (compile_expr(src, pnode->value.call.pexpr, pcs, dst)) {
		return 1;
	}
	LangP_AstNode *pexprlist = pnode->value.call.pexprlist;
	int nArg = pexprlist->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pexpr;
		list_get(&pexprlist->value.nodes, i, &pexpr);
		if (compile_expr(src, pexpr, pcs, dst)) {
			return 1;
		}
	}
	langC_emitop(dst, LANGV_OP_CALL);
	langC_emitinteger(dst, nArg);
	langC_emitinteger(dst, nReturn);
	psc->stackSize = sizeB + nReturn;
	return 0;
}

int compile_tailcall(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	if (compile_expr(src, pnode->value.call.pexpr, pcs, dst)) {
		return 1;
	}
	LangP_AstNode *pexprlistCall = pnode->value.call.pexprlist;
	int nArg = pexprlistCall->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pexpr;
		list_get(&pexprlistCall->value.nodes, i, &pexpr);
		if (compile_expr(src, pexpr, pcs, dst)) {
			return 1;
		}
	}
	langC_emitop(dst, LANGV_OP_TAILCALL);
	langC_emitinteger(dst, nArg);
}

int compile_binaryop_strict(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	if (compile_expr(src, pnode->value.binaryExpression.pright, pcs, dst)) {
		return 1;
	}
	langC_emitop(dst, LANGV_OP_BINARYOP);
	langC_emitbyte(dst, op);
	LangC_ScopeContext *psc;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
	psc->stackSize--;
	return 0;
}

int compile_unaryop(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	langC_emitop(dst, LANGV_OP_UNARYOP);
	langC_emitbyte(dst, op);
	return 0;
}

int compile_fieldexpr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst, int ignoreOutermost) {
	LangP_AstNode *pparent = pnode->value.fieldExpression.pparent;
	if (pparent->type == LANGP_AST_NODE_FIELDEXPR) {
		if (compile_fieldexpr(src, pparent, pcs, dst, 0)) {
			return 1;
		}
	} else {
		if (compile_expr(src, pparent, pcs, dst)) {
			return 1;
		}
	}
	if (!ignoreOutermost) {
		LangP_Token *ptok = pnode->value.fieldExpression.pchild->value.ptoken;
		langC_emitop(dst, LANGV_OP_FIELD);
		langC_emitlstring(dst, src + ptok->value.string.index, ptok->value.string.length);
	}
}

int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	LangP_AstNode *pvarlist = pnode->value.assignment.pvarlist;
	LangP_AstNode *pexprlist = pnode->value.assignment.pexprlist;
	int nVar = pvarlist->value.nodes.length;
	int nExpr = pexprlist->value.nodes.length;
	LangC_ScopeContext *pscCurr;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
	int sizeB = pscCurr->stackSize;

	LangP_AstNode *pexpr;
	list_get(&pexprlist->value.nodes, 0, &pexpr);
	if (nExpr == 1 && pexpr->type == LANGP_AST_NODE_CALL) {
		if (compile_expr(src, pexpr->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *pparamlist = pexpr->value.call.pexprlist;
		int nArg = pparamlist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *pparam;
			list_get(&pparamlist->value.nodes, i, &pparam);
			if (compile_expr(src, pparam, pcs, dst)) {
				return 1;
			}
		}
		langC_emitop(dst, LANGV_OP_CALL);
		langC_emitinteger(dst, nArg);
		langC_emitinteger(dst, nVar);
		pscCurr->stackSize = sizeB + nVar;
	} else {
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (list_get(&pexprlist->value.nodes, i, &pexpr)) {
				langC_emitop(dst, LANGV_OP_PUSHNULL);
				pscCurr->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	// Assign values
	int sizeT = pscCurr->stackSize;
	int sizeF = sizeB;
	for (int i = 0; i < nVar; i++) {
		LangP_AstNode *pvar;
		list_get(&pvarlist->value.nodes, i, &pvar);
		if (pvar->type == LANGP_AST_NODE_LEAF) {
			LangP_Token *ptok = pvar->value.ptoken;
			int len = ptok->value.string.length;
			char *identifier = malloc(len + 1);
			if (!identifier) {
				langC_errmsg(pcs, "failed to allocate");
				return 1;
			}
			memcpy(identifier, src + ptok->value.string.index, len);
			identifier[len] = '\0';

			int isDefinedVar = 0;
			int isUpvalue = 0;
			LangC_ScopeContext *pscOther;
			LangC_ScopeContext *pscFunction = NULL;
			for (int i = pcs->scopeContexts.length - 1;; i--) {
				list_get(&pcs->scopeContexts, i, &pscOther);
				isDefinedVar = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
				if (isDefinedVar || i == 0) {
					break;
				}
				if (!isUpvalue && pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
					isUpvalue = 1;
					pscFunction = pscOther;
				}
			}

			if (!isDefinedVar) {
				if (sizeB + i != sizeF) {
					langC_emitop(dst, LANGV_OP_MOVE);
					langC_emitinteger(dst, sizeB + i);
					langC_emitinteger(dst, sizeF);
				}
				stringhashtable_put(&pscCurr->identifierTable, identifier, &sizeF);
				sizeF++;
			} else if (isUpvalue) {
				int idxUpvalue;
				if (stringhashtable_get(&pscFunction->upvalueTable, identifier, &idxUpvalue) == 1) {
					idxUpvalue = pscFunction->upvalues.length;
					LangC_UpvalDesc ud;
					ud.type = LANG_UPVAL_STACK; // for now
					stringhashtable_get(&pscOther->identifierTable, identifier, &ud.index);
					stringhashtable_put(&pscFunction->upvalueTable, identifier, &idxUpvalue);
					list_push(&pscFunction->upvalues, &ud);
				}
				langC_emitop(dst, LANGV_OP_MOVETOUPVALUE);
				langC_emitinteger(dst, sizeB + i);
				langC_emitinteger(dst, idxUpvalue);
			} else {
				int indexStack;
				stringhashtable_get(&pscOther->identifierTable, identifier, &indexStack);
				langC_emitop(dst, LANGV_OP_MOVE);
				langC_emitinteger(dst, sizeB + i);
				langC_emitinteger(dst, indexStack);
			}
			free(identifier);
		} else if (pvar->type == LANGP_AST_NODE_FIELDEXPR) {
			if (compile_fieldexpr(src, pvar, pcs, dst, 1)) {
				return 1;
			}
			LangP_Token *ptok = pvar->value.fieldExpression.pchild->value.ptoken;
			langC_emitop(dst, LANGV_OP_MOVETOFIELD);
			langC_emitinteger(dst, sizeB + i);
			langC_emitlstring(dst, src + ptok->value.string.index, ptok->value.string.length);
		}
	}

	if (sizeF != sizeT) {
		int n = sizeT - sizeF;
		langC_emitop(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, n);
		pscCurr->stackSize -= n;
	}
	return 0;
}

int compile_expr(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_LEAF:
		{
			LangC_ScopeContext *pscCurr;
			list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
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
					LangC_ScopeContext *pscFunction = NULL;
					for (int i = pcs->scopeContexts.length - 1;; i--) {
						list_get(&pcs->scopeContexts, i, &pscOther);
						isDefinedVar = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
						if (isDefinedVar || i == 0) {
							break;
						}
						if (!isUpvalue && pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
							isUpvalue = 1;
							pscFunction = pscOther;
						}
					}
					if (!isDefinedVar) {
						langC_emitop(dst, LANGV_OP_PUSHNULL);
						pscCurr->stackSize++;
					} else if (isUpvalue) {
						int upvalueIndex;
						if (stringhashtable_get(&pscFunction->upvalueTable, identifier, &upvalueIndex) == 1) {
							upvalueIndex = pscFunction->upvalues.length;
							LangC_UpvalDesc ud;
							ud.type = LANG_UPVAL_STACK; // useless for now
							stringhashtable_get(&pscOther->identifierTable, identifier, &ud.index);
							stringhashtable_put(&pscFunction->upvalueTable, identifier, &upvalueIndex);
							list_push(&pscFunction->upvalues, &ud);
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
			push_scope_context(pcs, 0, LANGC_SCOPE_TYPE_FUNCTION);
			LangC_ScopeContext *pscInner;
			list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
			int nParam = pnode->value.controlFunction.pparamlist->value.nodes.length;
			for (int i = 0; i < nParam; i++) {
				LangP_AstNode *pvar;
				list_get(&pnode->value.controlFunction.pparamlist->value.nodes, i, &pvar);
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
			pscInner->stackSize = nParam;
			if (compile_block(src, pnode->value.controlFunction.pblock, pcs, dst)) {
				return 1;
			}
			langC_emitop(dst, LANGV_OP_END);
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			
			langC_emitop(dst, LANGV_OP_PUSHLCLOSURE);
			langC_emitptr(dst, blockStart);
			langC_emitinteger(dst, nParam); 
			langC_emitinteger(dst, pscInner->upvalues.length);
			for (int i = 0; i < pscInner->upvalues.length; i++) {
				LangC_UpvalDesc *pud = list_at(&pscInner->upvalues, i);
				langC_emitbyte(dst, pud->type);
				langC_emitinteger(dst, pud->index);
			}
			pop_scope_context(pcs);

			LangC_ScopeContext *pscOuter;
			list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
			pscOuter->stackSize++;
		} break;
		case LANGP_AST_NODE_FIELDEXPR:
		{
			if (compile_fieldexpr(src, pnode, pcs, dst, 0)) {
				return 1;
			}
		} break;
		default:
			langC_errmsg(pcs, "expected LEAF, BINARYEXPR, or CALL node");
			return 1;
	}
	return 0;
}


int compile_statement_else(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	LangC_ScopeContext *pscOuter;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
	push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL);
	if (compile_block(src, pnode->value.controlElse.pblock, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscInner;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
	if (pscInner->stackSize != pscOuter->stackSize) {
		langC_emitop(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(pcs);
	return 0;
}

int compile_statement_ifelseif(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	if (compile_expr(src, pnode->value.controlIfElseif.pexpr, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscOuter;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
	langC_emitop(dst, LANGV_OP_JMPZ);
	size_t pcJmpnzWrite = dst->length;
	langC_emitinteger(dst, 1); // PLACEHOLDER
	pscOuter->stackSize--;

	size_t pcJmpnzFrom = dst->length;

	push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL);
	if (compile_block(src, pnode->value.controlIfElseif.pblock, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscInner;
	list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
	if (pscInner->stackSize != pscOuter->stackSize) {
		langC_emitop(dst, LANGV_OP_POPN);
		langC_emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(pcs);

	LangP_AstNode *pnext = pnode->value.controlIfElseif.pnext;
	if (pnext != NULL) {
		langC_emitop(dst, LANGV_OP_JMP);
		size_t pcJmpWrite = dst->length;
		langC_emitinteger(dst, 1); // PLACEHOLDER
		size_t pcJmpFrom = dst->length;
		langC_writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
		if (pnext->type == LANGP_AST_NODE_CONTROL_ELSE) {
			if (compile_statement_else(src, pnext, pcs, dst)) {
				return 1;
			}
		} else if (pnext->type == LANGP_AST_NODE_CONTROL_IFELSEIF) {
			if (compile_statement_ifelseif(src, pnext, pcs, dst)) {
				return 1;
			}
		}
		langC_writeinteger(dst, pcJmpWrite, dst->length - pcJmpFrom);
	} else {
		langC_writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
	}
	return 0;
}

int compile_statement(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_CONTROL_IMPORT:
		{
			LangP_AstNode *pvarlist = pnode->value.pnode;
			int nVar = pvarlist->value.nodes.length;
			for (int i = 0; i < nVar; i++) {
				LangP_AstNode *pvar;
				list_get(&pvarlist->value.nodes, i, &pvar);
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
				LangC_ScopeContext *psc;
				list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
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
			list_get(&pexprlist->value.nodes, 0, &pexprFirst);
			if (nReturn == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
				// Tail call
				if (compile_tailcall(src, pexprFirst, pcs, dst)) {
					return 1;
				}
			} else {
				// Ordinary return
				for (int i = 0; i < nReturn; i++) {
					LangP_AstNode *pexpr;
					list_get(&pexprlist->value.nodes, i, &pexpr);
					if (compile_expr(src, pexpr, pcs, dst)) {
						return 1;
					}
				}
				langC_emitop(dst, LANGV_OP_RETURN);
				langC_emitinteger(dst, nReturn);
				// decrement stack size?
			}
		} break;
		case LANGP_AST_NODE_ASSIGNMENT:
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

int compile_block(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	if (pnode->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "expected BLOCK node");
		return 1;
	}
	for (int i = 0; i < pnode->value.nodes.length; i++) {
		LangP_AstNode *psubnode;
		list_get(&pnode->value.nodes, i, &psubnode);
		if (compile_statement(src, psubnode, pcs, dst))
			return 1;
	}
	return 0;
}

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, CharList *dst) {
	if (!root || root->type != LANGP_AST_NODE_BLOCK) {
		langC_errmsg(pcs, "invalid root node");
		return 1;
	}
	list_new(&pcs->scopeContexts, sizeof(LangC_ScopeContext *), 1);
	push_scope_context(pcs, 0, LANGC_SCOPE_TYPE_NORMAL);
	pcs->msg = NULL;
	return compile_block(src, root, pcs, dst);
}

void langC_free(LangC_CompilerState *pcs) {
	// todo: free all parts of psc's
	for (int i = 0; i < pcs->scopeContexts.length; i++) {
		LangC_ScopeContext *psc;
		list_get(&pcs->scopeContexts, i, &psc);
		free_scope_context(psc);
	}
	list_free(&pcs->scopeContexts);
}


/*int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
	LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
	int nVar = pvarlist->value.nodes.length;
	LangC_ScopeContext *psc = pcs->scopeContexts.data + sizeof(LangC_ScopeContext) * (pcs->scopeContexts.length - 1);
	int stackSizeBeforeAssignment = psc->stackSize;

	LangP_AstNode *pexprFirst;
	list_get(&pexprlist->value.nodes, 0, &pexprFirst);
	if (pexprlist->value.nodes.length == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
		// Single call
		if (compile_expr(src, pexprFirst->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *pexprlistCall = pexprFirst->value.call.pexprlist;
		int nArg = pexprlistCall->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *pexpr;
			list_get(&pexprlistCall->value.nodes, i, &pexpr);
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
			if (list_get(&pexprlist->value.nodes, i, &pexpr)) {
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
		list_get(&pvarlist->value.nodes, i, &pvar);
		LangP_Token *ptokVar = pvar->value.ptoken;
		int len = ptokVar->value.string.length;
		char *str = malloc(len + 1);
		if (!str) {
			free(str);
			fmsg(pcs, "failed to allocate");
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

/*
* int compile_assignment(char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, CharList *dst) {
	LangP_AstNode *pvarlist = pnode->value.binaryExpression.pleft;
	LangP_AstNode *pexprlist = pnode->value.binaryExpression.pright;
	int nVar = pvarlist->value.nodes.length;
	int nExpr = pexprlist->value.nodes.length;
	LangC_ScopeContext *pscCurr = list_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	int stackBase = pscCurr->stackSize;

	LangP_AstNode *pexpr;
	list_get(&pexprlist->value.nodes, 0, &pexpr);
	if (nExpr == 1 && pexpr->type == LANGP_AST_NODE_CALL) {
		if (compile_expr(src, pexpr->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *pparamlist = pexpr->value.call.pexprlist;
		int nArg = pparamlist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *pparam;
			list_get(&pparamlist->value.nodes, i, &pparam);
			if (compile_expr(src, pparam, pcs, dst)) {
				return 1;
			}
		}
		langC_emitop(dst, LANGV_OP_CALL);
		langC_emitinteger(dst, nArg);
		langC_emitinteger(dst, nVar);
		pscCurr = list_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
		pscCurr->stackSize = stackBase + nVar;
	} else {
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (list_get(&pexprlist->value.nodes, i, &pexpr)) {
				langC_emitop(dst, LANGV_OP_PUSHNIL);
				pscCurr = list_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
				pscCurr->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	// Assign values
	pscCurr = list_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
	int stackSectionNewTop = stackBase;
	for (int i = 0; i < nVar; i++) {
		LangP_AstNode *pvar;
		list_get(&pvarlist->value.nodes, i, &pvar);
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
			pscOther = list_at(&pcs->scopeContexts, i);
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
				list_push(&pscCurr->upvalues, &ud);
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
		pscCurr = list_at(&pcs->scopeContexts, pcs->scopeContexts.length - 1);
		pscCurr->stackSize -= n;
	}
	return 0;
}*/