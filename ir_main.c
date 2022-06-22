#include "ir.h"
#include <stdlib.h>
#include <string.h>

static void help(const char *cmd)
{
	printf(
		"Usage: %s [options] input-file...\n"
		"Options:\n"
		"  -O[012]                    - optimiztion level\n"
		"  -S                         - dump final target assembler code\n"
		"  -mavx                      - use AVX instruction set\n"
		"  -muse-fp                   - use base frame pointer register\n"
		"  --emit-c [file-name]       - convert to C source\n"
		"  --save [file-name]         - save IR\n"
		"  --dot  [file-name]         - dump IR graph\n"
		"  --dump [file-name]         - dump IR table\n"
		"  --dump-after-load          - dump IR after load and local optimiztion\n"
		"  --dump-after-sccp          - dump IR after SCCP optimization pass\n"
		"  --dump-after-gcm           - dump IR after GCM optimization pass\n"
		"  --dump-after-schedule      - dump IR after SCHEDULE pass\n"
		"  --dump-after-live-ranges   - dump IR after live ranges identification\n"
		"  --dump-after-coalescing    - dump IR after live ranges identification\n"
		"  --dump-after-all           - dump IR after each pass\n"
		"  --dump-use-lists           - dump def->use lists\n"
		"  --dump-cfg                 - dump CFG (Control Flow Graph)\n"
		"  --dump-cfg-map             - dump CFG map (instruction to BB number)\n"
#ifdef IR_DEBUG
		"  --debug-sccp               - debug SCCP optimization pass\n"
		"  --debug-gcm                - debug GCM optimization pass\n"
		"  --debug-schedule           - debug SCHEDULE optimization pass\n"
		"  --debug-ra                 - debug register allocator\n"
		"  --debug-regset <bit-mask>  - restrict available register set\n"
#endif
		"  --target                   - print JIT target\n"
		"  --version\n"
		"  --help\n",
		cmd);
}

#define IR_DUMP_SAVE                (1<<0)
#define IR_DUMP_DUMP                (1<<1)
#define IR_DUMP_DOT                 (1<<2)
#define IR_DUMP_USE_LISTS           (1<<3)
#define IR_DUMP_CFG                 (1<<4)
#define IR_DUMP_CFG_MAP             (1<<5)
#define IR_DUMP_LIVE_RANGES         (1<<6)

#define IR_DUMP_AFTER_LOAD          (1<<16)
#define IR_DUMP_AFTER_SCCP          (1<<17)
#define IR_DUMP_AFTER_GCM           (1<<18)
#define IR_DUMP_AFTER_SCHEDULE      (1<<19)
#define IR_DUMP_AFTER_LIVE_RANGES   (1<<20)
#define IR_DUMP_AFTER_COALESCING    (1<<21)

#define IR_DUMP_AFTER_ALL           (1<<29)
#define IR_DUMP_FINAL               (1<<30)

