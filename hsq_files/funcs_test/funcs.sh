#!/bin/sh
cat main.hsq funcs.hsq > _funcs.hsq
../../subleq_asm_compiler -i _funcs.hsq -o _funcs.sq -v
../../../subleq_assembler/subleq_assembler -i _funcs.sq -o funcs.bin -b 2 -E 0x200 -d _funcs.dbg
