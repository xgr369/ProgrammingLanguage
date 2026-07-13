#include "parser.h"

#define langP_errmsg(pps, _msg) (pps->msg = _msg)

LangP_AstNode *parse_block(LangP_ParserState *pps);
LangP_AstNode *parse_expr(LangP_ParserState *pps);

inline LangP_Token *peek_token(LangP_ParserState *pps) {
	if (pps->pos >= pps->ptokens->length) {
		langP_errmsg(pps, "exceeded EOF");
		return NULL;
	}
	return pps->ptokens->data + pps->pos * pps->ptokens->elemSize;
}

inline void consume_token(LangP_ParserState *pps) {
	pps->pos++;
}

int compare_strings(const char *a, const char *b) {
	int i = 0;
	for (;;) {
		if (!a[i]) {
			return 1;
		}
		if (!b[i]) {
			return 0;
		}
		if (a[i] != b[i]) {
			return 0;
		}
		i++;
	}
}

inline LangP_AstOperation get_op(LangP_TokenTag tag) {
	if (tag == LANGP_TOK_OPERATOR_ASSIGN) {
		return LANGP_AST_OP_ASSIGN;
	} else if (tag == LANGP_TOK_OPERATOR_ADD) {
		return LANGP_AST_OP_ADD;
	} else if (tag == LANGP_TOK_OPERATOR_SUB) {
		return LANGP_AST_OP_SUB;
	} else if (tag == LANGP_TOK_OPERATOR_MUL) {
		return LANGP_AST_OP_MUL;
	} else if (tag == LANGP_TOK_OPERATOR_DIV) {
		return LANGP_AST_OP_DIV;
	} else if (tag == LANGP_TOK_OPERATOR_EQ) {
		return LANGP_AST_OP_EQ;
	} else if (tag == LANGP_TOK_OPERATOR_LT) {
		return LANGP_AST_OP_LT;
	} else if (tag == LANGP_TOK_OPERATOR_GT) {
		return LANGP_AST_OP_GT;
	}
	return LANGP_AST_OP_UNKNOWN;
}

LangP_AstNode *parse_primary(LangP_ParserState *pps) {
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	consume_token(pps);
	if (ptok->type == LANGP_TOK_KEYWORD && ptok->value.tag == LANGP_TOK_KEYWORD_FUNCTION) {
		// function definition
		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARLEFT) {
			langP_errmsg(pps, "expected '('");
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pvarlist = malloc(sizeof(LangP_AstNode));
		if (!pvarlist) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pvarlist->type = LANGP_AST_NODE_VARLIST;
		vector_new(&pvarlist->value.nodes, sizeof(LangP_AstNode *), 2);

		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			for (;;) {
				LangP_AstNode *pexpr = parse_expr(pps);
				if (!pexpr) {
					return NULL;
				}
				if (pexpr->type != LANGP_AST_NODE_LEAF) {
					langP_errmsg(pps, "expected var");
					return NULL;
				}
				vector_push(&pvarlist->value.nodes, &pexpr);
				LangP_Token *ptok = peek_token(pps);
				if (!ptok) {
					return NULL;
				}
				if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
					break;
				}
				consume_token(pps);
			}
		}
		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			langP_errmsg(pps, "expected ')'");
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pblock = parse_block(pps);
		if (!pblock) {
			return NULL;
		}
		
		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_KEYWORD || ptok->value.tag != LANGP_TOK_KEYWORD_END) {
			langP_errmsg(pps, "expected 'end'");
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
		if (!pcontrol) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pcontrol->type = LANGP_AST_NODE_CONTROL_FUNCTION;
		pcontrol->value.controlFunction.pvarlist = pvarlist;
		pcontrol->value.controlFunction.pblock = pblock;
		return pcontrol;
	}
	if (ptok->type == LANGP_TOK_IDENTIFIER || ptok->type == LANGP_TOK_NUMBER) {
		// identifier or number
		LangP_AstNode *pleaf = malloc(sizeof(LangP_AstNode));
		if (!pleaf) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pleaf->type = LANGP_AST_NODE_LEAF;
		pleaf->value.ptoken = ptok;
		return pleaf;
	}

	langP_errmsg(pps, "expected identifier, number, or function definition");
	return NULL;
}

