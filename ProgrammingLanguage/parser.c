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

inline LangP_AstOperation get_op_binary(LangP_TokenTag tag) {
	if (tag == LANGP_TOK_OPERATOR_ADD) {
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
	} else if (tag == LANGP_TOK_OPERATOR_LE) {
		return LANGP_AST_OP_LE;
	} else if (tag == LANGP_TOK_OPERATOR_GE) {
		return LANGP_AST_OP_GE;
	}
	return LANGP_AST_OP_UNKNOWN;
}

inline LangP_AstOperation get_op_unary(LangP_TokenTag tag) {
	if (tag == LANGP_TOK_OPERATOR_SUB) {
		return LANGP_AST_OP_NEG;
	} else if (tag == LANGP_TOK_OPERATOR_NOT) {
		return LANGP_AST_OP_NOT;
	} else if (tag == LANGP_TOK_OPERATOR_LEN) {
		return LANGP_AST_OP_LEN;
	}
	return LANGP_AST_OP_UNKNOWN;
}

int is_param(LangP_AstNode *pnode) {
	return pnode->type == LANGP_AST_NODE_LEAF && pnode->value.ptoken->type == LANGP_TOK_IDENTIFIER;
}

int is_var(LangP_AstNode *pnode) {
	return is_param(pnode) || (pnode->type == LANGP_AST_NODE_FIELDEXPR);
}

void free_node(LangP_AstNode *pnode) {
	if (!pnode) {
		return;
	}
	switch (pnode->type) {
		case LANGP_AST_NODE_ASSIGNMENT:
			free_node(pnode->value.assignment.pvarlist);
			free_node(pnode->value.assignment.pexprlist);
			break;
		case LANGP_AST_NODE_BINARYEXPR:
			free_node(pnode->value.binaryExpression.pleft);
			free_node(pnode->value.binaryExpression.pright);
			break;
		case LANGP_AST_NODE_BLOCK:
		case LANGP_AST_NODE_EXPRLIST:
		case LANGP_AST_NODE_VARLIST:
			for (int i = 0; i < pnode->value.nodes.length; i++) {
				LangP_AstNode *psubnode;
				list_get(&pnode->value.nodes, i, &psubnode);
				free_node(psubnode);
			}
			break;
		case LANGP_AST_NODE_CALL:
			free_node(pnode->value.call.pexpr);
			free_node(pnode->value.call.pexprlist);
			break;
		case LANGP_AST_NODE_CONTROL_IMPORT:
		case LANGP_AST_NODE_CONTROL_RETURN:
			free_node(pnode->value.pnode);
			break;
		case LANGP_AST_NODE_CONTROL_ELSE:
			free_node(pnode->value.controlElse.pblock);
			break;
		case LANGP_AST_NODE_CONTROL_FUNCTION:
			free_node(pnode->value.controlFunction.pparamlist);
			free_node(pnode->value.controlFunction.pblock);
			break;
		case LANGP_AST_NODE_CONTROL_IFELSEIF:
			free_node(pnode->value.controlIfElseif.pexpr);
			free_node(pnode->value.controlIfElseif.pblock);
			if (pnode->value.controlIfElseif.pnext) {
				free_node(pnode->value.controlIfElseif.pnext);
			}
			break;
		case LANGP_AST_NODE_FIELDEXPR:
			free_node(pnode->value.fieldExpression.pparent);
			free_node(pnode->value.fieldExpression.pchild);
			break;
		case LANGP_AST_NODE_UNARYEXPR:
			free_node(pnode->value.unaryExpression.pinner);
			break;
		case LANGP_AST_NODE_LEAF:
			break;
		default:
			assert(0);
			break;
	}
	free(pnode);
}

