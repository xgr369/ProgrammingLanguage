#include "vm.h"

int langV_exec(LangState *L, LangChunk chunk, int baseFrame) {
	char *address = chunk.ptr;
	int len = chunk.length;
	L->paddress = &address;
	while (address - chunk.ptr < len) {
		//printf("pc=%ld, op=%ld\n", pc, src[pc]);
		LangV_Operation _op = *address;
		switch (_op) {
			case LANGV_OP_BINARYOP:
			{
				address++;

				char op = *address;
				address++;

				lang_binaryop(L, op);
				break;
			}
			case LANGV_OP_CALL:
			{
				address++;

				int nArg;
				memcpy(&nArg, address, sizeof(int));
				address += sizeof(int);

				int nReturn;
				memcpy(&nReturn, address, sizeof(int));
				address += sizeof(int);

				lang_call(L, nArg, nReturn);
				break;
			}
			case LANGV_OP_CLOSEUPVALS:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_closeupvals(L, idx);
				break;
			}
			case LANGV_OP_DEBUGGER:
			{
				address++;

				L->debug(L);
				break;
			}
			case LANGV_OP_EXPORT:
			{
				address++;

				int len;
				memcpy(&len, address, sizeof(int));
				address += sizeof(int);

				char *name = malloc(len + 1);
				if (!name) {
					lang_errmsg(L, "allocation failed");
					return 1;
				}
				memcpy(name, address, len);
				name[len] = '\0';
				address += len;

				lang_export(L, name);
				free(name);
				break;
			}
			case LANGV_OP_GETFIELD:
			{
				address++;

				int strLen;
				memcpy(&strLen, address, sizeof(int));
				address += sizeof(int);

				const char *name = address;
				address += strLen;

				lang_getfield(L, name, strLen);
				break;
			}
			case LANGV_OP_GETLOCAL:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_getlocal(L, idx);
				break;
			}
			case LANGV_OP_GETUPVAL:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_getupvalue(L, idx);
				break;
			}
			case LANGV_OP_IMPORT:
			{
				address++;

				int len;
				memcpy(&len, address, sizeof(int));
				address += sizeof(int);

				char *name = malloc(len + 1);
				if (!name) {
					lang_errmsg(L, "allocation failed");
					return 1;
				}
				memcpy(name, address, len);
				name[len] = '\0';
				address += len;

				lang_import(L, name);
				free(name);
				break;
			}
			case LANGV_OP_JMP:
			{
				address++;

				int steps;
				memcpy(&steps, address, sizeof(int));
				address += sizeof(int);

				address += steps;
				break;
			}
			case LANGV_OP_JMPZ:
			{
				address++;

				int flag = lang_iszero(L);
				lang_pop(L);
				if (flag) {
					int steps;
					memcpy(&steps, address, sizeof(int));

					address += steps;
				}

				address += sizeof(int);
				break;
			}
			case LANGV_OP_LIST:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_createlist(L, idx);
				break;
			}
			case LANGV_OP_POPN:
			{
				address++;

				int n;
				memcpy(&n, address, sizeof(int));
				address += sizeof(int);

				lang_popn(L, n);
				break;
			}
			case LANGV_OP_PUSHFUNCTION:
			{
				address++;

				LangChunk functionChunk;
				int pos;
				memcpy(&pos, address, sizeof(int));
				functionChunk.ptr = chunk.ptr + pos;
				address += sizeof(int);

				int nParam;
				memcpy(&nParam, address, sizeof(int));
				address += sizeof(int);

				int nUpval;
				memcpy(&nUpval, address, sizeof(int));
				address += sizeof(int);

				lang_pushlfunction(L, functionChunk, nParam, nUpval, address);
				address += nUpval * (sizeof(char) + sizeof(int));
			} break;
			case LANGV_OP_PUSHLSTRING:
			{
				address++;

				int strLen;
				memcpy(&strLen, address, sizeof(int));
				address += sizeof(int);

				char *pos = address;
				address += strLen;

				lang_pushlstring(L, pos, strLen);
				break;
			}
			case LANGV_OP_PUSHNULL:
			{
				address++;

				lang_pushnull(L);
				break;
			}
			case LANGV_OP_PUSHNUMBER:
			{
				address++;

				lang_number val;
				memcpy(&val, address, sizeof(lang_number));
				address += sizeof(lang_number);

				lang_pushnumber(L, val);
				break;
			}
			case LANGV_OP_PUSHTHIS:
			{
				address++;

				lang_pushthis(L);
				break;
			}
			case LANGV_OP_RETURN:
			{
				address++;

				int nReturn;
				memcpy(&nReturn, address, sizeof(int));
				address += sizeof(int);

				lang_return(L, nReturn, baseFrame);
				if (L->msgCode == LANG_EXIT) {
					L->msgCode = LANG_OK;
					goto langV_exec_end;
				}
				break;
			}
			case LANGV_OP_SETFIELD:
			{
				address++;

				int strLen;
				memcpy(&strLen, address, sizeof(int));
				address += sizeof(int);

				const char *name = address;
				address += strLen;

				lang_setfield(L, name, strLen);
				break;
			}
			case LANGV_OP_SETLOCAL:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_setlocal(L, idx);
				break;
			}
			case LANGV_OP_SETUPVAL:
			{
				address++;

				int idx;
				memcpy(&idx, address, sizeof(int));
				address += sizeof(int);

				lang_setupvalue(L, idx);
				break;
			}
			case LANGV_OP_TAILCALL:
			{
				address++;

				int nArg;
				memcpy(&nArg, address, sizeof(int));
				address += sizeof(int);

				lang_tailcall(L, nArg);
				break;
			}
			case LANGV_OP_UNARYOP:
			{
				address++;

				char op = *address;
				address++;

				lang_unaryop(L, op);
				break;
			}
			default:
				assert(0);
				return 1;
		}
		if (L->msgCode != LANG_OK) {
			return 1;
		}
		if (L->gcLowCount >= L->gcLowThreshold) {
			lang_collectgarbage(L);
		}
	}
	langV_exec_end:
	return 0;
}