/*LangP_AstNode *parse_expr(LangP_ParserState *pps) {
	LangP_AstNode *pexpr = parse_primary(pps);
	if (!pexpr) {
		return NULL;
	}
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag == LANGP_TOK_OPERATOR_COMMA || ptok->value.tag == LANGP_TOK_OPERATOR_PARLEFT || ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT || ptok->value.tag == LANGP_TOK_OPERATOR_ASSIGN) {
			break;
		}
		consume_token(pps);
		LangP_AstNode *pright = parse_expr(pps);
		if (!pright) {
			return NULL;
		}
		LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
		if (!pexprNew) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pexprNew->type = LANGP_AST_NODE_BINARYEXPR;
		pexprNew->value.binaryExpression.pleft = pexpr;
		pexprNew->value.binaryExpression.pright = pright;
		LangP_AstOperation op = get_op(ptok->value.tag);
		if (op == LANGP_AST_OP_UNKNOWN) {
			langP_errmsg(pps, "unknown operation");
			return NULL;
		}
		pexprNew->value.binaryExpression.op = op;
		pexpr = pexprNew;
	}

	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARLEFT) {
		return pexpr;
	}
	consume_token(pps);

	LangP_AstNode *pexprlist = malloc(sizeof(LangP_AstNode));
	if (!pexprlist) {
		langP_errmsg(pps, "failed to allocate");
		return NULL;
	}
	pexprlist->type = LANGP_AST_NODE_EXPRLIST;
	vector_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
	ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT) {
		consume_token(pps);
	} else {
		for (;;) {
			LangP_AstNode *pexpr = parse_expr(pps);
			if (!pexpr) {
				return NULL;
			}
			vector_push(&pexprlist->value.nodes, &pexpr);
			ptok = peek_token(pps);
			if (!ptok) {
				return NULL;
			}
			if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
				break;
			}
			consume_token(pps);
		}
		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			langP_errmsg(pps, "expected ')'");
			return NULL;
		}
		consume_token(pps);
	}
	LangP_AstNode *pcall = malloc(sizeof(LangP_AstNode));
	if (!pcall) {
		langP_errmsg(pps, "failed to allocate");
		return NULL;
	}
	pcall->type = LANGP_AST_NODE_CALL;
	pcall->value.call.pexpr = pexpr;
	pcall->value.call.pexprlist = pexprlist;
	return pcall;
}*/

LangP_AstNode *parse_expr(LangP_ParserState *pps) {
	LangP_AstNode *pexpr = parse_primary(pps);
	if (!pexpr) {
		return NULL;
	}
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag == LANGP_TOK_OPERATOR_COMMA || ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT || ptok->value.tag == LANGP_TOK_OPERATOR_ASSIGN) {
			break;
		}
		consume_token(pps);
		if (ptok->value.tag == LANGP_TOK_OPERATOR_PARLEFT) {
			// call
			LangP_AstNode *pexprlist = malloc(sizeof(LangP_AstNode));
			if (!pexprlist) {
				langP_errmsg(pps, "failed to allocate");
				return NULL;
			}
			pexprlist->type = LANGP_AST_NODE_EXPRLIST;
			vector_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
			ptok = peek_token(pps);
			if (!ptok) {
				return NULL;
			}
			if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT) {
				consume_token(pps);
			} else {
				for (;;) {
					LangP_AstNode *pexpr = parse_expr(pps);
					if (!pexpr) {
						return NULL;
					}
					vector_push(&pexprlist->value.nodes, &pexpr);
					ptok = peek_token(pps);
					if (!ptok) {
						return NULL;
					}
					if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
						break;
					}
					consume_token(pps);
				}
				ptok = peek_token(pps);
				if (!ptok) {
					return NULL;
				}
				if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
					langP_errmsg(pps, "expected ')'");
					return NULL;
				}
				consume_token(pps);
			}
			LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
			if (!pexprNew) {
				langP_errmsg(pps, "failed to allocate");
				return NULL;
			}
			pexprNew->type = LANGP_AST_NODE_CALL;
			pexprNew->value.call.pexpr = pexpr;
			pexprNew->value.call.pexprlist = pexprlist;
			pexpr = pexprNew;
		} else {
			// binary expression
			LangP_AstNode *pright = parse_expr(pps);
			if (!pright) {
				return NULL;
			}
			LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
			if (!pexprNew) {
				langP_errmsg(pps, "failed to allocate");
				return NULL;
			}
			pexprNew->type = LANGP_AST_NODE_BINARYEXPR;
			pexprNew->value.binaryExpression.pleft = pexpr;
			pexprNew->value.binaryExpression.pright = pright;
			LangP_AstOperation op = get_op(ptok->value.tag);
			if (op == LANGP_AST_OP_UNKNOWN) {
				langP_errmsg(pps, "unknown operation");
				return NULL;
			}
			pexprNew->value.binaryExpression.op = op;
			pexpr = pexprNew;
		}
	}
	return pexpr;
}

LangP_AstNode *parse_control_ifelseif(LangP_ParserState *pps);

