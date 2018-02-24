#include "tegra_compiler.h"
#include "tegra_common.h"
#include "fp_ir.h"

#include "tgsi/tgsi_parse.h"

#include "util/u_memory.h"
#include "util/register_allocate.h"

#include "nir/tgsi_to_nir.h"

static struct fpir_node *
create_fpir_node(struct tegra_fp_shader *fp, enum fpir_node_type type)
{
   struct fpir_node *node = CALLOC_STRUCT(fpir_node);
   list_inithead(&node->link);

   node->type = type;

   list_addtail(&node->link, &fp->nodes);
   return node;
}

static void
init_ssa_def(struct tegra_fp_shader *fp, nir_ssa_def *def, struct fpir_node *node)
{
   assert(def->num_components == 1);
   _mesa_hash_table_insert(fp->defs, def, node);
}

static struct fpir_node *
get_src(struct tegra_fp_shader *fp, nir_src *src)
{
   assert(src->is_ssa);
   assert(src->ssa->num_components == 1);

   struct hash_entry *entry = _mesa_hash_table_search(fp->defs, src->ssa);
   assert(entry != NULL);
   return entry->data;
}

static void
emit_mov(struct tegra_fp_shader *fp, nir_alu_instr *alu)
{
   assert(nir_op_infos[alu->op].num_inputs == 1);
   struct fpir_node *src = get_src(fp, &alu->src[0].src);

   assert(!alu->dest.dest.is_ssa);
   nir_register *dest = alu->dest.dest.reg.reg;
   printf("move\n");
   for (int i = 0; i < dest->num_components; ++i) {
      if ((alu->dest.write_mask >> i) & 1)
         printf("comp %d\n", i);
   }
}

static void
emit_alu(struct tegra_fp_shader *fp, nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];

   switch (alu->op) {
   case nir_op_fmov:
   case nir_op_imov:
      emit_mov(fp, alu);
      break;

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
emit_load_input(struct tegra_fp_shader *fp, nir_intrinsic_instr *intr)
{
   /* we can't dynamically index varyings */
   nir_const_value *const_offset = nir_src_as_const_value(intr->src[0]);
   assert(const_offset != NULL);

   assert(1 == intr->num_components); // should be scalar by now

   uint32_t offset = nir_intrinsic_base(intr) + const_offset->u32[0];

   struct fpir_node *node = create_fpir_node(fp, FPIR_VAR_NODE);
   node->var.index = offset * 4 + nir_intrinsic_component(intr);

   printf("load comp: %d\n", node->var.index);

   list_addtail(&node->link, &fp->nodes);

   assert(intr->dest.is_ssa);
   init_ssa_def(fp, &intr->dest.ssa, node);
}

static void
emit_store_output(struct tegra_fp_shader *fp, nir_intrinsic_instr *intr)
{
   assert(nir_intrinsic_infos[intr->intrinsic].num_srcs == 2);

   int idx = nir_intrinsic_base(intr);
   int comp = nir_intrinsic_component(intr);

   assert(!intr->src[0].is_ssa);

   // no support for dynamic indexing of outputs
   nir_const_value *const_offset = nir_src_as_const_value(intr->src[1]);
   assert(const_offset != NULL);
   idx += const_offset->u32[0];

/*
   src = get_src(ctx, &intr->src[0]);
   for (int i = 0; i < intr->num_components; i++) {
      unsigned n = idx * 4 + i + comp;
      ctx->ir->outputs[n] = src[i];
   }
*/
   printf("emit_store_output: idx: %d\n", idx);
}

static void
emit_intrinsic(struct tegra_fp_shader *fp, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_input:
      emit_load_input(fp, intr);
      break;

   case nir_intrinsic_store_output:
      emit_store_output(fp, intr);
      break;

   default:
      printf("emit_intrinsic: not implemented (%s)\n",
             nir_intrinsic_infos[intr->intrinsic].name);
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
         unreachable("nir_instr_type_call not supported");
         break;
      case nir_instr_type_parallel_copy:
         unreachable("nir_instr_type_parallel_copy not supported");
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
   list_inithead(&fp->nodes);
   fp->defs = _mesa_hash_table_create(NULL, _mesa_hash_pointer,
                                      _mesa_key_pointer_equal);

   NIR_PASS_V(s, nir_opt_global_to_local);
   NIR_PASS_V(s, nir_lower_regs_to_ssa);
   NIR_PASS_V(s, nir_lower_load_const_to_scalar);
   NIR_PASS_V(s, nir_lower_io_to_scalar, nir_var_shader_in);

   optimize_nir(s);

   NIR_PASS_V(s, nir_lower_locals_to_regs);
   NIR_PASS_V(s, nir_convert_from_ssa, true);
   NIR_PASS_V(s, nir_remove_dead_variables, nir_var_local);
   // NIR_PASS_V(s, nir_move_vec_src_uses_to_dest);
   NIR_PASS_V(s, nir_lower_vec_to_movs);

   nir_print_shader(s, stdout);

   nir_function_impl *entry = nir_shader_get_entrypoint(s);
   emit_cf_list(fp, &entry->body);

   struct ra_regs *regs = ra_alloc_reg_set(NULL, 32 + 16 + 2, true);
   unsigned int reg_class_any = ra_alloc_reg_class(regs);
   unsigned int reg_class_r0r1_r2r3 = ra_alloc_reg_class(regs);

   for (int reg = 0; reg < 32; ++reg)
      ra_class_add_reg(regs, reg_class_any, reg);

   for (int reg = 0; reg < 16; ++reg) {
      ra_class_add_reg(regs, reg_class_any, reg);
      ra_add_reg_conflict(regs, 0, 16);
      ra_add_reg_conflict(regs, 1, 16);
   }

   // "r16" is a lowp vec4 alias for r1 and r2
   ra_class_add_reg(regs, reg_class_r0r1_r2r3, 16);
   ra_add_reg_conflict(regs, 0, 16);
   ra_add_reg_conflict(regs, 1, 16);

   // "r17" is a lowp vec4 alias for r2 and r3
   ra_class_add_reg(regs, reg_class_r0r1_r2r3, 17);
   ra_add_reg_conflict(regs, 2, 17);
   ra_add_reg_conflict(regs, 3, 17);

   ra_set_finalize(regs, NULL);
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

   struct nir_shader *s = fp_tgsi_to_nir(tokens);
   tegra_nir_to_fp(fp, s);
}