LangP_AstNode *parse_primary(LangP_ParserState *pps) {
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type == LANGP_TOK_KEYWORD && ptok->value.tag == LANGP_TOK_KEYWORD_FUNCTION) {
		// function definition
		consume_token(pps);

		ptok = peek_token(pps);
		if (!ptok) {
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARLEFT) {
			langP_errmsg(pps, "expected '('");
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pparamlist = malloc(sizeof(LangP_AstNode));
		if (!pparamlist) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pparamlist->type = LANGP_AST_NODE_VARLIST;
		list_new(&pparamlist->value.nodes, sizeof(LangP_AstNode *), 2);

		ptok = peek_token(pps);
		if (!ptok) {
			free_node(pparamlist);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			for (;;) {
				LangP_AstNode *pexpr = parse_expr(pps);
				if (!pexpr) {
					free_node(pparamlist);
					return NULL;
				}
				if (!is_param(pexpr)) {
					langP_errmsg(pps, "expected param");
					free_node(pparamlist);
					free_node(pexpr);
					return NULL;
				}
				list_push(&pparamlist->value.nodes, &pexpr);
				LangP_Token *ptok = peek_token(pps);
				if (!ptok) {
					free_node(pparamlist);
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
			free_node(pparamlist);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			langP_errmsg(pps, "expected ')'");
			free_node(pparamlist);
			return NULL;
		}
		consume_token(pps);

		ptok = peek_token(pps);
		if (!ptok) {
			free_node(pparamlist);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACELEFT) {
			langP_errmsg(pps, "expected '{'");
			free_node(pparamlist);
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pblock = parse_block(pps);
		if (!pblock) {
			free_node(pparamlist);
			return NULL;
		}
		
		ptok = peek_token(pps);
		if (!ptok) {
			free_node(pparamlist);
			free_node(pblock);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACERIGHT) {
			langP_errmsg(pps, "expected '}'");
			free_node(pparamlist);
			free_node(pblock);
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
		if (!pcontrol) {
			langP_errmsg(pps, "failed to allocate");
			free_node(pparamlist);
			free_node(pblock);
			return NULL;
		}
		pcontrol->type = LANGP_AST_NODE_CONTROL_FUNCTION;
		pcontrol->value.controlFunction.pparamlist = pparamlist;
		pcontrol->value.controlFunction.pblock = pblock;
		return pcontrol;
	}
	if (ptok->type == LANGP_TOK_IDENTIFIER || ptok->type == LANGP_TOK_NUMBER || ptok->type == LANGP_TOK_STRING) {
		// identifier, number, or string
		consume_token(pps);

		LangP_AstNode *pleaf = malloc(sizeof(LangP_AstNode));
		if (!pleaf) {
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		pleaf->type = LANGP_AST_NODE_LEAF;
		pleaf->value.ptoken = ptok;
		return pleaf;
	}
	if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_PARLEFT) {
		// parenthesized expression
		consume_token(pps);

		LangP_AstNode *pexpr = parse_expr(pps);
		if (!pexpr) {
			return NULL;
		}
		ptok = peek_token(pps);
		if (!ptok) {
			free_node(pexpr);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
			langP_errmsg(pps, "expected ')'");
			free_node(pexpr);
			return NULL;
		}
		consume_token(pps);
		return pexpr;
	}

	langP_errmsg(pps, "expected identifier, number, string, or function definition");
	return NULL;
}

LangP_AstNode *parse_postfix(LangP_ParserState *pps) {
	LangP_AstNode *pexpr = parse_primary(pps);
	if (!pexpr) {
		return NULL;
	}
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			free_node(pexpr);
			return NULL;
		}
		if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_DOT) {
			// Field
			consume_token(pps);

			LangP_AstNode *pchild = parse_primary(pps);
			if (!pchild) {
				free_node(pexpr);
				return NULL;
			}
			if (!is_param(pchild)) {
				langP_errmsg(pps, "expected param");
				free_node(pexpr);
				free_node(pchild);
				return NULL;
			}
			LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
			if (!pexprNew) {
				langP_errmsg(pps, "failed to allocate");
				free_node(pexpr);
				free_node(pchild);
				return NULL;
			}
			pexprNew->type = LANGP_AST_NODE_FIELDEXPR;
			pexprNew->value.fieldExpression.pparent = pexpr;
			pexprNew->value.fieldExpression.pchild = pchild;
			pexpr = pexprNew;
			continue;
		}
		if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_PARLEFT) {
			// Call
			consume_token(pps);

			LangP_AstNode *parglist = malloc(sizeof(LangP_AstNode));
			if (!parglist) {
				langP_errmsg(pps, "failed to allocate");
				free_node(pexpr);
				return NULL;
			}
			parglist->type = LANGP_AST_NODE_EXPRLIST;
			list_new(&parglist->value.nodes, sizeof(LangP_AstNode *), 2);
			ptok = peek_token(pps);
			if (!ptok) {
				free_node(pexpr);
				free_node(parglist);
				return NULL;
			}
			if (ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT) {
				consume_token(pps);
			} else {
				for (;;) {
					LangP_AstNode *parg = parse_expr(pps);
					if (!parg) {
						free_node(pexpr);
						free_node(parglist);
						return NULL;
					}
					list_push(&parglist->value.nodes, &parg);
					ptok = peek_token(pps);
					if (!ptok) {
						free_node(pexpr);
						free_node(parglist);
						return NULL;
					}
					if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
						break;
					}
					consume_token(pps);
				}
				ptok = peek_token(pps);
				if (!ptok) {
					free_node(pexpr);
					free_node(parglist);
					return NULL;
				}
				if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
					langP_errmsg(pps, "expected ')'");
					free_node(pexpr);
					free_node(parglist);
					return NULL;
				}
				consume_token(pps);
			}
			LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
			if (!pexprNew) {
				langP_errmsg(pps, "failed to allocate");
				free_node(pexpr);
				free_node(parglist);
				return NULL;
			}
			pexprNew->type = LANGP_AST_NODE_CALL;
			pexprNew->value.call.pexpr = pexpr;
			pexprNew->value.call.pexprlist = parglist;
			pexpr = pexprNew;
			continue;
		}
		break;
	}
	return pexpr;
}

