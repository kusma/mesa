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

static void print_usage(void)
{
   printf("Usage: tegra_compiler [OPTIONS]... <file.tgsi | file.vert | file.frag>\n");
   printf("    --help            - show this message\n");
}

int main(int argc, char **argv)
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
         exit(1);
      }

      tgsi_dump(toks, 0);

      struct tgsi_parse_context parser;
      if (tgsi_parse_init(&parser, toks) != TGSI_PARSE_OK) {
         fprintf(stderr, "tgsi_parse_init failed\n");
         exit(1);
      }

      struct tegra_vpe_shader vpe;
      switch (parser.FullHeader.Processor.Processor) {
      case PIPE_SHADER_VERTEX:
         tegra_tgsi_to_vpe(&vpe, &parser);

         printf("words(%d):\n", vpe.num_instructions * 4);
         for (int i = 0; i < vpe.num_instructions; ++i) {
            uint32_t words[4];
            bool end_of_program = i == (vpe.num_instructions - 1);
            tegra_vpe_pack(words, vpe.instructions[i], end_of_program);
            for (int j = 0; j < 4; ++j)
               printf("\t%08x\n", words[j]);
         }
         break;

      default:
         fprintf(stderr, "unexpected shader type: '%s'\n", tgsi_processor_type_names[parser.FullHeader.Processor.Processor]);
         exit(1);
      }
   }
}
