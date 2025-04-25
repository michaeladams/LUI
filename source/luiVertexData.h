// Filename: luiVertexData.h
// Created by:  tobspr (01Sep14)
//

#ifndef LUI_VERTEX_DATA_H
#define LUI_VERTEX_DATA_H

#include "pandabase.h"
#include "pandasymbols.h"
#include "luse.h"

#include <stdint.h>

struct alignas(16) LUIVertexData {
  float x, y, z;                           // 12 bytes
  float u, v;                              // 8 bytes
  float atlas_u1, atlas_v1;                // 8 bytes (aligned to 4 bytes)
  float subregion_u_width, subregion_v_height;  // 8 bytes (aligned to 4 bytes)
  unsigned char color[4];                  // 4 bytes
  uint16_t texindex;                       // 2 bytes
  int32_t wrap_flag;                       // 4 byte
  float clip_x1, clip_y1, clip_x2, clip_y2; // 16 bytes
  unsigned char padding[6];                // 6 bytes
};

#endif
