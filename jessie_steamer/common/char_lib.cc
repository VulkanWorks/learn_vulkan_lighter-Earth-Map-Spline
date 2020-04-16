//
//  char_lib.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/char_lib.h"

#include "jessie_steamer/common/util.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace common {

CharLib::CharLib(absl::Span<const std::string> texts,
                 const std::string& font_path, int font_height) {
  FT_Library lib;
  FT_Face face;
  ASSERT_FALSE(FT_Init_FreeType(&lib), "Failed to init FreeType library");
  ASSERT_FALSE(FT_New_Face(lib, font_path.c_str(), /*face_index=*/0, &face),
               "Failed to load font");
  FT_Set_Pixel_Sizes(face, /*pixel_width=*/0, font_height);

  for (const auto& text : texts) {
    for (auto character : text) {
      if (char_info_map_.contains(character)) {
        continue;
      }

      ASSERT_FALSE(FT_Load_Char(face, character, FT_LOAD_RENDER),
                   "Failed to load glyph");
      char_info_map_.emplace(
          character, CharInfo{
              /*bearing=*/{
                  face->glyph->bitmap_left,
                  face->glyph->bitmap_top,
              },
              // Advance is measured in number of 1/64 pixels.
              /*advance=*/{
                  static_cast<unsigned int>(face->glyph->advance.x) >> 6U,
                  static_cast<unsigned int>(face->glyph->advance.y) >> 6U,
              },
              /*image=*/absl::make_unique<Image>(
                  /*width=*/face->glyph->bitmap.width,
                  /*height=*/face->glyph->bitmap.rows,
                  common::kBwImageChannel,
                  /*raw_data=*/face->glyph->bitmap.buffer,
                  /*flip_y=*/true
              ),
          }
      );
    }
  }

  FT_Done_Face(face);
  FT_Done_FreeType(lib);
}

} /* namespace common */
} /* namespace jessie_steamer */
