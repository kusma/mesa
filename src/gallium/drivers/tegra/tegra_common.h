#ifndef TEGRA_COMMON_H
#define TEGRA_COMMON_H

#define unimplemented() printf("TODO: %s()\n", __func__)

#define TGR3D_VAL(reg_name, field_name, value) \
   (((value) << TGR3D_ ## reg_name ## _ ## field_name ## __SHIFT) & \
           TGR3D_ ## reg_name ## _ ## field_name ## __MASK)

#define TGR3D_BOOL(reg_name, field_name, boolean) \
   ((boolean) ? TGR3D_ ## reg_name ## _ ## field_name : 0)

#endif
