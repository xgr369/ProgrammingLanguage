#include "compiler.h"
#include "vm.h"

static inline emitop(LangM_CharList *pbc, char c) {
	langM_charlist_push(pbc, c);
}
#define emitchar(pbc, c) (langM_charlist_push(dst, c))
#define emitbyte(pbc, c) (langM_charlist_push(pbc, c))
#define emitliteral(pbc, l) (langM_charlist_pusharray(pbc, "" l, sizeof(l) - 1))
static inline emitdouble(LangM_CharList *pbc, double d) {
	langM_charlist_pusharray(pbc, &d, sizeof(double));
}
static inline emitinteger(LangM_CharList *pbc, int i) {
	langM_charlist_pusharray(pbc, &i, sizeof(int));
}
static inline writeinteger(LangM_CharList *pbc, int index, int i) {
	langM_charlist_setarray(pbc, index, &i, sizeof(int));
}
static inline emitlstring(LangM_CharList *pbc, int l, const const char *src) {
	langM_charlist_pusharray(pbc, &l, sizeof(int));
	langM_charlist_pusharray(pbc, src, l);
}
#define langC_errmsg(pcs, _msg) (pcs->msg = _msg)

static int push_scope_context(LangC_CompilerState *pcs, size_t stackSize, LangC_ScopeType type) {
	LangC_ScopeContext *psc = malloc(sizeof(LangC_ScopeContext));
	if (!psc) {
		langC_errmsg(pcs, "allocation failed");
		return 1;
	}
	if (langM_table_init(&psc->identifierTable, sizeof(int), 10)) {
		langC_errmsg(pcs, "internal error");
		return 1;
	}
	psc->stackSize = stackSize;
	psc->type = type;
	if (langM_table_init(&psc->upvalTable, sizeof(int), 10)) {
		langC_errmsg(pcs, "internal error");
		return 1;
	}
	if (langM_list_init(&psc->upvals, sizeof(LangC_UpvalDesc), 10)) {
		langC_errmsg(pcs, "internal error");
		return 1;
	}
	psc->hasCapturedVariables = 0;
	if (langM_list_push(&pcs->scopeContexts, &psc)) {
		langC_errmsg(pcs, "internal error");
		return 1;
	}
	return 0;
}

static void free_scope_context(LangC_ScopeContext *psc) {
	langM_table_free(&psc->identifierTable);
	langM_table_free(&psc->upvalTable);
	langM_list_free(&psc->upvals);
}

static void pop_scope_context(LangC_CompilerState *pcs) {
	LangC_ScopeContext *psc;
	langM_list_pop(&pcs->scopeContexts, &psc);
	free_scope_context(psc);
}

static int compile_expr(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst);

static int compile_call(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst, int nReturn) {
	LangC_ScopeContext *psc;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
	int sizeB = psc->stackSize;
	if (compile_expr(src, pnode->value.call.pexpr, pcs, dst)) {
		return 1;
	}
	LangP_AstNode *parglist = pnode->value.call.pexprlist;
	int nArg = parglist->value.nodes.length;
	for (int i = 0; i < nArg; i++) {
		LangP_AstNode *pArg;
		langM_list_get(&parglist->value.nodes, i, &pArg);
		if (compile_expr(src, pArg, pcs, dst)) {
			return 1;
		}
	}
	emitop(dst, LANGV_OP_CALL);
	emitinteger(dst, nArg);
	emitinteger(dst, nReturn);
	psc->stackSize = sizeB + nReturn;
	return 0;
}

static int compile_binaryop(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst, char op) {
	LangC_ScopeContext *psc;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	if (compile_expr(src, pnode->value.binaryExpression.pright, pcs, dst)) {
		return 1;
	}
	emitop(dst, LANGV_OP_BINARYOP);
	emitbyte(dst, op);
	psc->stackSize--;
	return 0;
}

static int compile_unaryop(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst, char op) {
	if (compile_expr(src, pnode->value.binaryExpression.pleft, pcs, dst)) {
		return 1;
	}
	emitop(dst, LANGV_OP_UNARYOP);
	emitbyte(dst, op);
	return 0;
}

