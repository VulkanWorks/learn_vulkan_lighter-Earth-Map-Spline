//
//  char_lib.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/char_lib.h"

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace common {

CharLib::CharLib(const std::vector<std::string>& texts,
                 const std::string& font_path, int font_height) {
  FT_Library lib;
  FT_Face face;

  if (FT_Init_FreeType(&lib)) {
    FATAL("Failed to init FreeType library");
  }

  if (FT_New_Face(lib, font_path.c_str(), 0, &face)) {
    FATAL("Failed to load font");
  }
  FT_Set_Pixel_Sizes(face, 0, font_height);

  for (const auto& text : texts) {
    for (auto c : text) {
      if (char_info_map_.find(c) != char_info_map_.end()) {
        continue;
      }

      if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        FATAL("Failed to load glyph");
      }

      char_info_map_.emplace(c, CharInfo{
          /*bearing=*/{
              face->glyph->bitmap_left,
              face->glyph->bitmap_top,
          },
          // Advance is measured in number of 1/64 pixels.
          /*advance_x=*/static_cast<unsigned int>(face->glyph->advance.x) >> 6,
          /*image=*/absl::make_unique<Image>(
              /*width=*/face->glyph->bitmap.width,
              /*height=*/face->glyph->bitmap.rows,
              /*channel=*/1,
              /*raw_data=*/face->glyph->bitmap.buffer,
              /*flip_y=*/true
          ),
      });
    }
  }

  FT_Done_Face(face);
  FT_Done_FreeType(lib);
}

} /* namespace common */
} /* namespace jessie_steamer */
