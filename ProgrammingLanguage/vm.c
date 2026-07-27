#include "vm.h"

int langV_exec(LangState *ps, const char *pbc, int len) {
	size_t pc = 0;
	while (pc < len) {
		//printf("pc=%ld, op=%ld\n", pc, pbc[pc]);
		LangV_Operation _op = pbc[pc];
		switch (_op) {
			case LANGV_OP_BINARYOP:
			{
				pc++;

				char op = pbc[pc];
				pc++;

				lang_binaryop(ps, op);
				break;
			}
			case LANGV_OP_CALL:
			{
				pc++;

				int nArg;
				memcpy(&nArg, pbc + pc, sizeof(int));
				pc += sizeof(int);

				int nReturn;
				memcpy(&nReturn, pbc + pc, sizeof(int));
				pc += sizeof(int);

				ps->callInfo.pc = pc;
				lang_call(ps, nArg, nReturn);
				pc = ps->callInfo.pc;
				break;
			}
			case LANGV_OP_COPY:
			{
				pc++;

				int idxFrom;
				memcpy(&idxFrom, pbc + pc, sizeof(int));
				pc += sizeof(int);

				int idxTo;
				memcpy(&idxTo, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_copy(ps, idxFrom, idxTo);
				break;
			}
			case LANGV_OP_END:
			{
				lang_return(ps, 0);
				pc = ps->callInfo.pc;
				break;
			}
			case LANGV_OP_GETLOCAL:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_getlocal(ps, idx);
				break;
			}
			case LANGV_OP_GETUPVALUE:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_getupvalue(ps, idx);
				break;
			}
			case LANGV_OP_JMP:
			{
				pc++;

				int steps;
				memcpy(&steps, pbc + pc, sizeof(int));
				pc += sizeof(int);

				pc += steps;
				break;
			}
			case LANGV_OP_JMPZ:
			{
				pc++;

				int flag = lang_iszero(ps);
				lang_pop(ps);
				if (flag) {
					int steps;
					memcpy(&steps, pbc + pc, sizeof(int));

					pc += steps;
				}

				pc += sizeof(int);
				break;
			}
			case LANGV_OP_IMPORT:
			{
				pc++;

				int len;
				memcpy(&len, pbc + pc, sizeof(int));
				pc += sizeof(int);

				char *name = malloc(len + 1);
				if (!name) {
					lang_errmsg(ps, "allocation failed");
					return 1;
				}
				memcpy(name, pbc + pc, len);
				name[len] = '\0';
				pc += len;

				lang_import(ps, name);
				free(name);
				break;
			}
			case LANGV_OP_POPN:
			{
				pc++;

				int n;
				memcpy(&n, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_popn(ps, n);
				break;
			}
			case LANGV_OP_PUSHLCLOSURE:
			{
				pc++;

				size_t src;
				memcpy(&src, pbc + pc, sizeof(size_t));
				pc += sizeof(size_t);

				int nUpval;
				memcpy(&nUpval, pbc + pc, sizeof(int));
				pc += sizeof(int);

				void *upvals = pbc + pc;
				pc += nUpval * (sizeof(char) + sizeof(int));

				lang_pushlclosure(ps, src, nUpval, upvals);
			} break;
			case LANGV_OP_PUSHLFUNC:
			{
				pc++;

				size_t src;
				memcpy(&src, pbc + pc, sizeof(size_t));
				pc += sizeof(size_t);

				lang_pushlfunc(ps, src);
			} break;
			case LANGV_OP_PUSHLSTRING:
			{
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				pc += sizeof(int);

				char *src = pbc + pc;
				pc += strLen;

				lang_pushlstring(ps, src, strLen);
				break;
			}
			case LANGV_OP_PUSHNIL:
			{
				pc++;

				lang_pushnil(ps);
				break;
			}
			case LANGV_OP_PUSHNUMBER:
			{
				pc++;

				lang_number val;
				memcpy(&val, pbc + pc, sizeof(lang_number));
				pc += sizeof(lang_number);

				lang_pushnumber(ps, val);
				break;
			}
			case LANGV_OP_RETURN:
			{
				pc++;

				int nReturn;
				memcpy(&nReturn, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_return(ps, nReturn);
				pc = ps->callInfo.pc;
				break;
			}
			case LANGV_OP_SETLOCAL:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_setlocal(ps, idx);
				break;
			}
			case LANGV_OP_SETUPVALUE:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_setupvalue(ps, idx);
				break;
			}
			case LANGV_OP_TAILCALL:
			{
				pc++;

				int nArg;
				memcpy(&nArg, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_tailcall(ps, nArg);
				pc = ps->callInfo.pc;
				break;
			}
			case LANGV_OP_UNARYOP:
			{
				pc++;

				char op = pbc[pc];
				pc++;

				lang_unaryop(ps, op);
				break;
			}
			default:
				lang_errmsg(ps, "unrecognized bytecode operation");
		}
		if (ps->msg != NULL) {
			ps->callInfo.pc = pc;
			return 1;
		}
	}
	return 0;
}

