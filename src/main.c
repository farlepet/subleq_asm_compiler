#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

char *infile = NULL;
char *outfile = NULL;
int verbose = 0;

static FILE *in  = NULL;
static FILE *out = NULL;

void handle_opts(int, char **);
void usage(int);
int handle_op(char *);

typedef int (*func0_t)();
typedef int (*func1_t)(char *);
typedef int (*func2_t)(char *, char *);
typedef int (*func3_t)(char *, char *, char *);

typedef struct {
    char *op_name;
    int n_args;
    func0_t func;
} ops_t;

ops_t ops[];
char *end_vars;

unsigned line = 0;

int main(int argc, char **argv) {
	handle_opts(argc, argv);

	if(!infile || !outfile) usage(1);

	in  = fopen(infile, "r");
	out = fopen(outfile, "w");

	if(!in) {
		printf("Input file %s could not be opened!", infile);
		return 1;
	}
	if(!out) {
		printf("Output file %s could not be opened/created!", outfile);
		fclose(in);
		return 1;
	}
	
	char str[1024];

	while(fgets(str, 1024, in)) {
        line++;
		char *s = str;
        while(*s == ' ' || *s == '\t') s++;
        int ln = strlen(s);
        if(s[ln-1] == '\n') s[ln-1] = 0;
        // Convert entire line to uppercase
		unsigned i = 0;
        int comment = 0, label = 0;
		for(; i < strlen(s); i++)
        {
            if(s[i] == '#') comment = 1;
            else if(s[i] == ':' && comment == 0) label = 1; 
            s[i] = toupper(s[i]);
        }
        if(!label) {
            int r = handle_op(s);
            if(r) {
                return 1;
            }
        } else {
            fprintf(out, "%s\n", s);
        }
    }

    fprintf(out, "\n%s", end_vars);


	fclose(in);
	fclose(out);

	return 0;
}


void handle_opts(int argc, char **argv) {
    int i = 1;
    while(i < argc) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'h':
                    usage(0);
                    break;

                case 'i':
                    if(i == argc - 1) {
                        puts("Missing argument for -i!");
                        exit(1);
                    }
                    infile = argv[++i];
                    break;
                
                case 'o':
                    if(i == argc - 1) {
                        puts("Missing argument for -o!");
                        exit(1);
                    }
                    outfile = argv[++i];
                    break;

                case 'v':
                    verbose = 1;
                    break;

                default:
                    printf("Invalid option: %s\n", argv[i]);
                    usage(1);
                    break;
            }
        } else {
            printf("Unexpected argument: %s\n", argv[i]);
            usage(1);
        }
        i++;
    }
}

void usage(int retval) {
	puts(
    "USAGE: osic-asm [OPTIONS] -i infile -o outfile\n"
	"  OPTIONS:\n"
	"    -h: Show this help message.\n"
	"    -i: Source file to assemble.\n"
	"    -o: Output binary file.\n"
    "    -v: Verbose (include comments indicating operations).\n");
	exit(retval);
}




/****************************
 * Operations               *                                         *
 * NOTE: first operand is   *
 * normally the destination *
 * of data transfers!       *
 ****************************/

int op_nop() {
    fprintf(out, "_aZ, _aZ\n");
    return 0;
}

int op_jmp(char *arg1) {
    fprintf(out, "_aZ, _aZ, %s\n", arg1);
    return 0;
}

int op_add(char *arg1, char *arg2) {
    fprintf(out, "%s, _aZ\n_aZ, %s\n_aZ, _aZ\n", arg2, arg1);
    return 0;
}

int op_mov(char *arg1, char *arg2) {
    fprintf(out, "%s, %s\n%s, _aZ\n_aZ, %s\n_aZ, _aZ\n", arg1, arg1, arg2, arg1);
    return 0;
}

int op_clr(char *arg1) {
    fprintf(out, "%s, %s\n", arg1, arg1);
    return 0;
}

int op_sub(char *arg1, char *arg2) {
    fprintf(out, "%s, %s\n", arg2, arg1);
    return 0;
}

int op_jz(char *arg1, char *arg2) {
    static int n = 0;
    /*
     * _aZ, arg1, arg2 --> Actually only checks if it is <= 0
     *
     * _jz:
     *  MOV _atmpv1, arg1
     *  _aZ, _atmpv1, _jz.ltez
     *  JMP _jz.end
     * _jz.ltez // Less than or equal to zero
     *  INC _atmpv1
     *  _aZ, _atmpv1, _jz.end
     *  JMP arg2
     * _jz.end:
     */
    op_mov("_atmpv1", arg1);
    fprintf(out, "_aZ, _atmpv1, _jz%d.ltez\n_aZ, _aZ, _jz%d.end\n_jz%d.ltez:\n"
    "_aNeg1, _atmpv1\n_aZ, _atmpv1, _jz%d.end\n_aZ, _aZ, %s\n_jz%d.end:\n", n, n, n, n, arg2, n);
    n++;
    return 0;
}

int op_jlz(char *arg1, char *arg2) {
    /*
     * _jlz:
     *  MOV _atmpv1, arg1
     *  INC _atmpv1
     *  _aZ, _atmpv1, arg2
     */
    op_mov("_atmpv1", arg1);
    fprintf(out, "_aNeg1, _atmpv1\n_aZ, _atmpv1, %s\n", arg2);
    return 0;
}

