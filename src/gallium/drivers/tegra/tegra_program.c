#include <stdio.h>

#include "util/u_memory.h"

#include "tgsi/tgsi_dump.h"

#include "tegra_common.h"
#include "tegra_context.h"
#include "tegra_screen.h"
#include "tegra_program.h"
#include "vpe_ir.h"

static struct vpe_src_operand
src_undef()
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_UNDEF,
      .index = 0,
      .swizzle = { VPE_SWZ_X, VPE_SWZ_Y, VPE_SWZ_Z, VPE_SWZ_W }
   };
   return ret;
}

static struct vpe_src_operand
attrib(int index, const enum vpe_swz swizzle[4])
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_ATTRIB,
      .index = index
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_src_operand
uniform(int index, const enum vpe_swz swizzle[4])
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_UNIFORM,
      .index = index
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_src_operand
src_temp(int index, const enum vpe_swz swizzle[4])
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_TEMP,
      .index = index
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_dst_operand
output(int index, unsigned int write_mask)
{
   struct vpe_dst_operand ret = {
      .file = VPE_DST_FILE_OUTPUT,
      .index = index ? 7 : 0, // HACK: do proper mapping later!
      .write_mask = write_mask
   };
   return ret;
}

static struct vpe_dst_operand
dst_temp(int index, unsigned int write_mask)
{
   struct vpe_dst_operand ret = {
      .file = VPE_DST_FILE_TEMP,
      .index = index,
      .write_mask = write_mask
   };
   return ret;
}

static struct vpe_vec_instr
emit_vec_unop(enum vpe_vec_op op, struct vpe_dst_operand dst,
              struct vpe_src_operand src)
{
   struct vpe_vec_instr ret = {
      .op = op,
      .dst = dst,
      .src = { src, src_undef(), src_undef() }
   };
   return ret;
}

static struct vpe_vec_instr
emit_vMOV(struct vpe_dst_operand dst, struct vpe_src_operand src)
{
   return emit_vec_unop(VPE_VEC_OP_MOV, dst, src);
}

static struct vpe_vec_instr
emit_vADD(struct vpe_dst_operand dst, struct vpe_src_operand src0,
          struct vpe_src_operand src2)
{
   struct vpe_vec_instr ret = {
      .op = VPE_VEC_OP_ADD,
      .dst = dst,
      .src = { src0, src_undef(), src2 } // add is "strange" in that it takes src0 and src2
   };
   return ret;
}

static struct vpe_scalar_instr
emit_sNOP()
{
   struct vpe_scalar_instr ret = {
      .op = VPE_SCALAR_OP_NOP
      /* TODO: fill in more */
   };
   return ret;
}

static struct vpe_instr
emit_packed(struct vpe_vec_instr vec, struct vpe_scalar_instr scalar)
{
   struct vpe_instr ret = {
      ret.vec = vec,
      ret.scalar = scalar
   };
   return ret;
}

static void *
tegra_create_vs_state(struct pipe_context *pcontext,
                      const struct pipe_shader_state *template)
{
   struct tegra_vs_state *so = CALLOC_STRUCT(tegra_vs_state);
   if (!so)
      return NULL;

   so->base = *template;

   if (tegra_debug & TEGRA_DEBUG_TGSI) {
      fprintf(stderr, "DEBUG: TGSI:\n");
      tgsi_dump(template->tokens, 0);
      fprintf(stderr, "\n");
   }

   /* TODO: generate code! */

   return so;
}

static void
tegra_bind_vs_state(struct pipe_context *pcontext, void *so)
{
   unimplemented();
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
   struct tegra_fs_state *so = CALLOC_STRUCT(tegra_fs_state);
   if (!so)
      return NULL;

   so->base = *template;

   if (tegra_debug & TEGRA_DEBUG_TGSI) {
      fprintf(stderr, "DEBUG: TGSI:\n");
      tgsi_dump(template->tokens, 0);
      fprintf(stderr, "\n");
   }

   /* TODO: generate code! */

   return so;
}

static void
tegra_bind_fs_state(struct pipe_context *pcontext, void *so)
{
   unimplemented();
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
