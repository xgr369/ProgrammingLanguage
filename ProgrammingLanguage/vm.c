#include "vm.h"

int langV_exec(LangState *ps, const char *pbc, int len) {
	size_t pc = 0;
	while (pc < len) {
		//printf("pc=%ld, op=%ld\n", pc, pbc[pc]);
		switch (pbc[pc]) {
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

				lang_call(ps, nArg, nReturn);
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
			case LANGV_OP_JMPNZ:
			{
				pc++;

				int flag = lang_isnonzero(ps);
				lang_pop(ps);
				if (flag) {
					int steps;
					memcpy(&steps, pbc + pc, sizeof(int));

					pc += steps;
				}

				pc += sizeof(int);
				break;
			}
			case LANGV_OP_LOADEXTERNVALUE:
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

				lang_loadexternvalue(ps, name);
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
			case LANGV_OP_PUSHLSTRING:
			{
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				pc += sizeof(int);

				const char *pstr = pbc + pc;
				pc += strLen;

				lang_pushlstring(ps, pstr, strLen);
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
			case LANGV_OP_PUSHVALUE:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_pushvalue(ps, idx);
				break;
			}
			case LANGV_OP_REPLACE:
			{
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				pc += sizeof(int);

				lang_replace(ps, idx);
				break;
			}
			default:
				lang_errmsg(ps, "unrecognized bytecode operation");
		}
		if (ps->msg != NULL) {
			return 1;
		}
	}
	return 0;
}

int langV_dbg(const char *pbc, int len) {
	size_t pc = 0;
	while (pc < len) {
		printf("%d\t", pc);
		switch (pbc[pc]) {
			case LANGV_OP_BINARYOP:
			{
				printf("binaryop\t");
				pc++;

				char op = pbc[pc];
				printf("char %d ", op);
				pc++;
				break;
			}
			case LANGV_OP_CALL:
			{
				printf("call\t");
				pc++;

				int argCount;
				memcpy(&argCount, pbc + pc, sizeof(int));
				printf("int %d ", argCount);
				pc += sizeof(int);

				int returnCount;
				memcpy(&returnCount, pbc + pc, sizeof(int));
				printf("int %d ", returnCount);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_COPY:
			{
				printf("copy\t");
				pc++;

				int idxFrom;
				memcpy(&idxFrom, pbc + pc, sizeof(int));
				printf("int %d ", idxFrom);
				pc += sizeof(int);

				int idxTo;
				memcpy(&idxTo, pbc + pc, sizeof(int));
				printf("int %d ", idxTo);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_JMPNZ:
			{
				printf("jmpnz\t");
				pc++;

				int steps;
				memcpy(&steps, pbc + pc, sizeof(int));
				printf("int %d ", steps);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_LOADEXTERNVALUE:
			{
				printf("loadexternvalue\t");
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				printf("int %d ", strLen);
				pc += sizeof(int);

				const char *pstr = pbc + pc;
				printf("string %.*s ", strLen, pstr);
				pc += strLen;
				break;
			}
			case LANGV_OP_POPN:
			{
				printf("popn\t");
				pc++;

				int n;
				memcpy(&n, pbc + pc, sizeof(int));
				printf("int %d ", n);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_PUSHLSTRING:
			{
				printf("pushlstring\t");
				pc++;

				int strLen;
				memcpy(&strLen, pbc + pc, sizeof(int));
				printf("int %d ", strLen);
				pc += sizeof(int);

				const char *pstr = pbc + pc;
				printf("string %.*s ", strLen, pstr);
				pc += strLen;
				break;
			}
			case LANGV_OP_PUSHNIL:
			{
				printf("pushnil\t");
				pc++;
				break;
			}
			case LANGV_OP_PUSHNUMBER:
			{
				printf("pushnumber\t");
				pc++;

				lang_number val;
				memcpy(&val, pbc + pc, sizeof(lang_number));
				printf("double %f ", val);
				pc += sizeof(lang_number);
				break;
			}
			case LANGV_OP_PUSHVALUE:
			{
				printf("pushvalue\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				printf("int %d ", idx);
				pc += sizeof(int);
				break;
			}
			case LANGV_OP_REPLACE:
			{
				printf("replace\t");
				pc++;

				int idx;
				memcpy(&idx, pbc + pc, sizeof(int));
				printf("int %d ", idx);
				pc += sizeof(int);
				break;
			}
			default:
				printf("RuntimeError: Unrecognized bytecode op pc=%ld, op=%ld\n", pc, pbc[pc]);
				return 1;
		}
		printf("\n");
	}
	return 0;
}