#include "tegra_compiler.h"

#include "tgsi/tgsi_parse.h"

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

static struct fp_alu_src_operand
fp_alu_src_row(int index)
{
   assert(index >= 0 && index < 16);
   struct fp_alu_src_operand src = {
      .index = index
   };
   return src;
}

static struct fp_alu_src_operand
fp_alu_src_reg(int index)
{
   assert(index >= 0 && index < 8);
   struct fp_alu_src_operand src = {
      .index = 16 + index
   };
   return src;
}

static struct fp_alu_src_operand
fp_alu_src_zero()
{
   struct fp_alu_src_operand src = {
      .index = 31,
      .datatype = FP_DATATYPE_FIXED10,
      .sub_reg_select_high = 0
   };
   return src;
}

static struct fp_alu_src_operand
fp_alu_src_one()
{
   struct fp_alu_src_operand src = {
      .index = 31,
      .datatype = FP_DATATYPE_FIXED10,
      .sub_reg_select_high = 1
   };
   return src;
}

void
tegra_tgsi_to_vpe(struct tegra_vpe_shader *vpe, struct tgsi_parse_context *tgsi)
{
   vpe->num_instructions = 0;

   while (!tgsi_parse_end_of_tokens(tgsi)) {
      tgsi_parse_token(tgsi);
      switch (tgsi->FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         if (tgsi->FullToken.FullInstruction.Instruction.Opcode != TGSI_OPCODE_END)
            vpe->instructions[vpe->num_instructions++] = tgsi_to_vpe(&tgsi->FullToken.FullInstruction);
         break;
      }
   }
}

static void
fp_alu_MOV(struct fp_instr *inst, const struct tgsi_dst_register *dst,
           bool saturate, const struct tgsi_src_register *src)
{
   for (int i = 0; i < 4; ++i) {
/*
      if ((dst->WriteMask & (1 << i)) == 0)
         continue;
*/
      inst->alu[i].op = FP_ALU_OP_MAD;

      inst->alu[i].dst.index = 2 + dst->Index; // +2 to match hard-coded store shader for now
      if (dst->File == TGSI_FILE_OUTPUT) {
         // fixed10
         // swizzle RGBA -> BGRA
         int o = i < 3 ? (2 - i) : 3;
         inst->alu[i].dst.index += o / 2;
         inst->alu[i].dst.write_low_sub_reg = (o % 2) == 0;
         inst->alu[i].dst.write_high_sub_reg = (o % 2) != 0;
      } else {
         inst->alu[i].dst.index += i;
      }
      inst->alu[i].dst.saturate = saturate;

      if (src->File == TGSI_FILE_INPUT) {
         inst->mfu.var[i].op = FP_VAR_OP_FP20;
         inst->mfu.var[i].tram_row = src->Index;
         inst->alu[i].src[0] = fp_alu_src_row(i);
      } else {
         inst->alu[i].src[0] = fp_alu_src_reg(src->Index + i);
      }

      inst->alu[i].src[1] = fp_alu_src_one();
      inst->alu[i].src[2] = fp_alu_src_zero();
      inst->alu[i].src[3] = fp_alu_src_one();
   }

   if (dst->File == TGSI_FILE_OUTPUT) {
      inst->dw.enable = 1;
      inst->dw.index = 1; // hard-coded for now
      inst->dw.stencil_write = 0;
      inst->dw.src_regs = FP_DW_REGS_R2_R3; // hard-coded for now
   }
};

static struct fp_instr
tgsi_to_fp(const struct tgsi_full_instruction *inst)
{
   bool saturate = inst->Instruction.Saturate != 0;

   struct fp_instr ret = { };
   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_MOV:
      fp_alu_MOV(&ret, &inst->Dst[0].Register, saturate,
                       &inst->Src[0].Register);
      return ret;

   default:
      unreachable("unsupported TGSI-opcode!");
   }
}

void
tegra_tgsi_to_fp(struct tegra_fp_shader *fp, struct tgsi_parse_context *tgsi)
{
   fp->num_instructions = 0;

   while (!tgsi_parse_end_of_tokens(tgsi)) {
      tgsi_parse_token(tgsi);
      switch (tgsi->FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         if (tgsi->FullToken.FullInstruction.Instruction.Opcode != TGSI_OPCODE_END)
            fp->instructions[fp->num_instructions++] = tgsi_to_fp(&tgsi->FullToken.FullInstruction);
         break;
      }
   }

   /*
    * HACK: insert barycentric interpolation setup
    * This will overwrite instructions in some cases, need proper scheduler
    * to fix properly
    */

    fp->instructions[0].mfu.sfu.op = FP_SFU_OP_RCP;
    fp->instructions[0].mfu.sfu.reg = 4;
    fp->instructions[0].mfu.mul[0].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
    fp->instructions[0].mfu.mul[0].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
    fp->instructions[0].mfu.mul[0].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_0;

    fp->instructions[0].mfu.mul[1].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
    fp->instructions[0].mfu.mul[1].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
    fp->instructions[0].mfu.mul[1].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_1;
}