LangP_AstNode *parse_control_ifelseif(LangP_ParserState *pps) {
	LangP_Token *ptok = peek_token(pps);
	if (ptok->type != LANGP_TOK_KEYWORD || (ptok->value.tag != LANGP_TOK_KEYWORD_IF && ptok->value.tag != LANGP_TOK_KEYWORD_ELSEIF)) {
		langP_errmsg(pps, "expected 'if' or 'elseif'");
		return NULL;
	}
	consume_token(pps);
	LangP_AstNode *pexpr = parse_expr(pps);
	if (!pexpr) {
		return NULL;
	}
	ptok = peek_token(pps);
	if (ptok->type != LANGP_TOK_KEYWORD || ptok->value.tag != LANGP_TOK_KEYWORD_THEN) {
		langP_errmsg(pps, "expected 'then'");
		return NULL;
	}
	consume_token(pps);

	LangP_AstNode *pblock = parse_block(pps);
	if (!pblock) {
		return NULL;
	}

	ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type != LANGP_TOK_KEYWORD) {
		langP_errmsg(pps, "expected 'else', 'elseif', or 'end'");
		return NULL;
	}
	LangP_AstNode *pnext;
	if (ptok->value.tag == LANGP_TOK_KEYWORD_ELSE) {
		pnext = NULL;
		//TODOpnext = parse_control_else(pps);
	} else if (ptok->value.tag == LANGP_TOK_KEYWORD_ELSEIF) {
		pnext = parse_control_ifelseif(pps);
		if (!pnext) {
			return NULL;
		}
	} else if (ptok->value.tag == LANGP_TOK_KEYWORD_END) {
		consume_token(pps);
		pnext = NULL;
	} else {
		langP_errmsg(pps, "expected 'else', 'elseif', or 'end'");
		return NULL;
	}

	LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
	if (!pcontrol) {
		langP_errmsg(pps, "failed to allocate");
		return NULL;
	}
	pcontrol->type = LANGP_AST_NODE_CONTROL_IFELSEIF;
	pcontrol->value.controlIfElseif.pexpr = pexpr;
	pcontrol->value.controlIfElseif.pblock = pblock;
	pcontrol->value.controlIfElseif.pnext = pnext;
	return pcontrol;
}