LangP_AstNode *parse_unary(LangP_ParserState *pps) {
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}

	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag == LANGP_TOK_OPERATOR_PARLEFT) {
		return parse_postfix(pps);
	}
	consume_token(pps);

	LangP_AstOperation op = get_op_unary(ptok->value.tag);
	if (op == LANGP_AST_OP_UNKNOWN) {
		langP_errmsg(pps, "unknown operation");
		return NULL;
	}
	LangP_AstNode *pinner = parse_unary(pps);
	if (!pinner) {
		return NULL;
	}
	LangP_AstNode *pexpr = malloc(sizeof(LangP_AstNode));
	if (!pexpr) {
		langP_errmsg(pps, "failed to allocate");
		free_node(pinner);
		return NULL;
	}
	pexpr->type = LANGP_AST_NODE_UNARYEXPR;
	pexpr->value.unaryExpression.pinner = pinner;
	pexpr->value.unaryExpression.op = op;
	return pexpr;
}

LangP_AstNode *parse_expr(LangP_ParserState *pps) {
	LangP_AstNode *pexpr = parse_unary(pps);
	if (!pexpr) {
		return NULL;
	}

	// binary operation chain
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			free_node(pexpr);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag == LANGP_TOK_OPERATOR_COMMA || ptok->value.tag == LANGP_TOK_OPERATOR_PARRIGHT || ptok->value.tag == LANGP_TOK_OPERATOR_CBRACERIGHT || ptok->value.tag == LANGP_TOK_OPERATOR_ASSIGN) {
			break;
		}
		consume_token(pps);
		LangP_AstOperation op = get_op_binary(ptok->value.tag);
		if (op == LANGP_AST_OP_UNKNOWN) {
			langP_errmsg(pps, "unknown operation");
			free_node(pexpr);
			return NULL;
		}
		LangP_AstNode *pright = parse_unary(pps);
		if (!pright) {
			free_node(pexpr);
			return NULL;
		}
		LangP_AstNode *pexprNew = malloc(sizeof(LangP_AstNode));
		if (!pexprNew) {
			langP_errmsg(pps, "failed to allocate");
			free_node(pexpr);
			free_node(pright);
			return NULL;
		}
		pexprNew->type = LANGP_AST_NODE_BINARYEXPR;
		pexprNew->value.binaryExpression.pleft = pexpr;
		pexprNew->value.binaryExpression.pright = pright;
		pexprNew->value.binaryExpression.op = op;
		pexpr = pexprNew;
	}
	return pexpr;
}

