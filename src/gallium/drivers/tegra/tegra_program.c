#include <stdio.h>

#include "util/u_memory.h"

#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"

#include "host1x01_hardware.h"
#include "tegra_common.h"
#include "tegra_context.h"
#include "tegra_screen.h"
#include "tegra_program.h"
#include "tgr_3d.xml.h"
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
attrib(int index, const enum vpe_swz swizzle[4], bool negate, bool absolute)
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_ATTRIB,
      .index = index,
      .negate = negate,
      .absolute = absolute
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_src_operand
uniform(int index, const enum vpe_swz swizzle[4], bool negate, bool absolute)
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_UNIFORM,
      .index = index,
      .negate = negate,
      .absolute = absolute
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_src_operand
src_temp(int index, const enum vpe_swz swizzle[4], bool negate, bool absolute)
{
   struct vpe_src_operand ret = {
      .file = VPE_SRC_FILE_TEMP,
      .index = index,
      .negate = negate,
      .absolute = absolute
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vpe_dst_operand
dst_undef()
{
   struct vpe_dst_operand ret = {
      .file = VPE_DST_FILE_UNDEF,
      .index = 0,
      .write_mask = 0,
      .saturate = 0
   };
   return ret;
}

static struct vpe_dst_operand
output(int index, unsigned int write_mask, bool saturate)
{
   struct vpe_dst_operand ret = {
      .file = VPE_DST_FILE_OUTPUT,
      .index = index ? 7 : 0, // HACK: do proper mapping later!
      .write_mask = write_mask,
      .saturate = saturate
   };
   return ret;
}

static struct vpe_dst_operand
dst_temp(int index, unsigned int write_mask, bool saturate)
{
   struct vpe_dst_operand ret = {
      .file = VPE_DST_FILE_TEMP,
      .index = index,
      .write_mask = write_mask,
      .saturate = saturate
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
emit_vec_binop(enum vpe_vec_op op, struct vpe_dst_operand dst,
              struct vpe_src_operand src0, struct vpe_src_operand src1)
{
   struct vpe_vec_instr ret = {
      .op = op,
      .dst = dst,
      .src = { src0, src1, src_undef() }
   };
   return ret;
}

static struct vpe_vec_instr
emit_vNOP()
{
   struct vpe_vec_instr ret = {
      .op = VPE_VEC_OP_NOP,
      .dst = dst_undef(),
      .src = { src_undef(), src_undef(), src_undef() }
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

#define GEN_V_BINOP(OP) \
static struct vpe_vec_instr \
emit_v ## OP (struct vpe_dst_operand dst, struct vpe_src_operand src0, \
          struct vpe_src_operand src1) \
{ \
   return emit_vec_binop(VPE_VEC_OP_ ## OP, dst, src0, src1); \
}

GEN_V_BINOP(MUL)
GEN_V_BINOP(DP3)
GEN_V_BINOP(DP4)
GEN_V_BINOP(SLT)
GEN_V_BINOP(MAX)

static struct vpe_vec_instr
emit_vMAD(struct vpe_dst_operand dst, struct vpe_src_operand src0,
          struct vpe_src_operand src1, struct vpe_src_operand src2)
{
   struct vpe_vec_instr ret = {
      .op = VPE_VEC_OP_MAD,
      .dst = dst,
      .src = { src0, src1, src2 }
   };
   return ret;
}

static struct vpe_scalar_instr
emit_sNOP()
{
   struct vpe_scalar_instr ret = {
      .op = VPE_SCALAR_OP_NOP,
      .dst = dst_undef(),
      .src = src_undef()
   };
   return ret;
}

#define GEN_S_UNOP(OP) \
static struct vpe_scalar_instr \
emit_s ## OP (struct vpe_dst_operand dst, struct vpe_src_operand src) \
{ \
   struct vpe_scalar_instr ret = { \
      .op = VPE_SCALAR_OP_ ## OP, \
      .dst = dst, \
      .src = src \
   }; \
   return ret; \
}

GEN_S_UNOP(RSQ)

static struct vpe_instr
emit_packed(struct vpe_vec_instr vec, struct vpe_scalar_instr scalar)
{
   struct vpe_instr ret = {
      ret.vec = vec,
      ret.scalar = scalar
   };
   return ret;
}

static struct vpe_dst_operand
tgsi_dst_to_vpe(const struct tgsi_dst_register *dst, bool saturate)
{
   switch (dst->File) {
   case TGSI_FILE_OUTPUT:
      return output(dst->Index, dst->WriteMask, saturate);

   case TGSI_FILE_TEMPORARY:
      return dst_temp(dst->Index, dst->WriteMask, saturate);

   default:
      unreachable("unsupported output");
   }
}

static struct vpe_src_operand
tgsi_src_to_vpe(const struct tgsi_src_register *src)
{
   enum vpe_swz swizzle[4] = {
      src->SwizzleX,
      src->SwizzleY,
      src->SwizzleZ,
      src->SwizzleW
   };
   bool negate = src->Negate != 0;
   bool absolute = src->Absolute != 0;

   switch (src->File) {
   case TGSI_FILE_INPUT:
      return attrib(src->Index, swizzle, negate, absolute);

   case TGSI_FILE_CONSTANT:
      return uniform(src->Index, swizzle, negate, absolute);

   case TGSI_FILE_TEMPORARY:
      return src_temp(src->Index, swizzle, negate, absolute);

   case TGSI_FILE_IMMEDIATE:
      /* HACK: allocate uniforms from the top for immediates; need to actually record these */
      return uniform(1023 - src->Index, swizzle, negate, absolute);

   default:
      fprintf(stderr, "unsupported input: %d\n", src->File);
      unreachable("unsupported input!");
   }
}


static struct vpe_instr
tgsi_to_vpe(const struct tgsi_full_instruction *inst)
{
   bool saturate = inst->Instruction.Saturate != 0;
   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_MOV:
      return emit_packed(emit_vMOV(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_ADD:
      return emit_packed(emit_vADD(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_MUL:
      return emit_packed(emit_vMUL(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_DP3:
      return emit_packed(emit_vDP3(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_DP4:
      return emit_packed(emit_vDP4(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_SLT:
      return emit_packed(emit_vSLT(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_MAX:
      return emit_packed(emit_vMAX(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_MAD:
      return emit_packed(emit_vMAD(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register),
                                   tgsi_src_to_vpe(&inst->Src[1].Register),
                                   tgsi_src_to_vpe(&inst->Src[2].Register)),
                         emit_sNOP());

   case TGSI_OPCODE_RSQ:
      return emit_packed(emit_vNOP(),
                         emit_sRSQ(tgsi_dst_to_vpe(&inst->Dst[0].Register, saturate),
                                   tgsi_src_to_vpe(&inst->Src[0].Register)));

   default:
      unreachable("unsupported TGSI-opcode!");
   }
}

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

   struct vpe_instr vpe_instrs[256];
   int num_vpe_instrs = 0;

   while (!tgsi_parse_end_of_tokens(&parser)) {
      tgsi_parse_token(&parser);
      switch (parser.FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         if (parser.FullToken.FullInstruction.Instruction.Opcode != TGSI_OPCODE_END)
            vpe_instrs[num_vpe_instrs++] = tgsi_to_vpe(&parser.FullToken.FullInstruction);
         break;
      }
   }

   assert(num_vpe_instrs < 256);
   int num_commands = 2 + num_vpe_instrs * 4;
   uint32_t *commands = MALLOC(num_commands * sizeof(uint32_t));
   if (!commands) {
      FREE(so);
      return NULL;
   }

   commands[0] = host1x_opcode_imm(TGR3D_VP_UPLOAD_INST_ID, 0);
   commands[1] = host1x_opcode_nonincr(TGR3D_VP_UPLOAD_INST, num_vpe_instrs * 4);
   for (int i = 0; i < num_vpe_instrs; ++i) {
      bool end_of_program = i == (num_vpe_instrs - 1);
      tegra_vpe_pack(commands + 2 + i * 4, vpe_instrs[i], end_of_program);
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

   uint32_t prologue[] = {
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

   int num_commands = ARRAY_SIZE(prologue);

   uint32_t *commands = MALLOC(num_commands * sizeof(uint32_t));
   if (!commands) {
      FREE(so);
      return NULL;
   }

   memcpy(commands, prologue, sizeof(prologue));

   so->commands = commands;
   so->num_commands = num_commands;
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
