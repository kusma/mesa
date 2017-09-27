#include <stdio.h>

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

   /* TODO: generate code! */

   uint32_t commands[] = {
      host1x_opcode_incr(TGR3D_ALU_BUFFER_SIZE, 1),
      0x58000000,

      host1x_opcode_imm(TGR3D_FP_PSEQ_QUAD_ID, 0),
      host1x_opcode_imm(TGR3D_FP_UPLOAD_INST_ID_COMMON, 0),
      host1x_opcode_imm(TGR3D_FP_UPLOAD_MFU_INST_ID, 0),
      host1x_opcode_imm(TGR3D_FP_UPLOAD_ALU_INST_ID, 0),

      host1x_opcode_incr(TGR3D_FP_PSEQ_ENGINE_INST, 1),
      0x20006001,

      host1x_opcode_incr(TGR3D_FP_PSEQ_DW_CFG, 1),
      0x00000040,

      host1x_opcode_incr(TGR3D_FP_PSEQ_UPLOAD_INST_BUFFER_FLUSH, 1),
      0x00000000,

      host1x_opcode_nonincr(TGR3D_FP_PSEQ_UPLOAD_INST, 1),
      0x00000000,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_MFU_SCHED, 1),
      0x00000001,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_MFU_INST, 2),
      0x104e51ba,
      0x00408102,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_TEX_INST, 1),
      0x00000000,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_SCHED,1),
      0x00000001,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_INST, 8),
      0x0001c0c0,
      0x3f41f231,
      0x0001a040,
      0x3f41f231,
      0x00014000,
      0x3f41f231,
      0x00012080,
      0x3f41f231,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_ALU_INST_COMPLEMENT, 1),
      0x00000000,

      host1x_opcode_nonincr(TGR3D_FP_UPLOAD_DW_INST, 1),
      0x00028005,

      host1x_opcode_imm(TGR3D_TRAM_SETUP, 0x0140)
   };

   so->num_commands = ARRAY_SIZE(commands);

   so->commands = MALLOC(so->num_commands * sizeof(uint32_t));
   if (!so->commands) {
      FREE(so);
      return NULL;
   }

   memcpy(so->commands, commands, sizeof(commands));
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