#define print_literal(l) printf("%s", "" l)
#define print_op(l) printf("%-14s ", "" l)
int langV_print(LangChunk chunk) {
	char *src = chunk.ptr;
	int pc = 0;
	while (pc < chunk.length) {
		char buf[64];
		printf("%3d", pc);
		print_literal(" ");
		switch (src[pc]) {
			case LANGV_OP_BINARYOP:
			{
				print_op("binaryop");
				pc++;

				char op = src[pc];
				printf("%-3d", op);
				pc++;
				break;
			}
			case LANGV_OP_CALL:
			{
				print_op("call");
				pc++;

				int nArg;
				memcpy(&nArg, src + pc, sizeof(int));
				printf("%-3d", nArg);
				print_literal(" ");
				pc += sizeof(int);

				int nReturn;
				memcpy(&nReturn, src + pc, sizeof(int));
				printf("%-3d", nReturn);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_CLOSEUPVALS:
			{
				print_op("closeupvals");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_DEBUGGER:
			{
				print_op("debugger");
				pc++;
				break;
			}
			case LANGV_OP_EXPORT:
			{
				print_op("export");
				pc++;

				int strLen;
				memcpy(&strLen, src + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = src + pc;
				printf("%.*s", strLen, str);
				pc += strLen;
				break;
			}
			case LANGV_OP_GETFIELD:
			{
				print_op("getfield");
				pc++;

				int strLen;
				memcpy(&strLen, src + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = src + pc;
				printf("%.*s", strLen, str);
				pc += strLen;
				break;
			}
			case LANGV_OP_GETLOCAL:
			{
				print_op("getlocal");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_GETUPVAL:
			{
				print_op("getupval");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_IMPORT:
			{
				print_op("import");
				pc++;

				int strLen;
				memcpy(&strLen, src + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = src + pc;
				printf("%.*s", strLen, str);
				pc += strLen;
				break;
			}
			case LANGV_OP_JMP:
			{
				print_op("jmp");
				pc++;

				int steps;
				memcpy(&steps, src + pc, sizeof(int));
				printf("%-3d", steps);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_JMPZ:
			{
				print_op("jmpz");
				pc++;

				int steps;
				memcpy(&steps, src + pc, sizeof(int));
				printf("%-3d", steps);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_LIST:
			{
				print_op("list");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_POPN:
			{
				print_op("popn");
				pc++;

				int n;
				memcpy(&n, src + pc, sizeof(int));
				printf("%-3d", n);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_PUSHFUNCTION:
			{
				print_op("pushfunction");
				pc++;

				int pos;
				memcpy(&pos, src + pc, sizeof(int));
				printf("%-3d", pos);
				print_literal(" ");
				pc += sizeof(int);

				int nParam;
				memcpy(&nParam, src + pc, sizeof(int));
				printf("%-3d", nParam);
				print_literal(" ");
				pc += sizeof(int);

				int nUpval;
				memcpy(&nUpval, src + pc, sizeof(int));
				printf("%-3d", nUpval);
				print_literal(" ");
				pc += sizeof(int);

				for (int i = 0; i < nUpval; i++) {
					char upvalType = src[pc];
					printf("%-3d", upvalType);
					print_literal(" ");
					pc++;

					int idx;
					memcpy(&idx, src + pc, sizeof(int));
					printf("%-3d", idx);
					print_literal(" ");
					pc += sizeof(int);
				}
				break;
			}
			case LANGV_OP_PUSHLSTRING:
			{
				print_op("pushlstring");
				pc++;

				int strLen;
				memcpy(&strLen, src + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = src + pc;
				printf("%.*s", strLen, str);
				pc += strLen;
				break;
			}
			case LANGV_OP_PUSHNULL:
			{
				print_op("pushnull");
				pc++;
				break;
			}
			case LANGV_OP_PUSHNUMBER:
			{
				print_op("pushnumber");
				pc++;

				lang_number val;
				memcpy(&val, src + pc, sizeof(lang_number));
				printf("%.17g", val);
				pc += sizeof(lang_number);
				break;
			}
			case LANGV_OP_PUSHTHIS:
			{
				print_op("pushthis");
				pc++;
				break;
			}
			case LANGV_OP_RETURN:
			{
				print_op("return");
				pc++;

				int nReturn;
				memcpy(&nReturn, src + pc, sizeof(int));
				printf("%-3d", nReturn);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_SETFIELD:
			{
				print_op("setfield");
				pc++;

				int strLen;
				memcpy(&strLen, src + pc, sizeof(int));
				pc += sizeof(int);

				const char *str = src + pc;
				printf("%.*s,", strLen, str);
				pc += strLen;
				break;
			}
			case LANGV_OP_SETLOCAL:
			{
				print_op("setlocal");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_SETUPVAL:
			{
				print_op("setupval");
				pc++;

				int idx;
				memcpy(&idx, src + pc, sizeof(int));
				printf("%-3d", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_TAILCALL:
			{
				print_op("tailcall");
				pc++;

				int nArg;
				memcpy(&nArg, src + pc, sizeof(int));
				printf("%-3d", nArg);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_UNARYOP:
			{
				print_op("unaryop");
				pc++;

				char op = src[pc];
				printf("%-3d", op);
				pc++;
				break;
			}
			default:
				print_op("[unrecognized]");
				return 1;
		}
		print_literal("\n");
	}
	return 0;
}