#define print_literal(l) ( callback("" l, sizeof(l)) )
#define print_string(str, len) do {\
	callback("\"", 1);\
	callback((str), (len));\
	callback("\"", 1);\
} while (0);
#define print_int(i, bits) do {\
	char buf[(bits)];\
	long a = (i), b = 0;\
	while (b < (bits)) {\
		buf[(bits) - b - 1] = (a & 1) ? '1' : '0';\
		a = a >> 1;\
		b = b + 1;\
	}\
	callback(buf, (bits));\
} while (0);

int langV_print(LangWriteCallback callback, const char *pbc, int len) {
	size_t pc = 0;
	while (pc < len) {
		char buf[64];
		callback(buf, lang_tostringbufi(pc, buf));
		print_literal(" ");
		switch (pbc[pc]) {
			case LANGV_OP_BINARYOP:
			{
				print_literal("binaryop\t");
				pc++;

				char op = pbc[pc];
				callback(buf, lang_tostringbufi(op, buf));
				pc++;
				break;
			}
			case LANGV_OP_CALL:
			{
				print_literal("call\t");
				pc++;

				int nArg;
				memcpy(&nArg, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(nArg, buf));
				print_literal(" ");
				pc += sizeof(int);

				int nReturn;
				memcpy(&nReturn, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(nReturn, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_COPY:
			{
				print_literal("copy\t");
				pc++;

				int idxFrom;
				memcpy(&idxFrom, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idxFrom, buf));
				print_literal(" ");
				pc += sizeof(int);

				int idxTo;
				memcpy(&idxTo, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idxTo, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_END:
			{
				print_literal("end\t");
				pc++;
				break;
			}
			case LANGV_OP_GETLOCAL:
			{
				print_literal("getlocal\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idx, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_GETUPVALUE:
			{
				print_literal("getupvalue\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idx, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_JMP:
			{
				print_literal("jmp\t");
				pc++;

				int steps;
				memcpy(&steps, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(steps, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_JMPZ:
			{
				print_literal("jmpz\t");
				pc++;

				int steps;
				memcpy(&steps, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(steps, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_IMPORT:
			{
				print_literal("import\t");
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = pbc + pc;
				print_string(str, strLen);
				pc += strLen;
				break;
			}
			case LANGV_OP_POPN:
			{
				print_literal("popn\t");
				pc++;

				int n;
				memcpy(&n, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(n, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_PUSHLCLOSURE:
			{
				print_literal("pushlclosure\t");
				pc++;

				size_t pos;
				memcpy(&pos, pbc + pc, sizeof(size_t));
				callback(buf, lang_tostringbufi(pos, buf));
				print_literal(" ");
				pc += sizeof(size_t);

				int nUpval;
				memcpy(&nUpval, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(nUpval, buf));
				print_literal(" ");
				pc += sizeof(int);

				for (int i = 0; i < nUpval; i++) {
					char upvalType = pbc[pc];
					callback(buf, lang_tostringbufi(upvalType, buf));
					print_literal(" ");
					pc++;

					int idx;
					memcpy(&idx, pbc + pc, sizeof(int));
					callback(buf, lang_tostringbufi(idx, buf));
					print_literal(" ");
					pc += sizeof(int);
				}
				break;
			}
			case LANGV_OP_PUSHLFUNC:
			{
				print_literal("pushlfunc\t");
				pc++;

				size_t pos;
				memcpy(&pos, pbc + pc, sizeof(size_t));
				callback(buf, lang_tostringbufi(pos, buf));
				pc += sizeof(size_t);
				break;
			}
			case LANGV_OP_PUSHLSTRING:
			{
				print_literal("pushlstring\t");
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = pbc + pc;
				print_string(str, strLen);
				pc += strLen;
				break;
			}
			case LANGV_OP_PUSHNIL:
			{
				print_literal("pushnil\t");
				pc++;
				break;
			}
			case LANGV_OP_PUSHNUMBER:
			{
				print_literal("pushnumber\t");
				pc++;

				lang_number val;
				memcpy(&val, pbc + pc, sizeof(lang_number));
				callback(buf, lang_tostringbufd(val, buf));
				pc += sizeof(lang_number);
				break;
			}
			case LANGV_OP_RETURN:
			{
				print_literal("return\t");
				pc++;

				int nReturn;
				memcpy(&nReturn, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(nReturn, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_SETLOCAL:
			{
				print_literal("setlocal\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idx, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_SETUPVALUE:
			{
				print_literal("setupvalue\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(idx, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_TAILCALL:
			{
				print_literal("tailcall\t");
				pc++;

				int nArg;
				memcpy(&nArg, pbc + pc, sizeof(int));
				callback(buf, lang_tostringbufi(nArg, buf));
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_UNARYOP:
			{
				print_literal("unaryop\t");
				pc++;

				char op = pbc[pc];
				callback(buf, lang_tostringbufi(op, buf));
				pc++;
				break;
			}
			default:
				print_literal("[unrecognized op]");
				return 1;
		}
		print_literal("\n");
	}
	return 0;
}