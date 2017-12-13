#include "tegra_compiler.h"
#include "tegra_common.h"
#include "fp_ir.h"

#include "tgsi/tgsi_parse.h"

#include "util/u_memory.h"

#include "nir/tgsi_to_nir.h"


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

static struct fp_alu_instr
fp_alu_sMOV(struct fp_alu_dst_operand dst, struct fp_alu_src_operand src)
{
   struct fp_alu_instr ret = {
      .op = FP_ALU_OP_MAD,
      .dst = dst,
      .src = {
         src,
         fp_alu_src_one(),
         fp_alu_src_zero(),
         fp_alu_src_one()
      }
   };
   return ret;
}

static struct fp_alu_dst_operand
fp_alu_dst(const struct tgsi_dst_register *dst, int subreg, bool saturate)
{
   struct fp_alu_dst_operand ret = { 0 };

   ret.index = dst->Index;
   if (dst->File == TGSI_FILE_OUTPUT) {
      ret.index = 2; // HACK: r2+r3 to match hard-coded store shader for now

      // fixed10
      // swizzle RGBA -> BGRA
      int o = subreg < 3 ? (2 - subreg) : 3;
      ret.index += o / 2;
      ret.write_low_sub_reg = (o % 2) == 0;
      ret.write_high_sub_reg = (o % 2) != 0;
   } else
      ret.index += subreg;

   ret.saturate = saturate;

   return ret;
}

static void
emit_vMOV(struct tegra_fp_shader *fp, const struct tgsi_dst_register *dst,
          bool saturate, const struct tgsi_src_register *src)
{
   struct fp_instr *inst = CALLOC_STRUCT(fp_instr);
   list_inithead(&inst->link);

   struct fp_mfu_instr *mfu = NULL;
   if (src->File == TGSI_FILE_INPUT) {
      mfu = CALLOC_STRUCT(fp_mfu_instr);
      list_inithead(&mfu->link);
   }

   int swizzle[] = {
      src->SwizzleX,
      src->SwizzleY,
      src->SwizzleZ,
      src->SwizzleW
   };

   struct fp_alu_instr_packet *alu = CALLOC_STRUCT(fp_alu_instr_packet);
   int alu_instrs = 0;
   list_inithead(&alu->link);
   for (int i = 0; i < 4; ++i) {
      if ((dst->WriteMask & (1 << i)) == 0)
         continue;

      int comp = swizzle[i];

      struct fp_alu_src_operand src0 = { };
      if (src->File == TGSI_FILE_INPUT) {
         mfu->var[i].op = FP_VAR_OP_FP20;
         mfu->var[i].tram_row = src->Index;
         fp->info.max_tram_row = MAX2(fp->info.max_tram_row, src->Index);
         src0 = fp_alu_src_row(comp);
      } else
         src0 = fp_alu_src_reg(src->Index + comp);

      alu->slots[alu_instrs++] = fp_alu_sMOV(fp_alu_dst(dst, i, saturate), src0);
   }
   inst->alu_sched.num_instructions = 1;
   inst->alu_sched.address = list_length(&fp->fp_instructions);

   if (mfu != NULL) {
      inst->mfu_sched.num_instructions = 1;
      inst->mfu_sched.address = list_length(&fp->fp_instructions);
      list_addtail(&mfu->link, &fp->mfu_instructions);
   }

   if (dst->File == TGSI_FILE_OUTPUT) {
      inst->dw.enable = 1;
      inst->dw.index = 1 + dst->Index;
      inst->dw.stencil_write = 0;
      inst->dw.src_regs = FP_DW_REGS_R2_R3; // hard-coded for now
   }

   list_addtail(&alu->link, &fp->alu_instructions);
   list_addtail(&inst->link, &fp->fp_instructions);
}

static void
emit_tgsi_instr(struct tegra_fp_shader *fp, const struct tgsi_full_instruction *inst)
{
   bool saturate = inst->Instruction.Saturate != 0;

   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_MOV:
      emit_vMOV(fp, &inst->Dst[0].Register, saturate,
                    &inst->Src[0].Register);
      break;

   default:
      unreachable("unsupported TGSI-opcode!");
   }
}