static int compile_fieldexpr(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst, int ignoreOutermost) {
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
		emitop(dst, LANGV_OP_GETFIELD);
		emitlstring(dst, ptok->value.string.length, src + ptok->value.string.index);
	}
}

int compile_string(const char *src, LangP_Token *ptok, LangC_CompilerState *pcs, LangM_CharList *dst) {
	LangC_ScopeContext *pscCurr;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
	int maxLen = ptok->value.string.length;
	char *buf = malloc(maxLen + 1);
	if (!buf) {
		langC_errmsg(pcs, "allocation failed");
		return 1;
	}
	int len = 0;
	int escape = 0;
	for (int i = 0; i < maxLen; i++) {
		char c = src[ptok->value.string.index + i];
		if (escape) {
			escape = 0;
			switch (c) {
				case '0':
					c = '\0';
					break;
				case '\\':
					break;
				default:
					langC_errmsg(pcs, "unrecognized escape sequence");
					free(buf);
					return 1;
			}
		} else if (c == '\\') {
			escape = 1;
			continue;
		}
		buf[len++] = c;
	}
	emitop(dst, LANGV_OP_PUSHLSTRING);
	emitlstring(dst, len, buf);
	pscCurr->stackSize++;
	free(buf);
	return 0;
}

static int compile_expr(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	switch (pnode->type) {
		case LANGP_AST_NODE_LEAF: {
			LangC_ScopeContext *pscCurr;
			langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
			LangP_Token *ptok = pnode->value.ptoken;
			switch (ptok->type) {
				case LANGP_TOK_KEYWORD: {
					if (ptok->value.tag == LANGP_TOK_KEYWORD_NULL) {
						emitop(dst, LANGV_OP_PUSHNULL);
						pscCurr->stackSize++;
					} else if (ptok->value.tag == LANGP_TOK_KEYWORD_THIS) {
						emitop(dst, LANGV_OP_PUSHTHIS);
						pscCurr->stackSize++;
					}
				} break;
				case LANGP_TOK_NUMBER: {
					emitop(dst, LANGV_OP_PUSHNUMBER);
					emitdouble(dst, ptok->value.number);
					pscCurr->stackSize++;
				} break;
				case LANGP_TOK_STRING: {
					if (compile_string(src, ptok, pcs, dst)) {
						return 1;
					}
				} break;
				case LANGP_TOK_IDENTIFIER: {
					int len = ptok->value.string.length;
					char *identifier = malloc(len + 1);
					if (!identifier) {
						langC_errmsg(pcs, "allocation failed");
						return 1;
					}
					memcpy(identifier, src + ptok->value.string.index, len);
					identifier[len] = '\0';

					int isLocal = 1;
					for (int depth = pcs->scopeContexts.length - 1;; depth--) {
						LangC_ScopeContext *pscOther;
						langM_list_get(&pcs->scopeContexts, depth, &pscOther);
						int isDefined = langM_table_containskey(&pscOther->identifierTable, identifier) == 0;
						if (isDefined) {
							if (isLocal) {
								int idxLocal;
								langM_table_get(&pscOther->identifierTable, identifier, &idxLocal);
								emitop(dst, LANGV_OP_GETLOCAL);
								emitinteger(dst, idxLocal);
								pscCurr->stackSize++;
							} else {
								int idxUpval;
								if (langM_table_get(&pscCurr->upvalTable, identifier, &idxUpval) == 1) {
									int idxScopeContextInner = pcs->scopeContexts.length - 1;
									for (;; idxScopeContextInner--) {
										LangC_ScopeContext *pscOther;
										langM_list_get(&pcs->scopeContexts, idxScopeContextInner, &pscOther);
										if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
											break;
										}
									}
									int innermost = 1;
									for (;;) {
										int idxScopeContextOuter = idxScopeContextInner - 1;
										for (;; idxScopeContextOuter--) {
											LangC_ScopeContext *pscOther;
											langM_list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOther);
											if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
												break;
											}
										}
										// Record upval
										LangC_ScopeContext *pscInner;
										langM_list_get(&pcs->scopeContexts, idxScopeContextInner, &pscInner);
										LangC_ScopeContext *pscOuter;
										langM_list_get(&pcs->scopeContexts, idxScopeContextOuter, &pscOuter);
										LangC_UpvalDesc ud;
										int idxUpvalOuter;
										int terminate;
										if (langM_table_get(&pscOuter->upvalTable, identifier, &idxUpvalOuter) == 0) {
											// OLD, terminate -- already recorded in pscOuter
											ud.type = LANG_UPVAL_INDIRECT;
											ud.index = idxUpvalOuter;
											terminate = 1;
										} else if (idxScopeContextOuter <= depth) {
											// NEW, terminate
											LangC_ScopeContext *pscUpval;
											langM_list_get(&pcs->scopeContexts, depth, &pscUpval);
											ud.type = LANG_UPVAL_DIRECT;
											langM_table_get(&pscUpval->identifierTable, identifier, &ud.index);
											terminate = 1;
											pscUpval->hasCapturedVariables = 1;
										} else {
											// OLD
											idxUpvalOuter = pscOuter->upvals.length;
											ud.type = LANG_UPVAL_INDIRECT;
											ud.index = idxUpvalOuter;
											terminate = 0;
										}
										int idxUpvalInner = pscInner->upvals.length;
										langM_table_put(&pscInner->upvalTable, identifier, &idxUpvalInner);
										langM_list_push(&pscInner->upvals, &ud);
										if (innermost) {
											langM_table_get(&pscInner->upvalTable, identifier, &idxUpval);
										}
										if (terminate) {
											break;
										}
										idxScopeContextInner = idxScopeContextOuter;
										innermost = 0;
									}
								}
								emitop(dst, LANGV_OP_GETUPVAL);
								emitinteger(dst, idxUpval);
								pscCurr->stackSize++;
							}
							break;
						}
						if (depth == 0) {
							free(identifier);
							langC_errmsg(pcs, "variable is not defined");
							return 1;
						}
						if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
							isLocal = 0;
						}
					}

					free(identifier);
				} break;
			}
		} break;
		case LANGP_AST_NODE_BINARYEXPR: {
			switch (pnode->value.binaryExpression.op) {
				case LANGP_AST_OP_ADD:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_ADD);
				case LANGP_AST_OP_SUB:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_SUB);
				case LANGP_AST_OP_MUL:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_MUL);
				case LANGP_AST_OP_DIV:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_DIV);
				case LANGP_AST_OP_EQ:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_EQ);
				case LANGP_AST_OP_LT:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_LT);
				case LANGP_AST_OP_GT:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_GT);
				case LANGP_AST_OP_LE:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_LE);
				case LANGP_AST_OP_GE:
					return compile_binaryop(src, pnode, pcs, dst, LANG_OP_GE);
				default:
					assert(0);
					return 1;
			}
		} break;
		case LANGP_AST_NODE_CALL: {
			if (compile_call(src, pnode, pcs, dst, 1)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_FIELDEXPR: {
			if (compile_fieldexpr(src, pnode, pcs, dst, 0)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_FUNCTIONEXPR: {
			emitop(dst, LANGV_OP_JMP);
			emitinteger(dst, 1); // PLACEHOLDER

			int blockStart = dst->length;
			push_scope_context(pcs, 0, LANGC_SCOPE_TYPE_FUNCTION);
			LangC_ScopeContext *pscInner;
			langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
			int nParam = pnode->value.functionExpression.pparamlist->value.nodes.length;
			for (int i = 0; i < nParam; i++) {
				LangP_AstNode *pvar;
				langM_list_get(&pnode->value.functionExpression.pparamlist->value.nodes, i, &pvar);
				LangP_Token *ptok = pvar->value.ptoken;
				int len = ptok->value.string.length;
				char *identifier = malloc(len + 1);
				if (!identifier) {
					free(identifier);
					langC_errmsg(pcs, "allocation failed");
					return 1;
				}
				memcpy(identifier, src + ptok->value.string.index, len);
				identifier[len] = '\0';
				langM_table_put(&pscInner->identifierTable, identifier, &i);
				free(identifier);
			}
			pscInner->stackSize = nParam;
			LangP_AstNode *pblock = pnode->value.functionExpression.pblock;
			if (compile_block(src, pblock, pcs, dst)) {
				return 1;
			}
			int emitEnd = 1;
			if (pblock->value.nodes.length > 0) {
				LangP_AstNode *last;
				langM_list_get(&pblock->value.nodes, pblock->value.nodes.length - 1, &last);
				emitEnd = last->type != LANGP_AST_NODE_RETURN;
			}
			if (emitEnd) {
				emitop(dst, LANGV_OP_RETURN);
				emitinteger(dst, 0);
			}
			writeinteger(dst, blockStart - sizeof(int), dst->length - blockStart);

			emitop(dst, LANGV_OP_PUSHFUNCTION);
			emitinteger(dst, blockStart);
			emitinteger(dst, nParam);
			emitinteger(dst, pscInner->upvals.length);
			for (int i = 0; i < pscInner->upvals.length; i++) {
				LangC_UpvalDesc *pud = langM_list_at(&pscInner->upvals, i);
				emitbyte(dst, pud->type);
				emitinteger(dst, pud->index);
			}
			pop_scope_context(pcs);

			LangC_ScopeContext *pscOuter;
			langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
			pscOuter->stackSize++;
		} break;
		case LANGP_AST_NODE_TABLEEXPR: {
			int nProperty = pnode->value.nodes.length;
			for (int i = 0; i < nProperty; i++) {
				LangP_AstNode *pproperty;
				langM_list_get(&pnode->value.nodes, i, &pproperty);

				// TODO compile property
				// var a = {foo:bar}
				/* pushtable
				*  [compile_expr(bar)]
				*  getlocal ...
				*  setfield "foo"
				*/
			}
		} break;
		case LANGP_AST_NODE_UNARYEXPR: {
			switch (pnode->value.unaryExpression.op) {
				case LANGP_AST_OP_NEG:
					return compile_unaryop(src, pnode, pcs, dst, LANG_OP_NEG);
				case LANGP_AST_OP_NOT:
					return compile_unaryop(src, pnode, pcs, dst, LANG_OP_NOT);
				case LANGP_AST_OP_LEN:
					return compile_unaryop(src, pnode, pcs, dst, LANG_OP_LEN);
				default:
					assert(0);
					return 1;
			}
		} break;
		default:
			assert(0);
			return 1;
	}
	return 0;
}

static int compile_declaration(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	LangP_AstNode *pidentifierlist = pnode->value.declaration.pidentifierlist;
	LangP_AstNode *pexprlist = pnode->value.declaration.pexprlist;
	LangC_ScopeContext *pscCurr;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
	int nVar = pidentifierlist->value.nodes.length;
	int nExpr = pexprlist->value.nodes.length;
	int sizeB = pscCurr->stackSize;

	LangP_AstNode *pexpr;
	langM_list_get(&pexprlist->value.nodes, 0, &pexpr);
	if (nExpr == 1 && pexpr->type == LANGP_AST_NODE_CALL) {
		if (compile_expr(src, pexpr->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *parglist = pexpr->value.call.pexprlist;
		int nArg = parglist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *parg;
			langM_list_get(&parglist->value.nodes, i, &parg);
			if (compile_expr(src, parg, pcs, dst)) {
				return 1;
			}
		}
		emitop(dst, LANGV_OP_CALL);
		emitinteger(dst, nArg);
		emitinteger(dst, nVar);
		pscCurr->stackSize = sizeB + nVar;
	} else {
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (langM_list_get(&pexprlist->value.nodes, i, &pexpr)) {
				emitop(dst, LANGV_OP_PUSHNULL);
				pscCurr->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	for (int i = 0; i < nVar; i++) {
		LangP_AstNode *pvar;
		langM_list_get(&pidentifierlist->value.nodes, i, &pvar);
		LangP_Token *ptok = pvar->value.ptoken;
		int len = ptok->value.string.length;
		char *identifier = malloc(len + 1);
		if (!identifier) {
			langC_errmsg(pcs, "allocation failed");
			return 1;
		}
		memcpy(identifier, src + ptok->value.string.index, len);
		identifier[len] = '\0';

		int idxLocal = sizeB + i;
		langM_table_put(&pscCurr->identifierTable, identifier, &idxLocal);

		free(identifier);
	}
	return 0;
}

static int compile_assignment(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	LangP_AstNode *pvarlist = pnode->value.assignment.pvarlist;
	LangP_AstNode *pexprlist = pnode->value.assignment.pexprlist;
	LangC_ScopeContext *pscCurr;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscCurr);
	int nVar = pvarlist->value.nodes.length;
	int nExpr = pexprlist->value.nodes.length;
	int sizeB = pscCurr->stackSize;

	LangP_AstNode *pexpr;
	langM_list_get(&pexprlist->value.nodes, 0, &pexpr);
	if (nExpr == 1 && pexpr->type == LANGP_AST_NODE_CALL) {
		if (compile_expr(src, pexpr->value.call.pexpr, pcs, dst)) {
			return 1;
		}
		LangP_AstNode *parglist = pexpr->value.call.pexprlist;
		int nArg = parglist->value.nodes.length;
		for (int i = 0; i < nArg; i++) {
			LangP_AstNode *parg;
			langM_list_get(&parglist->value.nodes, i, &parg);
			if (compile_expr(src, parg, pcs, dst)) {
				return 1;
			}
		}
		emitop(dst, LANGV_OP_CALL);
		emitinteger(dst, nArg);
		emitinteger(dst, nVar);
		pscCurr->stackSize = sizeB + nVar;
	} else {
		for (int i = 0; i < nVar; i++) {
			LangP_AstNode *pexpr;
			if (langM_list_get(&pexprlist->value.nodes, i, &pexpr)) {
				emitop(dst, LANGV_OP_PUSHNULL);
				pscCurr->stackSize++;
			} else {
				if (compile_expr(src, pexpr, pcs, dst)) {
					return 1;
				}
			}
		}
	}

	// Assign values
	int i = nVar - 1;
	for (;;) {
		LangP_AstNode *pvar;
		langM_list_get(&pvarlist->value.nodes, i, &pvar);
		if (pvar->type == LANGP_AST_NODE_LEAF) {
			LangP_Token *ptok = pvar->value.ptoken;
			int len = ptok->value.string.length;
			char *identifier = malloc(len + 1);
			if (!identifier) {
				langC_errmsg(pcs, "allocation failed");
				return 1;
			}
			memcpy(identifier, src + ptok->value.string.index, len);
			identifier[len] = '\0';

			int isLocal = 1;
			for (int depth = pcs->scopeContexts.length - 1;; depth--) {
				LangC_ScopeContext *pscOther;
				langM_list_get(&pcs->scopeContexts, depth, &pscOther);
				int isDefined = langM_table_containskey(&pscOther->identifierTable, identifier) == 0;
				if (isDefined) {
					if (isLocal) {
						int idxLocal;
						langM_table_get(&pscOther->identifierTable, identifier, &idxLocal);
						emitop(dst, LANGV_OP_SETLOCAL);
						emitinteger(dst, idxLocal);
						pscCurr->stackSize--;
					} else {
						int idxUpval;
						if (langM_table_get(&pscCurr->upvalTable, identifier, &idxUpval) == 1) {
							int depthInner = pcs->scopeContexts.length - 1;
							for (;; depthInner--) {
								LangC_ScopeContext *pscOther;
								langM_list_get(&pcs->scopeContexts, depthInner, &pscOther);
								if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
									break;
								}
							}
							int innermost = 1;
							for (;;) {
								int depthOuter = depthInner - 1;
								for (;; depthOuter--) {
									LangC_ScopeContext *pscOther;
									langM_list_get(&pcs->scopeContexts, depthOuter, &pscOther);
									if (pscOther->type == LANGC_SCOPE_TYPE_FUNCTION) {
										break;
									}
								}
								// Record upval
								LangC_ScopeContext *pscInner;
								langM_list_get(&pcs->scopeContexts, depthInner, &pscInner);
								LangC_ScopeContext *pscOuter;
								langM_list_get(&pcs->scopeContexts, depthOuter, &pscOuter);
								LangC_UpvalDesc ud;
								int idxUpvalOuter;
								int terminate;
								if (langM_table_get(&pscOuter->upvalTable, identifier, &idxUpvalOuter) == 0) {
									// OLD, terminate
									ud.type = LANG_UPVAL_INDIRECT;
									ud.index = idxUpvalOuter;
									terminate = 1;
								} else if (depthOuter <= depth) {
									// NEW, terminate
									LangC_ScopeContext *pscUpval;
									langM_list_get(&pcs->scopeContexts, depth, &pscUpval);
									ud.type = LANG_UPVAL_DIRECT;
									langM_table_get(&pscUpval->identifierTable, identifier, &ud.index);
									terminate = 1;
									pscUpval->hasCapturedVariables = 1;
								} else {
									// OLD
									idxUpvalOuter = pscOuter->upvals.length;
									ud.type = LANG_UPVAL_INDIRECT;
									ud.index = idxUpvalOuter;
									terminate = 0;
								}
								int idxUpvalInner = pscInner->upvals.length;
								langM_table_put(&pscInner->upvalTable, identifier, &idxUpvalInner);
								langM_list_push(&pscInner->upvals, &ud);
								if (innermost) {
									langM_table_get(&pscInner->upvalTable, identifier, &idxUpval);
								}
								if (terminate) {
									break;
								}
								depthInner = depthOuter;
								innermost = 0;
							}
						}
						emitop(dst, LANGV_OP_SETUPVAL);
						emitinteger(dst, idxUpval);
						pscCurr->stackSize--;
					}
					break;
				}
				if (depth == 0) {
					free(identifier);
					langC_errmsg(pcs, "variable is not defined");
					return 1;
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
			emitop(dst, LANGV_OP_SETFIELD);
			emitlstring(dst, src + ptok->value.string.index, ptok->value.string.length);
			pscCurr->stackSize--;
		}
		if (i == 0) {
			break;
		}
		i--;
	}
	return 0;
}

static int compile_control_else(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	LangC_ScopeContext *pscOuter;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
	if (push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL)) {
		return 1;
	}
	if (compile_block(src, pnode->value.controlElse.pblock, pcs, dst)) {
		return 1;
	}

	LangC_ScopeContext *pscInner;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
	if (pscInner->hasCapturedVariables) {
		emitop(dst, LANGV_OP_CLOSEUPVALS);
		emitinteger(dst, pscOuter->stackSize);
	}
	if (pscInner->stackSize != pscOuter->stackSize) {
		emitop(dst, LANGV_OP_POPN);
		emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(pcs);
	return 0;
}

static int compile_control_ifelseif(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	if (compile_expr(src, pnode->value.controlIfElseif.pexpr, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscOuter;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
	emitop(dst, LANGV_OP_JMPZ);
	int pcJmpnzWrite = dst->length;
	emitinteger(dst, 1); // PLACEHOLDER
	pscOuter->stackSize--;
	int pcJmpnzFrom = dst->length;

	if (push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL)) {
		return 1;
	}
	if (compile_block(src, pnode->value.controlIfElseif.pblock, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscInner;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
	if (pscInner->hasCapturedVariables) {
		emitop(dst, LANGV_OP_CLOSEUPVALS);
		emitinteger(dst, pscOuter->stackSize);
	}
	if (pscInner->stackSize != pscOuter->stackSize) {
		emitop(dst, LANGV_OP_POPN);
		emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(pcs);

	LangP_AstNode *pnext = pnode->value.controlIfElseif.pnext;
	if (pnext != NULL) {
		emitop(dst, LANGV_OP_JMP);
		int pcJmpWrite = dst->length;
		emitinteger(dst, 1); // PLACEHOLDER
		int pcJmpFrom = dst->length;
		writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
		if (pnext->type == LANGP_AST_NODE_CONTROL_ELSE) {
			if (compile_control_else(src, pnext, pcs, dst)) {
				return 1;
			}
		} else if (pnext->type == LANGP_AST_NODE_CONTROL_IFELSEIF) {
			if (compile_control_ifelseif(src, pnext, pcs, dst)) {
				return 1;
			}
		}
		writeinteger(dst, pcJmpWrite, dst->length - pcJmpFrom);
	} else {
		writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
	}
	return 0;
}

static int compile_control_while(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	int pcJmpTo = dst->length;
	if (compile_expr(src, pnode->value.controlIfElseif.pexpr, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscOuter;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscOuter);
	emitop(dst, LANGV_OP_JMPZ);
	int pcJmpnzWrite = dst->length;
	emitinteger(dst, 1); // PLACEHOLDER
	pscOuter->stackSize--;
	int pcJmpnzFrom = dst->length;

	if (push_scope_context(pcs, pscOuter->stackSize, LANGC_SCOPE_TYPE_NORMAL)) {
		return 1;
	}
	if (compile_block(src, pnode->value.controlIfElseif.pblock, pcs, dst)) {
		return 1;
	}
	LangC_ScopeContext *pscInner;
	langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &pscInner);
	if (pscInner->hasCapturedVariables) {
		emitop(dst, LANGV_OP_CLOSEUPVALS);
		emitinteger(dst, pscOuter->stackSize);
	}
	if (pscInner->stackSize != pscOuter->stackSize) {
		emitop(dst, LANGV_OP_POPN);
		emitinteger(dst, pscInner->stackSize - pscOuter->stackSize);
	}
	pop_scope_context(pcs);
	emitop(dst, LANGV_OP_JMP);
	int pcJmpWrite = dst->length;
	emitinteger(dst, 0); // PLACEHOLDER
	writeinteger(dst, pcJmpWrite, pcJmpTo - dst->length);

	writeinteger(dst, pcJmpnzWrite, dst->length - pcJmpnzFrom);
	return 0;
}

static int compile_statement(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	switch (pnode->type) {
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
		case LANGP_AST_NODE_CONTROL_IFELSEIF:
		{
			if (compile_control_ifelseif(src, pnode, pcs, dst)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_CONTROL_WHILE:
		{
			if (compile_control_while(src, pnode, pcs, dst)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_DEBUGGER:
		{
			emitop(dst, LANGV_OP_DEBUGGER);
		} break;
		case LANGP_AST_NODE_DECLARATION:
		{
			if (compile_declaration(src, pnode, pcs, dst)) {
				return 1;
			}
		} break;
		case LANGP_AST_NODE_EXPORT:
		{
			int nVar = pnode->value.nodes.length;
			for (int i = 0; i < nVar; i++) {
				LangP_AstNode *pvar;
				langM_list_get(&pnode->value.nodes, i, &pvar);
				if (compile_expr(src, pvar, pcs, dst)) {
					return 1;
				}
				LangP_Token *ptokVar = pvar->value.ptoken;
				int len = ptokVar->value.string.length;
				char *identifier = malloc(len + 1);
				if (!identifier) {
					free(identifier);
					langC_errmsg(pcs, "allocation failed");
					return 1;
				}
				memcpy(identifier, src + ptokVar->value.string.index, len);
				identifier[len] = '\0';
				LangC_ScopeContext *psc;
				langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
				emitop(dst, LANGV_OP_EXPORT);
				emitlstring(dst, len, identifier);
				psc->stackSize--;
			}
		} break;
		case LANGP_AST_NODE_IMPORT:
		{
			int nVar = pnode->value.nodes.length;
			for (int i = 0; i < nVar; i++) {
				LangP_AstNode *pvar;
				langM_list_get(&pnode->value.nodes, i, &pvar);
				LangP_Token *ptokVar = pvar->value.ptoken;
				int len = ptokVar->value.string.length;
				char *identifier = malloc(len + 1);
				if (!identifier) {
					free(identifier);
					langC_errmsg(pcs, "allocation failed");
					return 1;
				}
				memcpy(identifier, src + ptokVar->value.string.index, len);
				identifier[len] = '\0';
				LangC_ScopeContext *psc;
				langM_list_get(&pcs->scopeContexts, pcs->scopeContexts.length - 1, &psc);
				emitop(dst, LANGV_OP_IMPORT);
				emitlstring(dst, len, identifier);
				psc->stackSize++;

				int idxLocal = psc->stackSize - 1;
				langM_table_put(&psc->identifierTable, identifier, &idxLocal);
			}
		} break;
		case LANGP_AST_NODE_RETURN:
		{
			int nReturn = pnode->value.nodes.length;
			LangP_AstNode *pexprFirst;
			langM_list_get(&pnode->value.nodes, 0, &pexprFirst);
			if (nReturn == 1 && pexprFirst->type == LANGP_AST_NODE_CALL) {
				// Tail call
				if (compile_expr(src, pexprFirst->value.call.pexpr, pcs, dst)) {
					return 1;
				}
				LangP_AstNode *parglist = pexprFirst->value.call.pexprlist;
				int nArg = parglist->value.nodes.length;
				for (int i = 0; i < nArg; i++) {
					LangP_AstNode *parg;
					langM_list_get(&parglist->value.nodes, i, &parg);
					if (compile_expr(src, parg, pcs, dst)) {
						return 1;
					}
				}
				emitop(dst, LANGV_OP_TAILCALL);
				emitinteger(dst, nArg);
			} else {
				// Ordinary return
				for (int i = 0; i < nReturn; i++) {
					LangP_AstNode *pexpr;
					langM_list_get(&pnode->value.nodes, i, &pexpr);
					if (compile_expr(src, pexpr, pcs, dst)) {
						return 1;
					}
				}
				emitop(dst, LANGV_OP_RETURN);
				emitinteger(dst, nReturn);
			}
		} break;
		default:
			assert(0);
			return 1;
	}
	return 0;
}

static int compile_block(const char *src, LangP_AstNode *pnode, LangC_CompilerState *pcs, LangM_CharList *dst) {
	if (pnode->type != LANGP_AST_NODE_BLOCK) {
		assert(0);
		return 1;
	}
	for (int i = 0; i < pnode->value.nodes.length; i++) {
		LangP_AstNode *psubnode;
		langM_list_get(&pnode->value.nodes, i, &psubnode);
		if (compile_statement(src, psubnode, pcs, dst))
			return 1;
	}
	return 0;
}

static int init(LangC_CompilerState *pcs) {
	if (langM_list_init(&pcs->scopeContexts, sizeof(LangC_ScopeContext *), 1)) {
		return 1;
	}
	if (push_scope_context(pcs, 0, LANGC_SCOPE_TYPE_FUNCTION)) {
		langM_list_free(&pcs->scopeContexts);
		return 1;
	}
	pcs->msg = NULL;
	return 0;
}

static void free_state(LangC_CompilerState *pcs) {
	for (int i = 0; i < pcs->scopeContexts.length; i++) {
		LangC_ScopeContext *psc;
		langM_list_get(&pcs->scopeContexts, i, &psc);
		free_scope_context(psc);
	}
	langM_list_free(&pcs->scopeContexts);
}

LangChunk langC_compile(const char *src, LangP_AstNode *ast, char **perrmsg) {
	if (!ast || ast->type != LANGP_AST_NODE_BLOCK) {
		assert(0);
	}
	LangC_CompilerState cs;
	if (init(&cs)) {
		*perrmsg = "internal error";
		return (LangChunk) { 0 };
	}
	LangM_CharList buf;
	if (langM_charlist_init(&buf, 10)) {
		*perrmsg = "internal error";
		free_state(&cs);
		return (LangChunk) { 0 };
	}
	if (compile_block(src, ast, &cs, &buf)) {
		*perrmsg = cs.msg;
		free_state(&cs);
		return (LangChunk) { 0 };
	}
	void *ptr = realloc(buf.data, buf.length ? buf.length : 1);
	if (!ptr) {
		*perrmsg = "allocation failed";
		free_state(&cs);
		return (LangChunk) { 0 };
	}
	free_state(&cs);
	return (LangChunk) {
		.ptr = ptr,
		.length = buf.length
	};
}

void langC_free(LangChunk chunk) {
	free(chunk.ptr);
}