LangP_AstNode *parse_statement(LangP_ParserState *pps) {
	// Parse keyword-based statement
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type == LANGP_TOK_KEYWORD) {
		switch (ptok->value.tag) {
			case LANGP_TOK_KEYWORD_EXTERN:
			{
				consume_token(pps);
				LangP_AstNode *pvarlist = malloc(sizeof(LangP_AstNode));
				if (!pvarlist) {
					langP_errmsg(pps, "failed to allocate");
					return NULL;
				}
				pvarlist->type = LANGP_AST_NODE_VARLIST;
				vector_new(&pvarlist->value.nodes, sizeof(LangP_AstNode *), 2);
				for (;;) {
					LangP_AstNode *pexpr = parse_expr(pps);
					if (!pexpr) {
						return NULL;
					}
					if (pexpr->type != LANGP_AST_NODE_LEAF) {
						langP_errmsg(pps, "expected var");
						return NULL;
					}
					vector_push(&pvarlist->value.nodes, &pexpr);
					LangP_Token *ptok = peek_token(pps);
					if (!ptok) {
						return NULL;
					}
					if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
						break;
					}
					consume_token(pps);
				}

				LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
				if (!pcontrol) {
					langP_errmsg(pps, "failed to allocate");
					return NULL;
				}
				pcontrol->type = LANGP_AST_NODE_CONTROL_EXTERN;
				pcontrol->value.pnode = pvarlist;
				return pcontrol;
			}
			case LANGP_TOK_KEYWORD_IF:
			{
				LangP_AstNode *pcontrol = parse_control_ifelseif(pps);
				if (!pcontrol) {
					return NULL;
				}
				return pcontrol;
			}
			case LANGP_TOK_KEYWORD_RETURN:
			{
				consume_token(pps);
				LangP_AstNode *pexprlist = malloc(sizeof(LangP_AstNode));
				if (!pexprlist) {
					langP_errmsg(pps, "failed to allocate");
					return NULL;
				}
				pexprlist->type = LANGP_AST_NODE_EXPRLIST;
				vector_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
				for (;;) {
					LangP_AstNode *pexpr = parse_expr(pps);
					if (!pexpr) {
						return NULL;
					}
					vector_push(&pexprlist->value.nodes, &pexpr);
					LangP_Token *ptok = peek_token(pps);
					if (!ptok) {
						return NULL;
					}
					if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
						break;
					}
					consume_token(pps);
				}

				LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
				if (!pcontrol) {
					langP_errmsg(pps, "failed to allocate");
					return NULL;
				}
				pcontrol->type = LANGP_AST_NODE_CONTROL_RETURN;
				pcontrol->value.pnode = pexprlist;
				return pcontrol;
			}
		}
	}

	// Parse assignment or call
	LangP_AstNode *pexpr = parse_expr(pps);
	if (!pexpr) {
		return NULL;
	}
	if (pexpr->type == LANGP_AST_NODE_CALL) {
		return pexpr;
	}
	if (pexpr->type == LANGP_AST_NODE_LEAF) {
		LangP_AstNode *pvarlist = malloc(sizeof(LangP_AstNode));
		if (!pvarlist) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pvarlist->type = LANGP_AST_NODE_VARLIST;
		vector_new(&pvarlist->value.nodes, sizeof(LangP_AstNode *), 2);
		vector_push(&pvarlist->value.nodes, &pexpr);
		for (;;) {
			LangP_Token *ptok = peek_token(pps);
			if (!ptok) {
				return NULL;
			}
			if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
				break;
			}
			consume_token(pps);
			pexpr = parse_expr(pps);
			if (!pexpr) {
				return NULL;
			}
			if (pexpr->type != LANGP_AST_NODE_LEAF) {
				langP_errmsg(pps, "expected var");
				return NULL;
			}
			vector_push(&pvarlist->value.nodes, &pexpr);
		}

		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_ASSIGN) {
			langP_errmsg(pps, "expected '='");
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pexprlist = malloc(sizeof(LangP_AstNode));
		if (!pexprlist) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pexprlist->type = LANGP_AST_NODE_EXPRLIST;
		vector_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
		for (;;) {
			pexpr = parse_expr(pps);
			if (!pexpr) {
				return NULL;
			}
			vector_push(&pexprlist->value.nodes, &pexpr);
			LangP_Token *ptok = peek_token(pps);
			if (!ptok) {
				return NULL;
			}
			if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
				break;
			}
			consume_token(pps);
		}

		LangP_AstNode *passignment = malloc(sizeof(LangP_AstNode));
		if (!passignment) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		passignment->type = LANGP_AST_NODE_BINARYEXPR;
		passignment->value.binaryExpression.op = LANGP_AST_OP_ASSIGN;
		passignment->value.binaryExpression.pleft = pvarlist;
		passignment->value.binaryExpression.pright = pexprlist;

		return passignment;
	}

	langP_errmsg(pps, "invalid statement");
	return NULL;
}

LangP_AstNode *parse_block(LangP_ParserState *pps) {
	LangP_AstNode *pblock = malloc(sizeof(LangP_AstNode));
	if (!pblock) {
		langP_errmsg(pps, "failed to allocate");
		return NULL;
	}
	pblock->type = LANGP_AST_NODE_BLOCK;
	vector_new(&pblock->value.nodes, sizeof(LangP_AstNode *), 2);
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type == LANGP_TOK_EOF) {
			langP_errmsg(pps, "unexpected EOF");
			return NULL;
		}
		if (ptok->type == LANGP_TOK_KEYWORD && (ptok->value.tag == LANGP_TOK_KEYWORD_END || ptok->value.tag == LANGP_TOK_KEYWORD_ELSE || ptok->value.tag == LANGP_TOK_KEYWORD_ELSEIF)) {
			break;
		}
		LangP_AstNode *pstatement;
		pstatement = parse_statement(pps);
		if (!pstatement) {
			return NULL;
		}
		vector_push(&pblock->value.nodes, &pstatement);
	}
	return pblock;
}

LangP_AstNode *parse_program(LangP_ParserState *pps) {
	LangP_AstNode *pblock = malloc(sizeof(LangP_AstNode));
	if (!pblock) {
		langP_errmsg(pps, "failed to allocate");
		return NULL;
	}
	pblock->type = LANGP_AST_NODE_BLOCK;
	vector_new(&pblock->value.nodes, sizeof(LangP_AstNode *), 2);
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type == LANGP_TOK_EOF) {
			break;
		}
		LangP_AstNode *pstatement;
		pstatement = parse_statement(pps);
		if (!pstatement) {
			return NULL;
		}
		vector_push(&pblock->value.nodes, &pstatement);
	}
	return pblock;
}

LangP_AstNode *langP_parse(char *src, Vector *ptokens, LangP_ParserState *pps) {
	pps->src = src;
	pps->ptokens = ptokens;
	pps->pos = 0;
	pps->msg = NULL;
	return parse_program(pps);
}