#define LINK_SRC(index) ((index) << 3)
#define LINK_DST(index, comp, type) (((comp) | (type) << 2) << ((index) * 4))
#define LINK_DST_NONE      0
#define LINK_DST_FX10_LOW  1
#define LINK_DST_FX10_HIGH 2
#define LINK_DST_FP20      3

static void
emit_tgsi_input(struct tegra_fp_shader *fp, const struct tgsi_full_declaration *decl)
{
   assert(decl->Range.First == decl->Range.Last);

   uint32_t src = LINK_SRC(1 + decl->Range.First);
   uint32_t dst = 0;
   for (int i = 0; i < 4; ++i)
      dst |= LINK_DST(i, i, LINK_DST_FP20);

   fp->info.inputs[fp->info.num_inputs].src = src;
   fp->info.inputs[fp->info.num_inputs].dst = dst;

   if (decl->Declaration.Semantic == TGSI_SEMANTIC_COLOR) {
      assert(fp->info.color_input < 0);
      fp->info.color_input = fp->info.num_inputs;
   }

   fp->info.num_inputs++;
}

static void
emit_tgsi_declaration(struct tegra_fp_shader *fp, const struct tgsi_full_declaration *decl)
{
   switch (decl->Declaration.File) {
   case TGSI_FILE_INPUT:
      emit_tgsi_input(fp, decl);
      break;
   }
}

static void
emit_alu(struct tegra_fp_shader *fp, nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];

   switch (alu->op) {
/*
   case nir_op_vec2:
   case nir_op_vec3:
   case nir_op_vec4:
      for (int i = 0; i < info->num_inputs; i++) {
         nir_alu_src *src = &alu->src[i];
         printf("lol %d: %d\n", i, src->src.is_ssa);
      }
      break;
*/

   default:
      printf("emit_alu: not implemented (%s)\n", info->name);
      break;
   }
}

static void
emit_intrinsic(struct tegra_fp_shader *fp, nir_intrinsic_instr *intr)
{
   const nir_intrinsic_info *info = &nir_intrinsic_infos[intr->intrinsic];

   switch (intr->intrinsic) {
   case nir_intrinsic_load_input:
      printf("emit_intrinsic: not implemented (load_input)\n");
      break;

   case nir_intrinsic_store_output:
      printf("emit_intrinsic: not implemented (store_output)\n");
      break;

   default:
      printf("emit_intrinsic: not implemented (%s)\n", info->name);
   }
}

static void
emit_load_const(struct tegra_fp_shader *fp, nir_load_const_instr *instr)
{
   printf("emit_load_const: not implemented\n");
   assert(instr->def.num_components == 1);
}

static void
emit_block(struct tegra_fp_shader *fp, struct nir_block *block)
{
   nir_foreach_instr(instr, block) {
      switch (instr->type) {
      case nir_instr_type_alu:
         emit_alu(fp, nir_instr_as_alu(instr));
         break;
      case nir_instr_type_intrinsic:
         emit_intrinsic(fp, nir_instr_as_intrinsic(instr));
         break;
      case nir_instr_type_load_const:
         emit_load_const(fp, nir_instr_as_load_const(instr));
         break;
      case nir_instr_type_ssa_undef:
         unreachable("nir_instr_type_ssa_undef not supported");
         break;
      case nir_instr_type_tex:
         unreachable("nir_instr_type_tex not supported");
         break;
      case nir_instr_type_phi:
         unreachable("nir_instr_type_phi not supported");
         break;
      case nir_instr_type_jump:
         unreachable("nir_instr_type_jump not supported");
         break;
      case nir_instr_type_call:
      case nir_instr_type_parallel_copy:
         unreachable("nir_instr_type_call / nir_instr_type_parallel_copy not supported");
         break;
      }
   }
}

static void
emit_cf_list(struct tegra_fp_shader *fp, struct exec_list *list)
{
   foreach_list_typed(nir_cf_node, node, node, list) {
      switch (node->type) {
      case nir_cf_node_block:
         emit_block(fp, nir_cf_node_as_block(node));
         break;

      case nir_cf_node_if:
         unreachable("nir_cf_node_if not supported");
         break;

      case nir_cf_node_loop:
         unreachable("nir_cf_node_loop not supported");
         break;

      case nir_cf_node_function:
         unreachable("nir_cf_node_function not supported");
         break;
      }
   }
}