static int _save(ir_ctx *ctx, uint32_t dump, uint32_t pass, const char *dump_file)
{
	char fn[4096];
	FILE *f;

	if (dump_file) {
		if (dump & IR_DUMP_AFTER_ALL) {
			if (pass == IR_DUMP_AFTER_LOAD) {
				snprintf(fn, sizeof(fn)-1, "01-load-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_AFTER_SCCP) {
				snprintf(fn, sizeof(fn)-1, "02-sccp-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_AFTER_GCM) {
				snprintf(fn, sizeof(fn)-1, "03-gcm-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_AFTER_SCHEDULE) {
				snprintf(fn, sizeof(fn)-1, "04-schedule-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_AFTER_LIVE_RANGES) {
				snprintf(fn, sizeof(fn)-1, "05-live-ranges-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_AFTER_COALESCING) {
				snprintf(fn, sizeof(fn)-1, "06-coalescing-%s", dump_file);
                dump_file = fn;
			} else if (pass == IR_DUMP_FINAL) {
				snprintf(fn, sizeof(fn)-1, "07-final-%s", dump_file);
                dump_file = fn;
			}
		}
		f = fopen(dump_file, "w+");
		if (!f) {
			fprintf(stderr, "ERROR: Cannot create file '%s'\n", dump_file);
			return 0;
		}
	} else {
	    f = stderr;
	}
	if (dump & IR_DUMP_SAVE) {
		ir_save(ctx, f);
	}
	if (dump & IR_DUMP_DUMP) {
		ir_dump(ctx, f);
	}
	if (dump & IR_DUMP_DOT) {
		ir_dump_dot(ctx, f);
	}
	if (dump & IR_DUMP_USE_LISTS) {
		ir_dump_use_lists(ctx, f);
	}
	if (dump & IR_DUMP_CFG) {
		ir_dump_cfg(ctx, f);
	}
	if (dump & IR_DUMP_CFG_MAP) {
		ir_dump_cfg_map(ctx, f);
	}
	if (dump & IR_DUMP_LIVE_RANGES) {
		ir_dump_live_ranges(ctx, f);
	}
	if (dump_file) {
		fclose(f);
	}
	return 1;
}

int ir_compile_func(ir_ctx *ctx, int opt_level, uint32_t dump, const char *dump_file)
{
	if (opt_level > 0) {
		ir_build_def_use_lists(ctx);
	}

	if ((dump & (IR_DUMP_AFTER_LOAD|IR_DUMP_AFTER_ALL))
	 && !_save(ctx, dump, IR_DUMP_AFTER_LOAD, dump_file)) {
		return 0;
	}

	/* Global Optimization */
	if (opt_level > 1) {
		ir_sccp(ctx);
		if ((dump & (IR_DUMP_AFTER_SCCP|IR_DUMP_AFTER_ALL))
		 && !_save(ctx, dump, IR_DUMP_AFTER_SCCP, dump_file)) {
			return 0;
		}
	}

	/* Schedule */
	if (opt_level > 0) {
		ir_build_cfg(ctx);
		ir_build_dominators_tree(ctx);
		ir_find_loops(ctx);
		ir_gcm(ctx);
		if ((dump & (IR_DUMP_AFTER_GCM|IR_DUMP_AFTER_ALL))
		 && !_save(ctx, dump, IR_DUMP_AFTER_GCM, dump_file)) {
			return 0;
		}
		ir_schedule(ctx);
		if ((dump & (IR_DUMP_AFTER_SCHEDULE|IR_DUMP_AFTER_ALL))
		 && !_save(ctx, dump, IR_DUMP_AFTER_SCHEDULE, dump_file)) {
			return 0;
		}
	} else if (ctx->flags & (IR_GEN_NATIVE|IR_GEN_C)) {
		ir_build_def_use_lists(ctx);
		ir_build_cfg(ctx);
	}

	if (ctx->flags & IR_GEN_NATIVE) {
		ir_match(ctx);
	}

	if (opt_level > 0) {
		ir_assign_virtual_registers(ctx);
		ir_compute_live_ranges(ctx);

		if ((dump & (IR_DUMP_AFTER_LIVE_RANGES|IR_DUMP_AFTER_ALL))
		 && !_save(ctx, dump, IR_DUMP_AFTER_LIVE_RANGES, dump_file)) {
			return 0;
		}

		ir_coalesce(ctx);

		if ((dump & (IR_DUMP_AFTER_COALESCING|IR_DUMP_AFTER_ALL))
		 && !_save(ctx, dump, IR_DUMP_AFTER_COALESCING, dump_file)) {
			return 0;
		}

		if (ctx->flags & IR_GEN_NATIVE) {
			ir_reg_alloc(ctx);
		}

		ir_schedule_blocks(ctx);
	} else if (ctx->flags & (IR_GEN_NATIVE|IR_GEN_C)) {
		ir_assign_virtual_registers(ctx);
		ir_compute_dessa_moves(ctx);
	}

	if ((dump & (IR_DUMP_FINAL|IR_DUMP_AFTER_ALL))
	 && !_save(ctx, dump, IR_DUMP_FINAL, dump_file)) {
		return 0;
	}

	return 1;
}

int main(int argc, char **argv)
{
	int i;
	char *input = NULL;
	char *dump_file = NULL, *c_file = NULL;
	FILE *f;
	ir_ctx ctx;
	bool emit_c = 0, dump_asm = 0, run = 0;
	uint32_t dump = 0;
	int opt_level = 2;
	uint32_t mflags = 0;
	uint64_t debug_regset = 0xffffffffffffffff;

	ir_consistency_check();

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0
		 || strcmp(argv[i], "--help") == 0) {
			help(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("IR %s\n", IR_VERSION);
			return 0;
		} else if (strcmp(argv[i], "--target") == 0) {
			printf("%s\n", IR_TARGET);
			return 0;
		} else if (argv[i][0] == '-' && argv[i][1] == 'O' && strlen(argv[i]) == 3) {
			if (argv[i][2] == '0') {
				opt_level = 0;
			} else if (argv[i][2] == '1') {
				opt_level = 1;
			} else if (argv[i][2] == '2') {
				opt_level = 2;
			} else {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
		} else if (strcmp(argv[i], "--emit-c") == 0) {
			emit_c = 1;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				c_file = argv[i + 1];
				i++;
			}
		} else if (strcmp(argv[i], "--save") == 0) {
			// TODO: check save/dot/dump/... conflicts
			dump |= IR_DUMP_SAVE;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				dump_file = argv[i + 1];
				i++;
			}
		} else if (strcmp(argv[i], "--dot") == 0) {
			dump |= IR_DUMP_DOT;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				dump_file = argv[i + 1];
				i++;
			}
		} else if (strcmp(argv[i], "--dump") == 0) {
			dump |= IR_DUMP_DUMP;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				dump_file = argv[i + 1];
				i++;
			}
		} else if (strcmp(argv[i], "--dump-use-lists") == 0) {
			dump |= IR_DUMP_USE_LISTS;
		} else if (strcmp(argv[i], "--dump-cfg") == 0) {
			dump |= IR_DUMP_CFG;
		} else if (strcmp(argv[i], "--dump-cfg-map") == 0) {
			dump |= IR_DUMP_CFG_MAP;
		} else if (strcmp(argv[i], "--dump-live-ranges") == 0) {
			dump |= IR_DUMP_LIVE_RANGES;
		} else if (strcmp(argv[i], "--dump-after-load") == 0) {
			dump |= IR_DUMP_AFTER_LOAD;
		} else if (strcmp(argv[i], "--dump-after-sccp") == 0) {
			dump |= IR_DUMP_AFTER_SCCP;
		} else if (strcmp(argv[i], "--dump-after-gcm") == 0) {
			dump |= IR_DUMP_AFTER_GCM;
		} else if (strcmp(argv[i], "--dump-after-schedule") == 0) {
			dump |= IR_DUMP_AFTER_SCHEDULE;
		} else if (strcmp(argv[i], "--dump-after-live-ranges") == 0) {
			dump |= IR_DUMP_AFTER_LIVE_RANGES;
		} else if (strcmp(argv[i], "--dump-after-coalescing") == 0) {
			dump |= IR_DUMP_AFTER_COALESCING;
		} else if (strcmp(argv[i], "--dump-after-all") == 0) {
			dump |= IR_DUMP_AFTER_ALL;
		} else if (strcmp(argv[i], "--dump-final") == 0) {
			dump |= IR_DUMP_FINAL;
		} else if (strcmp(argv[i], "-S") == 0) {
			dump_asm = 1;
		} else if (strcmp(argv[i], "--run") == 0) {
			run = 1;
		} else if (strcmp(argv[i], "-mavx") == 0) {
			mflags |= IR_AVX;
		} else if (strcmp(argv[i], "-muse-fp") == 0) {
			mflags |= IR_USE_FRAME_POINTER;
		} else if (strcmp(argv[i], "-mfastcall") == 0) {
			mflags |= IR_FASTCALL_FUNC;
#ifdef IR_DEBUG
		} else if (strcmp(argv[i], "--debug-sccp") == 0) {
			mflags |= IR_DEBUG_SCCP;
		} else if (strcmp(argv[i], "--debug-gcm") == 0) {
			mflags |= IR_DEBUG_GCM;
		} else if (strcmp(argv[i], "--debug-schedule") == 0) {
			mflags |= IR_DEBUG_SCHEDULE;
		} else if (strcmp(argv[i], "--debug-ra") == 0) {
			mflags |= IR_DEBUG_RA;
#endif
		} else if (strcmp(argv[i], "--debug-regset") == 0) {
			if (i + 1 == argc || argv[i + 1][0] == '-') {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
			debug_regset = strtoull(argv[i + 1], NULL, 0);
			i++;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "ERROR: Unknown option '%s' (use --help)\n", argv[i]);
			return 1;
		} else {
			if (input) {
				fprintf(stderr, "ERROR: Invalid usage' (use --help)\n");
				return 1;
			}
			input = argv[i];
		}
	}

	if (dump && !(dump & (IR_DUMP_AFTER_LOAD|IR_DUMP_AFTER_SCCP|
		IR_DUMP_AFTER_GCM|IR_DUMP_AFTER_SCHEDULE|
		IR_DUMP_AFTER_LIVE_RANGES|IR_DUMP_AFTER_COALESCING|IR_DUMP_FINAL))) {
		dump |= IR_DUMP_FINAL;
	}

	if (!input) {
		fprintf(stderr, "ERROR: no input file\n");
		return 1;
	}

	f = fopen(input, "r");
	if (!f) {
		fprintf(stderr, "ERROR: Cannot open input file '%s'\n", input);
		return 1;
	}

	ir_loader_init();

	ir_init(&ctx, 256, 1024);
	ctx.flags |= IR_FUNCTION;
	ctx.flags |= mflags;

	if (opt_level > 0) {
		ctx.flags |= IR_OPT_FOLDING | IR_OPT_CODEGEN;
	}
	if (emit_c) {
		ctx.flags |= IR_GEN_C;
	}
	if (dump_asm || run) {
		ctx.flags |= IR_GEN_NATIVE;
	}
	ctx.fixed_regset = ~debug_regset;

	if (!ir_load(&ctx, f)) {
		fprintf(stderr, "ERROR: Cannot load input file '%s'\n", input);
		return 1;
	}
	fclose(f);

	if (!ir_compile_func(&ctx, opt_level, dump, dump_file)) {
		return 1;
	}

	if (emit_c) {
		if (c_file) {
			f = fopen(c_file, "w+");
			if (!f) {
				fprintf(stderr, "ERROR: Cannot create file '%s'\n", c_file);
				return 0;
			}
		} else {
		    f = stderr;
		}
		ir_emit_c(&ctx, f);
		if (c_file) {
			fclose(f);
		}
	}

	if (dump_asm || run) {
		size_t size;
		void *entry = ir_emit_code(&ctx, &size);

		if (entry) {
			if (dump_asm) {
				ir_disasm_add_symbol("test", (uintptr_t)entry, size);
				ir_disasm("test", entry, size, 0, &ctx, stderr);
			}
			if (run) {
				int (*func)(void) = entry;
				int ret;

				ir_perf_map_register("test", entry, size);
				ir_perf_jitdump_open();
				ir_perf_jitdump_register("test", entry, size);

				ir_mem_unprotect(entry, 4096);
				ir_gdb_register("test", entry, size, 0, 0);
				ir_mem_protect(entry, 4096);

				ret = func();
				fflush(stdout);
				fprintf(stderr, "\nexit code = %d\n", ret);
			}
		}
	}

	ir_free(&ctx);

	ir_loader_free();

	return 0;
}