int op_jlez(char *arg1, char *arg2) {
    op_mov("_atmpv1", arg1);
    fprintf(out, "_aZ, _atmpv1, %s\n", arg2);
    return 0;
}

int op_mul(char *arg1, char *arg2) {
    static int n = 0;
    /*
     * _mul:
     *  CLR _atmpv1
     *  MOV _atmpv2, arg2
     * _mov.loop:
     *  JZ _atmpv2, _mul.end
     *  ADD _atmpv1, arg1
     *  DEC _atmpv2
     *  JMP _mul.loop
     * _mul.end:
     *  MOV arg1, _atmpv1
     */
    fprintf(out,
    "_atmpv1, _atmpv1\n" // CLR _atmpv1
    "_atmpv2, _atmpv2\n%s, _aZ\n_aZ, _atmpv2\n_aZ, _aZ\n" // MOV _atmpv2, arg2
    "_mul_%d.loop:\n" // Loop label
    "_aZ, _atmpv2, _mul_%d.end\n" // JZ _atmpv2, end
    "%s, _aZ\n_aZ, _atmpv1\n_aZ, _aZ\n" // ADD _atmpv1, arg1
    "_aOne, _atmpv2\n" // DEC _atmpv2
    "_aZ, _aZ, _mul_%d.loop\n" // JMP loop
    "_mul_%d.end:\n" // End label
    "%s, %s\n_atmpv1, _aZ\n_aZ, %s\n_aZ, _aZ\n" // MOV arg1, _atmpv1
    , arg2, n, n, arg1, n, n, arg1, arg1, arg1);
    n++;
    return 0;
}

int op_div(char *arg1, char *arg2) {
    static int n = 0;
    /*
     * _div:
     *  MOV _atmpv2, arg1
     *  CLR arg1
     *  JZ _atmpv2, _div.end
     * _div.loop:
     *  SUB _atmpv2, arg2
     *  JLZ _atmpv2, _div.end
     *  INC arg1
     *  JLEZ _atmpv2, _div.end
     *  JMP _div.loop
     * _div.end:
     */
    char tmp[16];
    op_mov("_atmpv2", arg1); // MOV _atmpv2, arg1
    op_clr(arg1); // CLR arg2
    sprintf(tmp, "_div%d.end", n);
    op_jz("_atmpv2", tmp); // JZ _atmpv2, _div.end
    fprintf(out, "_div%d.loop:\n%s, _atmpv2\n", n, arg2); // SUB _atmpv2, arg2
    op_jlz("_atmpv2", tmp); // JLZ _atmpv2, _div.end
    fprintf(out, "_aNeg1, %s\n", arg1); // INC arg1
    op_jlez("_atmpv2", tmp); // JLEZ _atmpv2, _div.end
    fprintf(out, "_aZ, _aZ, _div%d.loop\n_div%d.end:\n", n, n); // JMP _div.loop
    n++;
    return 0;
}

ops_t ops[] = {
    { "NOP",  0, op_nop  },
    { "JMP",  1, op_jmp  },
    { "CLR",  1, op_clr  },
    { "ADD",  2, op_add  },
    { "SUB",  2, op_sub  },
    { "JZ" ,  2, op_jz   },
    { "JLZ",  2, op_jlz  },
    { "JLEZ", 2, op_jlez },
    { "MOV",  2, op_mov  },
    { "MUL",  2, op_mul  },
    { "DIV",  2, op_div  }
};

int handle_op(char *str) {
    if(str[0] == '.' || str[0] == '#') {
        fprintf(out, "%s\n", str);
        return 0;
    }
    unsigned i;
    int idx = -1;
    for(i = 0; i < (sizeof(ops) / sizeof(ops[0])); i++) {
        
        if(strstr(str, ops[i].op_name) == str) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        fprintf(out, "%s\n", str);
        return 0;
    } else {
        if(verbose) {
            fprintf(out, "\n#%s\n", str);
        }
        char *args[5];
        unsigned n_args = ops[i].n_args;
        i = 1;
        args[0] = strtok(str, " \t");
        while((args[i] = strtok(NULL, ", \t")) != NULL) {
            if(++i > 4) break;
        }
        if((i - 1) < n_args) {
            printf("ERROR[ln %d]: Too few arguments for operation: %s\n", line, ops[idx].op_name);
            return 1;
        } else {
            if(n_args == 0) {
                ops[idx].func();
            } else if(n_args == 1) {
                ((func1_t)ops[idx].func)(args[1]);
            } else if(n_args == 2) {
                ((func2_t)ops[idx].func)(args[1], args[2]);
            } else if(n_args == 3) {
                ((func3_t)ops[idx].func)(args[1], args[2], args[3]);
            } else {
                printf("ERROR[ln %d]: Operation requires unsupported number of arguments: %s", line, ops[idx].op_name);
                return 1;
            }
        }
    }
    return 0;
}


/*************
 * Variables *
 *************/

char *end_vars = 
"_aZ:\n"
". 0\n"
"_aOne:\n"
". 1\n"
"_aNeg1:\n"
". -1\n"
"_atmpv1:\n"
". 0\n"
"_atmpv2:\n"
". 0\n";