static void
optimize_nir(struct nir_shader *s)
{
   bool progress;
   do {
      progress = false;
      NIR_PASS_V(s, nir_lower_vars_to_ssa);
      NIR_PASS(progress, s, nir_lower_alu_to_scalar);
      NIR_PASS(progress, s, nir_lower_phis_to_scalar);
      NIR_PASS(progress, s, nir_copy_prop);
      NIR_PASS(progress, s, nir_opt_remove_phis);
      NIR_PASS(progress, s, nir_opt_dce);
      NIR_PASS(progress, s, nir_opt_dead_cf);
      NIR_PASS(progress, s, nir_opt_cse);
      NIR_PASS(progress, s, nir_opt_peephole_select, 8);
      NIR_PASS(progress, s, nir_opt_algebraic);
      NIR_PASS(progress, s, nir_opt_constant_folding);
      NIR_PASS(progress, s, nir_opt_undef);
      NIR_PASS(progress, s, nir_opt_loop_unroll,
         nir_var_shader_in |
         nir_var_shader_out |
         nir_var_local);
   } while (progress);
}

static void
tegra_nir_to_fp(struct tegra_fp_shader *fp, nir_shader *s)
{
   NIR_PASS_V(s, nir_opt_global_to_local);
   NIR_PASS_V(s, nir_lower_regs_to_ssa);
   NIR_PASS_V(s, nir_lower_load_const_to_scalar);

   optimize_nir(s);

   NIR_PASS_V(s, nir_lower_locals_to_regs);
   NIR_PASS_V(s, nir_convert_from_ssa, true);
   NIR_PASS_V(s, nir_remove_dead_variables, nir_var_local);
   NIR_PASS_V(s, nir_move_vec_src_uses_to_dest);
   NIR_PASS_V(s, nir_lower_vec_to_movs);

   nir_print_shader(s, stdout);

   nir_function_impl *entry = nir_shader_get_entrypoint(s);
   emit_cf_list(fp, &entry->body);
}

static const nir_shader_compiler_options options = {
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_scmp = true,
   .lower_flrp32 = true,
   .lower_flrp64 = true,
   .lower_ffract = true,
   .lower_fmod32 = true,
   .lower_fmod64 = true,
   .lower_fdiv = true,
   .fuse_ffma = true,
};

static struct nir_shader *
fp_tgsi_to_nir(const struct tgsi_token *tokens)
{
   return tgsi_to_nir(tokens, &options);
}

void
tegra_tgsi_to_fp(struct tegra_fp_shader *fp, const struct tgsi_token *tokens)
{
   list_inithead(&fp->fp_instructions);
   list_inithead(&fp->alu_instructions);
   list_inithead(&fp->mfu_instructions);

   fp->info.num_inputs = 0;
   fp->info.color_input = -1;
   fp->info.max_tram_row = 1;

#if 1
   struct nir_shader *s = fp_tgsi_to_nir(tokens);
   tegra_nir_to_fp(fp, s);
#else
   struct tgsi_parse_context parser;
   int ret = tgsi_parse_init(&parser, tokens);
   assert(ret == TGSI_PARSE_OK);

   while (!tgsi_parse_end_of_tokens(&parser)) {
      tgsi_parse_token(&parser);
      switch (parser.FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_DECLARATION:
         emit_tgsi_declaration(fp, &parser.FullToken.FullDeclaration);
         break;

      case TGSI_TOKEN_TYPE_INSTRUCTION:
         if (parser.FullToken.FullInstruction.Instruction.Opcode != TGSI_OPCODE_END)
            emit_tgsi_instr(fp, &parser.FullToken.FullInstruction);
         break;
      }
   }
#endif
   /*
    * HACK: insert barycentric interpolation setup
    * This will overwrite instructions in some cases, need proper scheduler
    * to fix properly
    */
    struct fp_mfu_instr *first = list_first_entry(&fp->mfu_instructions, struct fp_mfu_instr, link);
    first->sfu.op = FP_SFU_OP_RCP;
    first->sfu.reg = 4;
    first->mul[0].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
    first->mul[0].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
    first->mul[0].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_0;

    first->mul[1].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
    first->mul[1].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
    first->mul[1].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_1;
}
