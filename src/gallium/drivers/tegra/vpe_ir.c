#include <assert.h>
#include <stdint.h>

#include "util/macros.h"

#include "vpe_ir.h"

void
tegra_vpe_pack(uint32_t *dst, struct vpe_instr instr, bool end_of_program)
{
   /* we can only handle one output register per instruction */
   assert(instr.vec.dst.file != VPE_DST_FILE_OUTPUT ||
          instr.scalar.dst.file != VPE_DST_FILE_OUTPUT);

   union {
      struct __attribute__((packed)) {
         unsigned end_of_program : 1;                       //   0
         unsigned constant_relative_addressing_enable : 1;  //   1
         unsigned export_write_index : 5;                   //   2 .. 6
         unsigned scalar_rD_index : 6;                      //   7 .. 12
         unsigned vector_op_write_w_enable : 1;             //  13
         unsigned vector_op_write_z_enable : 1;             //  14
         unsigned vector_op_write_y_enable : 1;             //  15
         unsigned vector_op_write_x_enable : 1;             //  16
         unsigned scalar_op_write_w_enable : 1;             //  17
         unsigned scalar_op_write_z_enable : 1;             //  18
         unsigned scalar_op_write_y_enable : 1;             //  19
         unsigned scalar_op_write_x_enable : 1;             //  20
         unsigned rC_type : 2;                              //  21 .. 22
         unsigned rC_index : 6;                             //  23 .. 28
         unsigned rC_swizzle_w : 2;                         //  29 .. 30
         unsigned rC_swizzle_z : 2;                         //  31 .. 32
         unsigned rC_swizzle_y : 2;                         //  33 .. 34
         unsigned rC_swizzle_x : 2;                         //  35 .. 36
         unsigned rC_negate : 1;                            //  37
         unsigned rB_type : 2;                              //  38 .. 39
         unsigned rB_index : 6;                             //  40 .. 45
         unsigned rB_swizzle_w : 2;                         //  46 .. 47
         unsigned rB_swizzle_z : 2;                         //  48 .. 49
         unsigned rB_swizzle_y : 2;                         //  50 .. 51
         unsigned rB_swizzle_x : 2;                         //  52 .. 53
         unsigned rB_negate : 1;                            //  54
         unsigned rA_type : 2;                              //  55 .. 56
         unsigned rA_index : 6;                             //  57 .. 62
         unsigned rA_swizzle_w : 2;                         //  63 .. 64
         unsigned rA_swizzle_z : 2;                         //  65 .. 66
         unsigned rA_swizzle_y : 2;                         //  67 .. 68
         unsigned rA_swizzle_x : 2;                         //  69 .. 70
         unsigned rA_negate : 1;                            //  71
         unsigned attribute_fetch_index : 4;                //  72 .. 75
         unsigned uniform_fetch_index : 10;                 //  76 .. 85
         unsigned vector_opcode : 5;                        //  86 .. 90
         unsigned scalar_opcode : 5;                        //  91 .. 95
         unsigned address_register_select : 2;              //  96 .. 97
         unsigned predicate_swizzle_w : 2;                  //  98 .. 99
         unsigned predicate_swizzle_z : 2;                  // 100 .. 101
         unsigned predicate_swizzle_y : 2;                  // 102 .. 103
         unsigned predicate_swizzle_x : 2;                  // 104 .. 105
         unsigned predicate_lt : 1;                         // 106
         unsigned predicate_eq : 1;                         // 107
         unsigned predicate_gt : 1;                         // 108
         unsigned condition_check : 1;                      // 109
         unsigned condition_set : 1;                        // 110
         unsigned vector_rD_index : 6;                      // 111 .. 116
         unsigned rA_absolute : 1;                          // 117
         unsigned rB_absolute : 1;                          // 118
         unsigned rC_absolute : 1;                          // 119
         unsigned bit120 : 1;                               // 120
         unsigned condition_register_index : 1;             // 121
         unsigned saturate_result : 1;                      // 122
         unsigned attribute_relative_addressing_enable : 1; // 123
         unsigned export_relative_addressing_enable : 1;    // 124
         unsigned condition_flags_write_enable : 1;         // 125
         unsigned export_vector_write_enable : 1;           // 126
         unsigned bit127 : 1;                               // 127
      };

      uint32_t words[4];
   } tmp = {
      .predicate_lt = 1,
      .predicate_eq = 1,
      .predicate_gt = 1,

      .predicate_swizzle_x = VPE_SWZ_X,
      .predicate_swizzle_y = VPE_SWZ_Y,
      .predicate_swizzle_z = VPE_SWZ_Z,
      .predicate_swizzle_w = VPE_SWZ_W,
   };

   /* find the attribute/uniform fetch-values, and zero out the index
    * for these registers.
    */
   int attr_fetch = -1, uniform_fetch = -1;
   for (int i = 0; i < 3; ++i) {
      switch (instr.vec.src[i].file) {
      case VPE_SRC_FILE_ATTRIB:
         assert(attr_fetch < 0 ||
                attr_fetch == instr.vec.src[i].index);
         attr_fetch = instr.vec.src[i].index;
         instr.vec.src[i].index = 0;
         break;

      case VPE_SRC_FILE_UNIFORM:
         assert(uniform_fetch < 0 ||
                uniform_fetch == instr.vec.src[i].index);
         uniform_fetch = instr.vec.src[i].index;
         instr.vec.src[i].index = 0;
         break;

      default: /* nothing */
         break;
      }
   }

   switch (instr.scalar.src.file) {
   case VPE_SRC_FILE_ATTRIB:
      assert(attr_fetch < 0 ||
             attr_fetch == instr.scalar.src.index);
      attr_fetch = instr.scalar.src.index;
      instr.scalar.src.index = 0;
      break;

      case VPE_SRC_FILE_UNIFORM:
         assert(uniform_fetch < 0 ||
                uniform_fetch == instr.scalar.src.index);
         uniform_fetch = instr.scalar.src.index;
         instr.scalar.src.index = 0;
         break;

      default: /* nothing */
         break;

   }

   tmp.attribute_fetch_index = attr_fetch >= 0 ? attr_fetch : 0;
   tmp.uniform_fetch_index = uniform_fetch >= 0 ? uniform_fetch : 0;

   tmp.vector_opcode = instr.vec.op;
   switch (instr.vec.dst.file) {
   case VPE_DST_FILE_TEMP:
      tmp.vector_rD_index = instr.vec.dst.index;
      tmp.export_write_index = 31;
      break;

   case VPE_DST_FILE_OUTPUT:
      tmp.vector_rD_index = 63; // disable register-write
      tmp.export_vector_write_enable = 1;
      tmp.export_write_index = instr.vec.dst.index;
      break;

   case VPE_DST_FILE_UNDEF:
      // assert(0);  // TODO: consult NOP
      break;

   default:
      unreachable("illegal enum vpe_dst_file value");
   }
   tmp.vector_op_write_x_enable = (instr.vec.dst.write_mask >> 0) & 1;
   tmp.vector_op_write_y_enable = (instr.vec.dst.write_mask >> 1) & 1;
   tmp.vector_op_write_z_enable = (instr.vec.dst.write_mask >> 2) & 1;
   tmp.vector_op_write_w_enable = (instr.vec.dst.write_mask >> 3) & 1;

   tmp.scalar_opcode = instr.scalar.op;
   switch (instr.scalar.dst.file) {
   case VPE_DST_FILE_TEMP:
      tmp.scalar_rD_index = instr.scalar.dst.index;
      tmp.export_write_index = 31;
      break;

   case VPE_DST_FILE_OUTPUT:
      tmp.scalar_rD_index = 63; // disable register-write
      tmp.export_vector_write_enable = 0;
      tmp.export_write_index = instr.scalar.dst.index;
      break;

   case VPE_DST_FILE_UNDEF:
      // assert(0);  // TODO: consult NOP
      break;

   default:
      unreachable("illegal enum vpe_dst_file value");
   }
   tmp.scalar_op_write_x_enable = (instr.scalar.dst.write_mask >> 0) & 1;
   tmp.scalar_op_write_y_enable = (instr.scalar.dst.write_mask >> 1) & 1;
   tmp.scalar_op_write_z_enable = (instr.scalar.dst.write_mask >> 2) & 1;
   tmp.scalar_op_write_w_enable = (instr.scalar.dst.write_mask >> 3) & 1;

   tmp.rA_type = instr.vec.src[0].file;
   tmp.rA_index = instr.vec.src[0].index;
   tmp.rA_swizzle_x = instr.vec.src[0].swizzle[0];
   tmp.rA_swizzle_y = instr.vec.src[0].swizzle[1];
   tmp.rA_swizzle_z = instr.vec.src[0].swizzle[2];
   tmp.rA_swizzle_w = instr.vec.src[0].swizzle[3];
   tmp.rA_negate = instr.vec.src[0].negate;
   tmp.rA_absolute = instr.vec.src[0].absolute;

   tmp.rB_type = instr.vec.src[1].file;
   tmp.rB_index = instr.vec.src[1].index;
   tmp.rB_swizzle_x = instr.vec.src[1].swizzle[0];
   tmp.rB_swizzle_y = instr.vec.src[1].swizzle[1];
   tmp.rB_swizzle_z = instr.vec.src[1].swizzle[2];
   tmp.rB_swizzle_w = instr.vec.src[1].swizzle[3];
   tmp.rB_negate = instr.vec.src[1].negate;
   tmp.rB_absolute = instr.vec.src[1].absolute;

   if (instr.vec.src[2].file != VPE_SRC_FILE_UNDEF) {
      tmp.rC_type = instr.vec.src[2].file;
      tmp.rC_index = instr.vec.src[2].index;
      tmp.rC_swizzle_x = instr.vec.src[2].swizzle[0];
      tmp.rC_swizzle_y = instr.vec.src[2].swizzle[1];
      tmp.rC_swizzle_z = instr.vec.src[2].swizzle[2];
      tmp.rC_swizzle_w = instr.vec.src[2].swizzle[3];
      tmp.rC_negate = instr.vec.src[2].negate;
      tmp.rC_absolute = instr.vec.src[2].absolute;
   } else if (instr.scalar.src.file != VPE_SRC_FILE_UNDEF) {
      tmp.rC_type = instr.scalar.src.file;
      tmp.rC_index = instr.scalar.src.index;
      tmp.rC_swizzle_x = instr.scalar.src.swizzle[0];
      tmp.rC_swizzle_y = instr.scalar.src.swizzle[1];
      tmp.rC_swizzle_z = instr.scalar.src.swizzle[2];
      tmp.rC_swizzle_w = instr.scalar.src.swizzle[3];
      tmp.rC_negate = instr.scalar.src.negate;
      tmp.rC_absolute = instr.scalar.src.absolute;
   }

   tmp.end_of_program = end_of_program;

   /* copy packed instruction into destination */
   for (int i = 0; i < 4; ++i)
      dst[i] = tmp.words[3 - i];
}
