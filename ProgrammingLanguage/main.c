/* 
* main.c
* Entry point for the command-line application.
*/

#if !LANG_BUILD_AS_DLL

#include <stdio.h>
#include "lang.h"

int error(LangState *L) {
    switch (L->msgCode) {
        case LANG_OK:
            printf("Error: no error");
            break;
        case LANG_ERR_COMPILE:
            printf("Compilation Error: %s\n", L->msg);
            break;
        case LANG_ERR_RUN:
            printf("Runtime Error: %s\n", L->msg);
            break;
    }
    return 0;
}

void debug(LangState *L) {
	LangM_List stack = L->stack;
    for (int i = 0; i < stack.length; i++) {
        LangTValue ltv;
        langM_list_get(&stack, i, &ltv);
        printf("%2d. ", i);
        switch (ltv.type) {
            case LANG_TYPE_NULL:
                printf("%-6s  ", "NULL");
                break;
            case LANG_TYPE_NUMBER:
                printf("%-6s  %lf", "NUMBER", ltv.value.number);
                break;
            case LANG_TYPE_STRING:
            {
                LangString *pls = ltv.value.ptr;
                printf("%-6s  %p (\"%.*s\")", "STRING", pls, pls->length, pls->data);
                break;
            }
            case LANG_TYPE_FUNCTION:
            {
                if (ltv.variant == LANG_VARIANT_LFUNC) {
                    printf("%-6s  %p (narg=%d,nupval=%d)", "LFUNC", ltv.value.ptr, ((LangFunction *)ltv.value.ptr)->numArg, ((LangFunction *)ltv.value.ptr)->numUpval);
                } else {
                    printf("%-6s  %p", "CFUNC", ltv.value.ptr);
                }
                break;
            }
            case LANG_TYPE_USERDATA:
            {
                printf("%-6s  %p ", "UDATA", ltv.value.ptr);
                break;
            }
            case LANG_TYPE_RANGE:
            {
                printf("%-6s  %d..%d", "RANGE", ltv.value.range.start, ltv.value.range.end);
                break;
            }
            default:
                printf("INVALID [type=%d]", ltv.type);
        }
        printf("\n");
    }
}

int langB_print(LangState *L) {
    if (lang_argcount(L) != 1) {
        return 0;
    }
    lang_getlocal(L, 0);
    lang_tostring(L);
    LangTValue *pltv = lang_gettvaluelocal(L, 1);
    LangString *pls = pltv->value.ptr;
    printf("%.*s\n", pls->length, pls->data);
    return 0;
}

int langB_input(LangState *L) {
    if (lang_argcount(L) != 0) {
        return 0;
    }
    char buf[1024];
    fgets(buf, 1023, stdin);
    int len = strlen(buf);
    lang_pushlstring(L, buf, len - 1);
    return 1;
}

int langB_tonumber(LangState *L) {
    if (lang_argcount(L) != 1) {
        return 0;
    }
    lang_getlocal(L, 0);
    lang_tonumber(L);
    return 1;
}

int langB_tostring(LangState *L) {
    if (lang_argcount(L) != 1) {
        return 0;
    }
    lang_getlocal(L, 0);
    lang_tostring(L);
    return 1;
}

static const lang_reg base_funcs[] = {
    { "print", langB_print },
    { "input", langB_input },
    { "tonumber", langB_tonumber },
    { "tostring", langB_tostring },
};

int run_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return 1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    long sz = ftell(file);
    if (sz == -1L) {
        perror("ftell");
        fclose(file);
        return 1;
    }
    rewind(file);
    char *src = malloc((size_t)sz + 1);
    if (src == NULL) {
        fclose(file);
        return 1;
    }
    size_t n = fread(src, 1, (size_t)sz, file);
    if (ferror(file)) {
        perror("fread");
        free(src);
        fclose(file);
        return 1;
    }
    src[n] = '\0';
    fclose(file);

    LangState *L = lang_newstate();
    if (!L) {
        free(src);
        fclose(file);
        return 1;
    }
    lang_atdebug(L, debug);
    lang_aterror(L, error);

    for (size_t i = 0; i < sizeof(base_funcs) / sizeof(*base_funcs); i++) {
        lang_registerfunc(L, base_funcs[i].name, base_funcs[i].func);
    }

    lang_load(L, src);
    lang_close(L);
    free(src);
    return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s <source_file>\n", argv[0]);
		return 1;
	}
	const char *path = argv[1];
	run_file(path);
	return 0;
}

#endif // !LANG_BUILD_AS_DLL