LangP_AstNode *parse_control_else(LangP_ParserState *pps) {
	consume_token(pps);

	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACELEFT) {
		langP_errmsg(pps, "expected '{'");
		return NULL;
	}
	consume_token(pps);
	LangP_AstNode *pblock = parse_block(pps);
	if (!pblock) {
		return NULL;
	}
	ptok = peek_token(pps);
	if (!ptok) {
		free_node(pblock);
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACERIGHT) {
		langP_errmsg(pps, "expected '}'");
		free_node(pblock);
		return NULL;
	}
	consume_token(pps);

	LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
	if (!pcontrol) {
		langP_errmsg(pps, "failed to allocate");
		free_node(pblock);
		return NULL;
	}
	pcontrol->type = LANGP_AST_NODE_CONTROL_ELSE;
	pcontrol->value.controlElse.pblock = pblock;
	return pcontrol;
}

LangP_AstNode *parse_control_ifelseif(LangP_ParserState *pps) {
	consume_token(pps); // if/elseif
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARLEFT) {
		langP_errmsg(pps, "expected '('");
		return NULL;
	}
	consume_token(pps);
	LangP_AstNode *pexpr = parse_expr(pps);
	if (!pexpr) {
		return NULL;
	}
	ptok = peek_token(pps);
	if (!ptok) {
		free_node(pexpr);
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_PARRIGHT) {
		langP_errmsg(pps, "expected ')'");
		free_node(pexpr);
		return NULL;
	}
	consume_token(pps);

	ptok = peek_token(pps);
	if (!ptok) {
		free_node(pexpr);
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACELEFT) {
		langP_errmsg(pps, "expected '{'");
		free_node(pexpr);
		return NULL;
	}
	consume_token(pps);
	LangP_AstNode *pblock = parse_block(pps);
	if (!pblock) {
		free_node(pexpr);
		return NULL;
	}
	ptok = peek_token(pps);
	if (!ptok) {
		free_node(pexpr);
		free_node(pblock);
		return NULL;
	}
	if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_CBRACERIGHT) {
		langP_errmsg(pps, "expected '}'");
		free_node(pexpr);
		free_node(pblock);
		return NULL;
	}
	consume_token(pps);

	ptok = peek_token(pps);
	if (!ptok) {
		free_node(pexpr);
		free_node(pblock);
		return NULL;
	}
	LangP_AstNode *pnext;
	if (ptok->type == LANGP_TOK_KEYWORD && ptok->value.tag == LANGP_TOK_KEYWORD_ELSEIF) {
		pnext = parse_control_ifelseif(pps);
		if (!pnext) {
			free_node(pexpr);
			free_node(pblock);
			return NULL;
		}
	} else if (ptok->type == LANGP_TOK_KEYWORD && ptok->value.tag == LANGP_TOK_KEYWORD_ELSE) {
		pnext = parse_control_else(pps);
		if (!pnext) {
			free_node(pexpr);
			free_node(pblock);
			return NULL;
		}
	} else {
		pnext = NULL;
	}

	LangP_AstNode *pcontrol = malloc(sizeof(LangP_AstNode));
	if (!pcontrol) {
		langP_errmsg(pps, "failed to allocate");
		free_node(pexpr);
		free_node(pblock);
		free_node(pnext);
		return NULL;
	}
	pcontrol->type = LANGP_AST_NODE_CONTROL_IFELSEIF;
	pcontrol->value.controlIfElseif.pexpr = pexpr;
	pcontrol->value.controlIfElseif.pblock = pblock;
	pcontrol->value.controlIfElseif.pnext = pnext;
	return pcontrol;
}

