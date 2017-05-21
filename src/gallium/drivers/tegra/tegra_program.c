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
   struct vpe_src_operand ret = {};
   ret.file = VPE_SRC_FILE_UNDEF;
   ret.index = 0;
   ret.swizzle[0] = VPE_SWZ_X;
   ret.swizzle[1] = VPE_SWZ_Y;
   ret.swizzle[2] = VPE_SWZ_Z;
   ret.swizzle[3] = VPE_SWZ_W;
   return ret;
}

static struct vpe_src_operand
attrib(int index, const enum vpe_swz swizzle[4])
{
   struct vpe_src_operand ret = {};
   ret.file = VPE_SRC_FILE_ATTRIB;
   ret.index = index;
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_src_operand
uniform(int index, const enum vpe_swz swizzle[4])
{
   struct vpe_src_operand ret = {};
   ret.file = VPE_SRC_FILE_UNIFORM;
   ret.index = index;
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}


static struct vpe_dst_operand
output(int index, unsigned int write_mask)
{
   struct vpe_dst_operand ret = {};
   ret.file = VPE_DST_FILE_OUTPUT;
   ret.index = index;
   ret.write_mask = write_mask;
   return ret;
}

static struct vpe_vec_instr
emit_vec_unop(enum vpe_vec_op op, struct vpe_dst_operand dst,
              struct vpe_src_operand src)
{
   struct vpe_vec_instr ret = {};
   ret.op = op;
   ret.dst = dst;
   ret.src[0] = src;
   ret.src[1] = src; // src_undef();
   ret.src[2] = src; // src_undef();
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
   struct vpe_vec_instr ret = {};
   ret.op = VPE_VEC_OP_ADD;
   ret.dst = dst;
   ret.src[0] = src0;
   ret.src[1] = src_undef(); // add is "strange" in that it takes src0 and src2
   ret.src[2] = src2;
   return ret;
}

static struct vpe_scalar_instr
emit_sNOP()
{
   struct vpe_scalar_instr ret = {};
   ret.op = VPE_SCALAR_OP_NOP;
   /* TODO: fill in more */
   return ret;
}

static struct vpe_instr
emit_packed(struct vpe_vec_instr vec, struct vpe_scalar_instr scalar)
{
   struct vpe_instr ret = {};
   ret.vec = vec;
   ret.scalar = scalar;
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

   static const enum vpe_swz identity_swizzle[] = { VPE_SWZ_X, VPE_SWZ_Y, VPE_SWZ_Z, VPE_SWZ_W };

   static uint32_t vp_insts[4 * 2];
   tegra_vpe_pack(vp_insts,
                  emit_packed(emit_vADD(output(0, (1 << 4) - 1),
                                        attrib(0, identity_swizzle),
                                        uniform(0, identity_swizzle)),
                              emit_sNOP()),
                  false);
   tegra_vpe_pack(vp_insts + 4,
                  emit_packed(emit_vMOV(output(7, (1 << 4) - 1),
                                        attrib(1, identity_swizzle)),
                              emit_sNOP()),
                  true);

   so->vp_insts = vp_insts;
   so->num_vp_insts = ARRAY_SIZE(vp_insts);

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

   static uint32_t pseq_insts[] = {
      0x00000000
   };
   so->pseq_insts = pseq_insts;
   so->num_pseq_insts = ARRAY_SIZE(pseq_insts);

   static uint32_t mfu_scheds[] = {
      0x00000001
   };
   so->mfu_scheds = mfu_scheds;
   so->num_mfu_scheds = ARRAY_SIZE(mfu_scheds);

   static uint32_t mfu_insts[] = {
      0x104e51ba,
      0x00408102
   };
   so->mfu_insts = mfu_insts;
   so->num_mfu_insts = ARRAY_SIZE(mfu_insts);

   static uint32_t tex_insts[] = {
      0x00000000
   };
   so->tex_insts = tex_insts;
   so->num_tex_insts = ARRAY_SIZE(tex_insts);

   static uint32_t alu_scheds[] = {
      0x00000001
   };
   so->alu_scheds = alu_scheds;
   so->num_alu_scheds = ARRAY_SIZE(alu_scheds);

   static uint32_t alu_insts[] = {
      0x0001c0c0,
      0x3f41f200,
      0x0001a080,
      0x3f41f200,
      0x00014000,
      0x3f41f200,
      0x00012040,
      0x3f41f200
   };
   so->alu_insts = alu_insts;
   so->num_alu_insts = ARRAY_SIZE(alu_insts);

   static uint32_t dw_insts[] = {
      0x00028005
   };
   so->dw_insts = dw_insts;
   so->num_dw_insts = ARRAY_SIZE(dw_insts);

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
