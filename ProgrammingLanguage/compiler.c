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

int push_scope_context(LangC_CompilerState *pcs, size_t stackSize, LangC_ScopeType type) {
	LangC_ScopeContext *psc = malloc(sizeof(LangC_ScopeContext));
	if (!psc) {
		goto push_scope_context_error;
	}
	if (stringhashtable_new(&psc->identifierTable, sizeof(int), 10)) {
		goto push_scope_context_error;
	}
	psc->stackSize = stackSize;
	psc->type = type;
	if (stringhashtable_new(&psc->upvalTable, sizeof(int), 10)) {
		goto push_scope_context_error;
	}
	if (list_new(&psc->upvals, sizeof(LangC_UpvalDesc), 10)) {
		goto push_scope_context_error;
	}
	psc->capturedLocalCount = 0;
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
	stringhashtable_free(&psc->upvalTable);
	list_free(&psc->upvals);
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
	LangP_AstNode *parglist = pnode->value.call.pexprlist;
	int nArg = parglist->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pArg;
		list_get(&parglist->value.nodes, i, &pArg);
		if (compile_expr(src, pArg, pcs, dst)) {
			return 1;
		}
	}
	langC_emitop(dst, LANGV_OP_CALL);
	langC_emitinteger(dst, nArg);
	langC_emitinteger(dst, nReturn);
	psc->stackSize = sizeB + nReturn;
	return 0;
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
		LangP_AstNode *parglist = pexpr->value.call.pexprlist;
		int nArg = parglist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *parg;
			list_get(&parglist->value.nodes, i, &parg);
			if (compile_expr(src, parg, pcs, dst)) {
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

			int isLocal = 1;
			for (int depth = pcs->scopeContexts.length - 1;; depth--) {
				LangC_ScopeContext *pscOther;
				list_get(&pcs->scopeContexts, depth, &pscOther);
				int isDefined = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
				if (isDefined) {
					if (isLocal) {
						int idxLocal;
						stringhashtable_get(&pscOther->identifierTable, identifier, &idxLocal);
						langC_emitop(dst, LANGV_OP_MOVE);
						langC_emitinteger(dst, sizeB + i);
						langC_emitinteger(dst, idxLocal);
					} else {
						int idxUpval;
						if (stringhashtable_get(&pscCurr->upvalTable, identifier, &idxUpval) == 1) {
							int idxScopeContextInner = pcs->scopeContexts.length - 1;
							for (;; idxScopeContextInner--) {
								LangC_ScopeContext *pscOther;
								list_get(&pcs->scopeContexts, idxScopeContextInner, &pscOther);
								if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
									break;
								}
							}
							// TODO: tidy up names
							for (;;) {
								int idxScopeContextOuter = idxScopeContextInner - 1;
								for (;; idxScopeContextOuter--) {
									LangC_ScopeContext *pscOther;
									list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOther);
									if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
										break;
									}
								}
								// Record upval
								LangC_ScopeContext *pscInner;
								list_get(&pcs->scopeContexts, idxScopeContextInner, &pscInner);
								LangC_ScopeContext *pscOuter;
								list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOuter);
								LangC_UpvalDesc ud;
								int idxUpvalOuter;
								int terminate;
								if (stringhashtable_get(&pscOuter->upvalTable, identifier, &idxUpvalOuter) == 0) {
									// OLD, terminate
									ud.type = LANG_UPVAL_OLD;
									ud.index = idxUpvalOuter;
									terminate = 1;
								} else if (idxScopeContextOuter <= depth) {
									// NEW, terminate
									LangC_ScopeContext *pscUpval;
									list_get(&pcs->scopeContexts, depth, &pscUpval);
									ud.type = LANG_UPVAL_NEW;
									stringhashtable_get(&pscUpval->identifierTable, identifier, &ud.index);
									terminate = 1;
									pscUpval->capturedLocalCount++;
								} else {
									// OLD
									idxUpvalOuter = pscOuter->upvals.length;
									ud.type = LANG_UPVAL_OLD;
									ud.index = idxUpvalOuter;
									terminate = 0;
								}
								int idxUpvalInner = pscInner->upvals.length;
								stringhashtable_put(&pscInner->upvalTable, identifier, &idxUpvalInner);
								list_push(&pscInner->upvals, &ud);
								if (terminate) {
									break;
								}
								idxScopeContextInner = idxScopeContextOuter;
							}
							stringhashtable_get(&pscCurr->upvalTable, identifier, &idxUpval);
						}
						langC_emitop(dst, LANGV_OP_MOVETOUPVAL);
						langC_emitinteger(dst, sizeB + i);
						langC_emitinteger(dst, idxUpval);
					}
					break;
				}
				if (depth == 0) {
					if (sizeB + i != sizeF) {
						langC_emitop(dst, LANGV_OP_MOVE);
						langC_emitinteger(dst, sizeB + i);
						langC_emitinteger(dst, sizeF);
					}
					stringhashtable_put(&pscCurr->identifierTable, identifier, &sizeF);
					sizeF++;
					break;
				}
				if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
					isLocal = 0;
				}
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
						langC_errmsg(pcs, "failed to allocate");
						return 1;
					}
					memcpy(identifier, src + ptok->value.string.index, len);
					identifier[len] = '\0';

					int isLocal = 1;
					for (int depth = pcs->scopeContexts.length - 1;; depth--) {
						LangC_ScopeContext *pscOther;
						list_get(&pcs->scopeContexts, depth, &pscOther);
						int isDefined = stringhashtable_containskey(&pscOther->identifierTable, identifier) == 0;
						if (isDefined) {
							if (isLocal) {
								int idxLocal;
								stringhashtable_get(&pscOther->identifierTable, identifier, &idxLocal);
								langC_emitop(dst, LANGV_OP_GETLOCAL);
								langC_emitinteger(dst, idxLocal);
								pscCurr->stackSize++;
							} else {
								int idxUpval;
								if (stringhashtable_get(&pscCurr->upvalTable, identifier, &idxUpval) == 1) {
									int idxScopeContextInner = pcs->scopeContexts.length - 1;
									for (;; idxScopeContextInner--) {
										LangC_ScopeContext *pscOther;
										list_get(&pcs->scopeContexts, idxScopeContextInner, &pscOther);
										if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
											break;
										}
									}
									for (;;) {
										int idxScopeContextOuter = idxScopeContextInner - 1;
										for (;; idxScopeContextOuter--) {
											LangC_ScopeContext *pscOther;
											list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOther);
											if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
												break;
											}
										}
										// Record upval
										LangC_ScopeContext *pscInner;
										list_get(&pcs->scopeContexts, idxScopeContextInner, &pscInner);
										LangC_ScopeContext *pscOuter;
										list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOuter);
										LangC_UpvalDesc ud;
										int idxUpvalOuter;
										int terminate;
										if (stringhashtable_get(&pscOuter->upvalTable, identifier, &idxUpvalOuter) == 0) {
											// OLD, terminate
											ud.type = LANG_UPVAL_OLD;
											ud.index = idxUpvalOuter;
											terminate = 1;
										} else if (idxScopeContextOuter <= depth) {
											// NEW, terminate
											LangC_ScopeContext *pscUpval;
											list_get(&pcs->scopeContexts, depth, &pscUpval);
											ud.type = LANG_UPVAL_NEW;
											stringhashtable_get(&pscUpval->identifierTable, identifier, &ud.index);
											terminate = 1;
											pscUpval->capturedLocalCount++;
										} else {
											// OLD
											idxUpvalOuter = pscOuter->upvals.length;
											ud.type = LANG_UPVAL_OLD;
											ud.index = idxUpvalOuter;
											terminate = 0;
										}
										int idxUpvalInner = pscInner->upvals.length;
										stringhashtable_put(&pscInner->upvalTable, identifier, &idxUpvalInner);
										list_push(&pscInner->upvals, &ud);

										if (terminate) {
											break;
										}
										idxScopeContextInner = idxScopeContextOuter;
									}
									stringhashtable_get(&pscCurr->upvalTable, identifier, &idxUpval);
								}
								langC_emitop(dst, LANGV_OP_GETUPVAL);
								langC_emitinteger(dst, idxUpval);
								pscCurr->stackSize++;
							}
							break;
						}
						if (depth == 0) {
							langC_emitop(dst, LANGV_OP_PUSHNULL);
							pscCurr->stackSize++;
							break;
						}
						if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
							isLocal = 0;
						}
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
			LangP_AstNode *pblock = pnode->value.controlFunction.pblock;
			if (compile_block(src, pblock, pcs, dst)) {
				return 1;
			}

			int emitEnd = 1;
			if (pblock->value.nodes.length > 0) {
				LangP_AstNode *last;
				list_get(&pblock->value.nodes, pblock->value.nodes.length - 1, &last);
				emitEnd = last->type != LANGP_AST_NODE_CONTROL_RETURN;
			}
			if (emitEnd) {
				if (pscInner->capturedLocalCount != 0) {
					langC_emitop(dst, LANGV_OP_CLOSEUPVALN);
					langC_emitinteger(dst, pscInner->capturedLocalCount);
				}
				langC_emitop(dst, LANGV_OP_END);
			}
			langC_writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);
			
			langC_emitop(dst, LANGV_OP_PUSHLCLOSURE);
			langC_emitptr(dst, blockStart);
			langC_emitinteger(dst, nParam); 
			langC_emitinteger(dst, pscInner->upvals.length);
			for (int i = 0; i < pscInner->upvals.length; i++) {
				LangC_UpvalDesc *pud = list_at(&pscInner->upvals, i);
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
	if (push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL)) {
		return 1;
	}
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

	if (push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL)) {
		return 1;
	}
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
				
				// Record as new variable
				int idxLocal = psc->stackSize - 1;
				stringhashtable_put(&psc->identifierTable, str, &idxLocal);
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
				if (compile_expr(src, pexprFirst->value.call.pexpr, pcs, dst)) {
					return 1;
				}
				LangP_AstNode *parglist = pexprFirst->value.call.pexprlist;
				int nArg = parglist->value.nodes.length;
				for (int i = 0; i < nArg; i++) {
					LangP_AstNode *parg;
					list_get(&parglist->value.nodes, i, &parg);
					if (compile_expr(src, parg, pcs, dst)) {
						return 1;
					}
				}
				LangC_ScopeContext *psc;
				list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
				if (psc->capturedLocalCount != 0) {
					langC_emitop(dst, LANGV_OP_CLOSEUPVALN);
					langC_emitinteger(dst, psc->capturedLocalCount);
				}
				langC_emitop(dst, LANGV_OP_TAILCALL);
				langC_emitinteger(dst, nArg);
			} else {
				// Ordinary return
				for (int i = 0; i < nReturn; i++) {
					LangP_AstNode *pexpr;
					list_get(&pexprlist->value.nodes, i, &pexpr);
					if (compile_expr(src, pexpr, pcs, dst)) {
						return 1;
					}
				}
				LangC_ScopeContext *psc;
				list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
				if (psc->capturedLocalCount != 0) {
					langC_emitop(dst, LANGV_OP_CLOSEUPVALN);
					langC_emitinteger(dst, psc->capturedLocalCount);
				}
				langC_emitop(dst, LANGV_OP_RETURN);
				langC_emitinteger(dst, nReturn);
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
	if (push_scope_context(pcs, 0, LANGC_SCOPE_TYPE_FUNCTION)) {
		return 1;
	}
	pcs->msg = NULL;
	return compile_block(src, root, pcs, dst);
}

void langC_free(LangC_CompilerState *pcs) {
	for (int i = 0; i < pcs->scopeContexts.length; i++) {
		LangC_ScopeContext *psc;
		list_get(&pcs->scopeContexts, i, &psc);
		free_scope_context(psc);
	}
	list_free(&pcs->scopeContexts);
}