int consume_semicolon(LangP_ParserState *pps) {
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return 1;
	}
	if (ptok->type != LANGP_TOK_SEMICOLON) {
		langP_errmsg(pps, "expected ';'");
		return 1;
	}
	consume_token(pps);
	return 0;
}

LangP_AstNode *parse_statement(LangP_ParserState *pps) {
	// Parse keyword-based statement
	LangP_Token *ptok = peek_token(pps);
	if (!ptok) {
		return NULL;
	}
	if (ptok->type == LANGP_TOK_KEYWORD) {
		switch (ptok->value.tag) {
			case LANGP_TOK_KEYWORD_IMPORT:
			{
				consume_token(pps);
				LangP_AstNode *pvarlist = malloc(sizeof(LangP_AstNode));
				if (!pvarlist) {
					langP_errmsg(pps, "failed to allocate");
					return NULL;
				}
				pvarlist->type = LANGP_AST_NODE_VARLIST;
				list_new(&pvarlist->value.nodes, sizeof(LangP_AstNode *), 2);
				for (;;) {
					LangP_AstNode *pexpr = parse_expr(pps);
					if (!pexpr) {
						free_node(pvarlist);
						return NULL;
					}
					if (!is_param(pexpr)) {
						langP_errmsg(pps, "expected param");
						free_node(pexpr);
						free_node(pvarlist);
						return NULL;
					}
					list_push(&pvarlist->value.nodes, &pexpr);
					LangP_Token *ptok = peek_token(pps);
					if (!ptok) {
						free_node(pvarlist);
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
					free_node(pvarlist);
					return NULL;
				}
				pcontrol->type = LANGP_AST_NODE_CONTROL_IMPORT;
				pcontrol->value.pnode = pvarlist;

				if (consume_semicolon(pps)) {
					free_node(pvarlist);
					return NULL;
				}
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
				list_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
				for (;;) {
					LangP_AstNode *pexpr = parse_expr(pps);
					if (!pexpr) {
						free_node(pexprlist);
						return NULL;
					}
					list_push(&pexprlist->value.nodes, &pexpr);
					LangP_Token *ptok = peek_token(pps);
					if (!ptok) {
						free_node(pexprlist);
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
					free_node(pexprlist);
					return NULL;
				}
				pcontrol->type = LANGP_AST_NODE_CONTROL_RETURN;
				pcontrol->value.pnode = pexprlist;

				if (consume_semicolon(pps)) {
					free_node(pcontrol);
					return NULL;
				}
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
		if (consume_semicolon(pps)) {
			free_node(pexpr);
			return NULL;
		}
		return pexpr;
	}
	if (is_var(pexpr)) {
		LangP_AstNode *pvarlist = malloc(sizeof(LangP_AstNode));
		if (!pvarlist) {
			langP_errmsg(pps, "failed to allocate");
			free_node(pexpr);
			return NULL;
		}
		pvarlist->type = LANGP_AST_NODE_VARLIST;
		list_new(&pvarlist->value.nodes, sizeof(LangP_AstNode *), 2);
		list_push(&pvarlist->value.nodes, &pexpr);
		for (;;) {
			LangP_Token *ptok = peek_token(pps);
			if (!ptok) {
				free_node(pvarlist);
				return NULL;
			}
			if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
				break;
			}
			consume_token(pps);
			pexpr = parse_expr(pps);
			if (!pexpr) {
				free_node(pvarlist);
				return NULL;
			}
			if (!is_var(pexpr)) {
				langP_errmsg(pps, "expected var");
				free_node(pexpr);
				free_node(pvarlist);
				return NULL;
			}
			list_push(&pvarlist->value.nodes, &pexpr);
		}

		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			free_node(pvarlist);
			return NULL;
		}
		if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_ASSIGN) {
			langP_errmsg(pps, "expected '='");
			free_node(pvarlist);
			return NULL;
		}
		consume_token(pps);

		LangP_AstNode *pexprlist = malloc(sizeof(LangP_AstNode));
		if (!pexprlist) {
			langP_errmsg(pps, "failed to allocate");
			free_node(pvarlist);
			return NULL;
		}
		pexprlist->type = LANGP_AST_NODE_EXPRLIST;
		list_new(&pexprlist->value.nodes, sizeof(LangP_AstNode *), 2);
		for (;;) {
			pexpr = parse_expr(pps);
			if (!pexpr) {
				free_node(pvarlist);
				free_node(pexprlist);
				return NULL;
			}
			list_push(&pexprlist->value.nodes, &pexpr);
			LangP_Token *ptok = peek_token(pps);
			if (!ptok) {
				free_node(pvarlist);
				free_node(pexprlist);
				return NULL;
			}
			if (ptok->type != LANGP_TOK_OPERATOR || ptok->value.tag != LANGP_TOK_OPERATOR_COMMA) {
				break;
			}
			consume_token(pps);
		}

		LangP_AstNode *passignment = malloc(sizeof(LangP_AstNode));
		if (!passignment) {
			free_node(pvarlist);
			free_node(pexprlist);
			langP_errmsg(pps, "failed to allocate");
			return NULL;
		}
		passignment->type = LANGP_AST_NODE_ASSIGNMENT;
		passignment->value.assignment.pvarlist = pvarlist;
		passignment->value.assignment.pexprlist = pexprlist;

		if (consume_semicolon(pps)) {
			free_node(passignment);
			return NULL;
		}
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
	list_new(&pblock->value.nodes, sizeof(LangP_AstNode *), 2);
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			free_node(pblock);
			return NULL;
		}
		if (ptok->type == LANGP_TOK_EOF) {
			langP_errmsg(pps, "unexpected EOF");
			free_node(pblock);
			return NULL;
		}
		if ((ptok->type == LANGP_TOK_OPERATOR && ptok->value.tag == LANGP_TOK_OPERATOR_CBRACERIGHT) ||
			(ptok->type == LANGP_TOK_KEYWORD && (ptok->value.tag == LANGP_TOK_KEYWORD_ELSE || ptok->value.tag == LANGP_TOK_KEYWORD_ELSEIF))) {
			break;
		}
		LangP_AstNode *pstatement;
		pstatement = parse_statement(pps);
		if (!pstatement) {
			free_node(pblock);
			return NULL;
		}
		list_push(&pblock->value.nodes, &pstatement);
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
	list_new(&pblock->value.nodes, sizeof(LangP_AstNode *), 2);
	for (;;) {
		LangP_Token *ptok = peek_token(pps);
		if (!ptok) {
			free_node(pblock);
			return NULL;
		}
		if (ptok->type == LANGP_TOK_EOF) {
			break;
		}
		LangP_AstNode *pstatement;
		pstatement = parse_statement(pps);
		if (!pstatement) {
			free_node(pblock);
			return NULL;
		}
		list_push(&pblock->value.nodes, &pstatement);
	}
	return pblock;
}

LangP_AstNode *langP_parse(char *src, List *ptokens, LangP_ParserState *pps) {
	pps->src = src;
	pps->ptokens = ptokens;
	pps->pos = 0;
	pps->msg = NULL;
	return parse_program(pps);
}

void langP_free(LangP_AstNode *pnode) {
	free_node(pnode);
}