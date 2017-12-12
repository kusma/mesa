#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_strings.h"

#include "tegra_compiler.h"
#include "vpe_ir.h"
#include "fp_ir.h"

static int
read_file(const char *filename, void **ptr, size_t *size)
{
   int fd, ret;
   struct stat st;

   *ptr = MAP_FAILED;

   fd = open(filename, O_RDONLY);
   if (fd == -1) {
      fprintf(stderr, "couldn't open `%s'\n", filename);
      return 1;
   }

   ret = fstat(fd, &st);
   if (ret) {
      fprintf(stderr, "couldn't stat `%s'\n", filename);
      exit(1);
   }

   *size = st.st_size;
   *ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
   if (*ptr == MAP_FAILED) {
      fprintf(stderr, "couldn't map `%s'\n", filename);
      exit(1);
   }

   close(fd);
   return 0;
}

static void
print_usage(void)
{
   printf("Usage: tegra_compiler [OPTIONS]... <file.tgsi>\n");
   printf("    --help            - show this message\n");
}

static void
dump_vpe_shader(struct tegra_vpe_shader *vpe)
{
   printf("words(%d):\n", list_length(&vpe->instructions) * 4);
   struct vpe_instr *last = list_last_entry(&vpe->instructions, struct vpe_instr, link);
   list_for_each_entry(struct vpe_instr, instr, &vpe->instructions, link) {
      uint32_t words[4];
      bool end_of_program = instr == last;
      tegra_vpe_pack(words, instr, end_of_program);
      for (int j = 0; j < 4; ++j)
         printf("\t%08x\n", words[j]);
   }
}

static void
dump_fp_shader(struct tegra_fp_shader *fp)
{
   int num_alu_instrs = list_length(&fp->alu_instructions);
   printf("alu-words(%d):\n", num_alu_instrs * 4 * 2);
   list_for_each_entry(struct fp_alu_instr_packet, instr, &fp->alu_instructions, link) {
      for (int j = 0; j < 4; ++j) {
         uint32_t words[2];
         tegra_fp_pack_alu(words, instr->slots + j);
         for (int k = 0; k < 2; ++k)
            printf("\t%08x\n", words[k]);
      }
   }

   int num_mfu_instrs = list_length(&fp->mfu_instructions);
   printf("mfu-words(%d):\n", num_mfu_instrs * 2);
   list_for_each_entry(struct fp_mfu_instr, instr, &fp->mfu_instructions, link) {
      uint32_t words[2];
      tegra_fp_pack_mfu(words, instr);
      for (int k = 0; k < 2; ++k)
         printf("\t%08x\n", words[k]);
   }

   int num_fp_instrs = list_length(&fp->fp_instructions);
   printf("dw-words(%d):\n", num_fp_instrs);
   list_for_each_entry(struct fp_instr, instr, &fp->fp_instructions, link)
      printf("\t%08x\n", tegra_fp_pack_dw(&instr->dw));
}

static int
compile_tgsi(const char *filename)
{
   void *ptr = NULL;
   size_t size = 0;
   int ret = read_file(filename, &ptr, &size);
   if (ret) {
      print_usage();
      return ret;
   }

   struct tgsi_token toks[65536];
   if (!tgsi_text_translate(ptr, toks, ARRAY_SIZE(toks))) {
      fprintf(stderr, "could not parse `%s'\n", filename);
      return -1;
   }

   tgsi_dump(toks, 0);

   struct tgsi_parse_context parser;
   if (tgsi_parse_init(&parser, toks) != TGSI_PARSE_OK) {
      fprintf(stderr, "tgsi_parse_init failed\n");
      return -1;
   }

   struct tegra_vpe_shader vpe;
   struct tegra_fp_shader fp;
   switch (parser.FullHeader.Processor.Processor) {
   case PIPE_SHADER_VERTEX:
      tegra_tgsi_to_vpe(&vpe, toks);
      dump_vpe_shader(&vpe);
      break;

   case PIPE_SHADER_FRAGMENT:
      tegra_tgsi_to_fp(&fp, toks);
      dump_fp_shader(&fp);
      break;

   default:
      fprintf(stderr, "unexpected shader type: '%s'\n", tgsi_processor_type_names[parser.FullHeader.Processor.Processor]);
      return -1;
   }

   return 0;
}

int
main(int argc, char **argv)
{
   int n;
   for (n = 1; n < argc; ++n) {
      if (!strcmp(argv[n], "--help")) {
         print_usage();
         return 0;
      } else
         break;
   }

   for (; n < argc; ++n) {
      char *filename = argv[n];
      char *ext = rindex(filename, '.');

      if (strcmp(ext, ".tgsi") == 0) {
         if (compile_tgsi(filename) < 0)
            exit(1);
      }
   }
}
