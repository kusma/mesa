#include <stdio.h>
#include <string.h>

#include "util/u_dynarray.h"
#include "util/u_memory.h"

#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"

#include "host1x01_hardware.h"
#include "tegra_common.h"
#include "tegra_context.h"
#include "tegra_screen.h"
#include "tegra_program.h"
#include "tegra_compiler.h"
#include "tgr_3d.xml.h"

static void *
tegra_create_vs_state(struct pipe_context *pcontext,
                      const struct pipe_shader_state *template)
{
   struct tegra_shader_state *so = CALLOC_STRUCT(tegra_shader_state);
   if (!so)
      return NULL;

   so->base = *template;

   if (tegra_debug & TEGRA_DEBUG_TGSI) {
      fprintf(stderr, "DEBUG: TGSI:\n");
      tgsi_dump(template->tokens, 0);
      fprintf(stderr, "\n");
   }

   struct tgsi_parse_context parser;
   unsigned ok = tgsi_parse_init(&parser, template->tokens);
   assert(ok == TGSI_PARSE_OK);

   struct tegra_vpe_shader vpe;
   tegra_tgsi_to_vpe(&vpe, &parser);

   assert(vpe.num_instructions < 256);
   int num_commands = 2 + vpe.num_instructions * 4;
   uint32_t *commands = MALLOC(num_commands * sizeof(uint32_t));
   if (!commands) {
      FREE(so);
      return NULL;
   }

   commands[0] = host1x_opcode_imm(TGR3D_VP_UPLOAD_INST_ID, 0);
   commands[1] = host1x_opcode_nonincr(TGR3D_VP_UPLOAD_INST,
                                       vpe.num_instructions * 4);

   for (int i = 0; i < vpe.num_instructions; ++i) {
      bool end_of_program = i == (vpe.num_instructions - 1);
      tegra_vpe_pack(commands + 2 + i * 4, vpe.instructions[i], end_of_program);
   }

   so->commands = commands;
   so->num_commands = num_commands;
   return so;
}

static void
tegra_bind_vs_state(struct pipe_context *pcontext, void *so)
{
   tegra_context(pcontext)->vshader = so;
}

static void
tegra_delete_vs_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

static void *
tegra_create_fs_state(struct pipe_context *pcontext,
                      const struct pipe_shader_state *template)
{
   struct tegra_shader_state *so = CALLOC_STRUCT(tegra_shader_state);
   if (!so)
      return NULL;

   so->base = *template;

   if (tegra_debug & TEGRA_DEBUG_TGSI) {
      fprintf(stderr, "DEBUG: TGSI:\n");
      tgsi_dump(template->tokens, 0);
      fprintf(stderr, "\n");
   }

   struct tgsi_parse_context parser;
   unsigned ok = tgsi_parse_init(&parser, template->tokens);
   assert(ok == TGSI_PARSE_OK);

   struct tegra_fp_shader fp;
   tegra_tgsi_to_fp(&fp, &parser);

   struct util_dynarray buf;
   util_dynarray_init(&buf, NULL);

#define PUSH(x) util_dynarray_append(&buf, uint32_t, (x))
   PUSH(host1x_opcode_incr(TGR3D_ALU_BUFFER_SIZE, 1));
   PUSH(0x58000000);

   PUSH(host1x_opcode_imm(TGR3D_FP_PSEQ_QUAD_ID, 0));
   PUSH(host1x_opcode_imm(TGR3D_FP_UPLOAD_INST_ID_COMMON, 0));
   PUSH(host1x_opcode_imm(TGR3D_FP_UPLOAD_MFU_INST_ID, 0));
   PUSH(host1x_opcode_imm(TGR3D_FP_UPLOAD_ALU_INST_ID, 0));

   PUSH(host1x_opcode_incr(TGR3D_FP_PSEQ_ENGINE_INST, 1));
   PUSH(0x20006001);

   PUSH(host1x_opcode_incr(TGR3D_FP_PSEQ_DW_CFG, 1));
   PUSH(0x00000040);

   PUSH(host1x_opcode_incr(TGR3D_FP_PSEQ_UPLOAD_INST_BUFFER_FLUSH, 1));
   PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_PSEQ_UPLOAD_INST, 1));
   PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_MFU_SCHED, 1));
   PUSH(0x00000001);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_MFU_INST,
        fp.num_instructions * 2));
   for (int i = 0; i < fp.num_instructions; ++i) {
      uint32_t words[2];
      tegra_fp_pack_mfu(words, &fp.instructions[i].mfu);
      PUSH(words[0]);
      PUSH(words[1]);
   }

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_TEX_INST, 1));
   PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_SCHED,1));
   PUSH(0x00000001);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_INST,
        fp.num_instructions * 4 * 2));
   for (int i = 0; i < fp.num_instructions; ++i) {
      for (int j = 0; j < 4; ++j) {
         uint32_t words[2];
	 tegra_fp_pack_alu(words, fp.instructions[i].alu + j);
	 PUSH(words[0]);
	 PUSH(words[1]);
      }
   }

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_INST_COMPLEMENT, 1));
   PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(TGR3D_FP_UPLOAD_DW_INST,
        fp.num_instructions));
   for (int i = 0; i < fp.num_instructions; ++i)
      PUSH(tegra_fp_pack_dw(&fp.instructions[i].dw));

   PUSH(host1x_opcode_imm(TGR3D_TRAM_SETUP, 0x0140));
#undef PUSH
   util_dynarray_trim(&buf);

   so->num_commands = buf.size / sizeof(uint32_t);
   so->commands = buf.data;
   return so;
}

static void
tegra_bind_fs_state(struct pipe_context *pcontext, void *so)
{
   tegra_context(pcontext)->fshader = so;
}

static void
tegra_delete_fs_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

void
tegra_context_program_init(struct pipe_context *pcontext)
{
   pcontext->create_vs_state = tegra_create_vs_state;
   pcontext->bind_vs_state = tegra_bind_vs_state;
   pcontext->delete_vs_state = tegra_delete_vs_state;

   pcontext->create_fs_state = tegra_create_fs_state;
   pcontext->bind_fs_state = tegra_bind_fs_state;
   pcontext->delete_fs_state = tegra_delete_fs_state;
}
