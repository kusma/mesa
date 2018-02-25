#ifndef TEGRA_COMPILER_H
#define TEGRA_COMPILER_H

#include "util/list.h"
#include "fp_ir.h"

#include <stdint.h>

struct tgsi_token;

struct tegra_vpe_shader {
   struct list_head instructions;
   uint16_t output_mask;
};

enum fpir_node_type {
   FPIR_VAR_NODE,
   FPIR_ALU_NODE,
   FPIR_IMM_NODE
};

struct fpir_node {
   struct list_head link;
   enum fpir_node_type type;
   union {
      struct {
         int index;
      } var;
      struct {
         enum fp_alu_op op;
         struct fpir_node *src[4];
      } alu;
      struct {
         float value;
      } imm;
   };
};

struct tegra_fp_info {
   struct {
      uint32_t src;
      uint32_t dst;
   } inputs[16];
   int num_inputs;
   int color_input;
   int max_tram_row;
};

struct tegra_fp_shader {
   struct tegra_fp_info info;

   /* before scheduling */
   struct list_head nodes;
   struct hash_table *defs;

   /* after scheduling */
   struct list_head fp_instructions;
   struct list_head alu_instructions;
   struct list_head mfu_instructions;
};

void
tegra_tgsi_to_vpe(struct tegra_vpe_shader *vpe, const struct tgsi_token *tokens);

void
tegra_tgsi_to_fp(struct tegra_fp_shader *fp, const struct tgsi_token *tokens);

#endif
