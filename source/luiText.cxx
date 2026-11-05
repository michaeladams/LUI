#include "luiText.h"
#include "pandaVersion.h"

TypeHandle LUIText::_type_handle;

LUIText::LUIText(PyObject* self, LUIObject* parent, const wstring& text,
                 const string& font_name, float font_size, float x, float y, bool wordwrap, bool visible)
  :
  LUIObject(self, parent, x, y, parent->get_parent_width(), parent->get_parent_width(), false, visible),
  _text(text),
  _font_size(font_size),
  _wordwrap(wordwrap),
  _glow_radius_px(0.0f),
  _glow_color(LColor(1, 1, 1, 1)),
  _glow_pass_only(false),
  _background_color(LColor(0, 0, 0, 0)),
  _background_ignore_parent_color(false),
  _text_ignore_parent_color(false),
  _background_sprite(nullptr) {
  set_font(font_name);
}

LUIText::~LUIText() {
}

void LUIText::ensure_background_sprite() {
  if (_background_sprite == nullptr) {
    _background_sprite = new LUISprite(nullptr, this, "fill", "skin", 0.0f, 0.0f, 1.0f, 1.0f, LColor(1));
    _background_sprite->set_snap_position(false);
    _background_sprite->set_z_offset(-1.0f);
  }
}

void LUIText::sync_background_sprite() {
  ensure_background_sprite();
  if (_background_sprite == nullptr) {
    return;
  }

  _background_sprite->set_ignore_parent_color(_background_ignore_parent_color);
  _background_sprite->set_color(_background_color);
  _background_sprite->set_pos(0.0f, 0.0f);
  _background_sprite->set_size(get_width(), get_height());

  if (_background_color.get_w() > 0.0f) {
    _background_sprite->show();
  } else {
    _background_sprite->hide();
  }
}

void LUIText::update_text() {
  int len = _text.size();

  nassertv(_font != nullptr);

  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Current text is '" << _text.c_str() << "'" << endl;
  }

  int glyph_count = 0;
  for (auto it = _children.begin(); it != _children.end(); ++it) {
    if (*it != _background_sprite) {
      ++glyph_count;
    }
  }

  // Remove all glyph sprites which are no longer required.
  while (glyph_count > len) {
    if (lui_cat.is_spam()) {
      lui_cat.spam() << "Removing sprite .. " << endl;
    }

    auto to_remove = _children.end();
    for (auto it = _children.begin(); it != _children.end(); ++it) {
      if (*it != _background_sprite) {
        to_remove = it;
        break;
      }
    }

    if (to_remove == _children.end()) {
      break;
    }

    remove_child(*to_remove);
    --glyph_count;
  }

  // Allocate as many glyph sprites as required.
  int to_allocate = len - glyph_count;
  for (int i = 0; i < to_allocate; ++i) {
    if (lui_cat.is_spam()) {
      lui_cat.spam() << "Allocating sprite .. " << endl;
    }

    PT(LUISprite) sprite = new LUISprite(this);
    sprite->set_snap_position(false);
  }

  // Pixels per unit, used to convert between coordinate spaces
  float ppu = _font_size;
  float line_height = _font->get_line_height();

  vector<int> line_breaks = get_line_breaks();

  // Unreference all current glyphs
  _glyphs.clear();

  // Iterate over glyph sprites only.
  int char_idx = 0;
  float current_x_pos = 0.0f;
  float current_y_pos = 0.0f;
  float max_x_pos = 0.0f;

  for (auto it = _children.begin(); it != _children.end(); ++it) {
    if (*it == _background_sprite) {
      continue;
    }

    LUIBaseElement* child = *it;
    LUISprite* sprite = DCAST(LUISprite, child);

    nassertv(sprite != nullptr);
    nassertv(char_idx < len);

    int char_code = (int)_text.at(char_idx);
    ++char_idx;

#if PANDA_MAJOR_VERSION > 1 || PANDA_MINOR_VERSION >= 10
    CPT(TextGlyph) const_glyph;
#else
    const TextGlyph* const_glyph;
#endif

    // Newline - handled for both wordwrap and non-wordwrap modes
    if (char_code == 10) {
      sprite->hide();
      if (current_x_pos > max_x_pos) max_x_pos = current_x_pos;
      current_x_pos = 0;
      current_y_pos += floor(line_height * ppu);
      continue;
    }

    if (_wordwrap) {
      if (find(line_breaks.begin(), line_breaks.end(), char_idx - 1) != line_breaks.end()) {
        current_x_pos = 0;
        current_y_pos += floor(line_height * ppu);
      }
    }

    if (!_font->get_glyph(char_code, const_glyph)) {
      sprite->set_texture(nullptr);
      lui_cat.error() << "Font does not support character with char code " << char_code
                      << ", ignoring .. target = " << _debug_name << endl;
      continue;
    }

    CPT(DynamicTextGlyph) dynamic_glyph = DCAST(DynamicTextGlyph, const_glyph);
    nassertv(dynamic_glyph != nullptr);

    _glyphs.push_back(dynamic_glyph);

    // Some characters have no texture (like space)
    if (dynamic_glyph->get_page() == nullptr) {
      lui_cat.debug() << "Character '" << (char)char_code << "' (Code: " << char_code
                      << ") has no texture page!" << endl;
      sprite->hide();

    } else {
      sprite->show();

      // LUISprite has a check if the texture is the same, so if the atlas didn't
      // change, this is quite efficient.
      sprite->set_texture(dynamic_glyph->get_page());

      if (_glow_pass_only && _glow_radius_px > 0.0f) {
        // Outer glow pass: expand the quad by 2x the kernel radius.
        float exp = _glow_radius_px * 2.0f;

        int tex_w = dynamic_glyph->get_page()->get_x_size();
        int tex_h = dynamic_glyph->get_page()->get_y_size();
        if (tex_w < 1) tex_w = 1;
        if (tex_h < 1) tex_h = 1;
        float uv_exp_u = exp / (float)tex_w;
        float uv_exp_v = exp / (float)tex_h;

        sprite->set_pos(
          current_x_pos + dynamic_glyph->get_left() * ppu - exp,
          (0.85f - dynamic_glyph->get_top()) * ppu + 1 + current_y_pos - exp);

        sprite->set_uv_range(
          dynamic_glyph->get_uv_left()       - uv_exp_u,
          1 - dynamic_glyph->get_uv_top()    - uv_exp_v,
          dynamic_glyph->get_uv_right()      + uv_exp_u,
          1 - dynamic_glyph->get_uv_bottom() + uv_exp_v);

        sprite->set_clamp_uv_range(
          dynamic_glyph->get_uv_left(),
          1 - dynamic_glyph->get_uv_top(),
          dynamic_glyph->get_uv_right(),
          1 - dynamic_glyph->get_uv_bottom());

        sprite->set_size(
          (dynamic_glyph->get_right() - dynamic_glyph->get_left()) * ppu + 2.0f * exp,
          (dynamic_glyph->get_top()   - dynamic_glyph->get_bottom()) * ppu + 2.0f * exp);

        // Negative radius signals the shader to render only the halo, not the base glyph.
        sprite->set_glow(-_glow_radius_px, _glow_color);
      } else {
        // Normal glyph layout.
        sprite->clear_clamp_uv_range();
        sprite->set_pos(
          current_x_pos + dynamic_glyph->get_left() * ppu,
          (0.85f - dynamic_glyph->get_top()) * ppu + 1 + current_y_pos);
        sprite->set_uv_range(
          dynamic_glyph->get_uv_left(),
          1 - dynamic_glyph->get_uv_top(),
          dynamic_glyph->get_uv_right(),
          1 - dynamic_glyph->get_uv_bottom());
        sprite->set_size(
          (dynamic_glyph->get_right() - dynamic_glyph->get_left()) * ppu,
          (dynamic_glyph->get_top()   - dynamic_glyph->get_bottom()) * ppu);
        sprite->set_glow(_glow_radius_px > 0.0f ? _glow_radius_px : 0.0f, _glow_color);
      }

      // Glyph tint is either inherited from parent-composed text color (default)
      // or fixed to local text color while ignoring parent tint.
      sprite->set_ignore_parent_color(_text_ignore_parent_color);
      sprite->set_color(_text_ignore_parent_color ? _color : LColor(1));
    }

    // Break word wrapping
    if (_wordwrap && current_x_pos + dynamic_glyph->get_advance() * ppu > get_parent_width()) {
      current_x_pos = 0;
      current_y_pos += floor(line_height * ppu);
    } else {
      // Trim left
      if (_wordwrap && current_x_pos == 0 && dynamic_glyph->get_page() == nullptr) {
        continue;
      }

      // Move cursor by glyph advance
      current_x_pos += dynamic_glyph->get_advance() * ppu;
      if (current_x_pos > max_x_pos) max_x_pos = current_x_pos;
    }
  }

  if (_wordwrap) {
    set_size(get_parent_width(), floor(current_y_pos + line_height * ppu));
  } else {
    set_size(floor(max_x_pos), floor(current_y_pos + line_height * ppu));
  }

  if (_background_sprite != nullptr) {
    sync_background_sprite();
  }
}

void LUIText::set_color(const LColor& color) {
  LUIColorable::set_color(color);

  // Keep already allocated glyph sprites in sync without requiring a re-layout.
  for (auto it = _children.begin(); it != _children.end(); ++it) {
    LUISprite* sprite = DCAST(LUISprite, *it);
    if (sprite == nullptr || sprite == _background_sprite) {
      continue;
    }

    sprite->set_ignore_parent_color(_text_ignore_parent_color);
    sprite->set_color(_text_ignore_parent_color ? _color : LColor(1));
  }
}

void LUIText::ls(int indent) {
  cout << string(indent, ' ')  << "[" << _debug_name << "] pos = " << get_abs_pos().get_x() << ", " << get_abs_pos().get_y()
       << "; size = " << get_width() << " x " << get_height() << "; text = u'" << _text << "'; color = "
       << _color << " / " << _composed_color << "; z = " << _z_offset << endl;
}

int LUIText::get_char_index(float pos) const {
  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Trying to resolve " << pos << " into a character index .." << endl;
  }
  nassertr(_font != nullptr, 0);

  float cursor = 0.0f;

  for (int i = 0; i < _text.size(); ++i) {
    int char_code = (int)_text.at(i);

#if PANDA_MAJOR_VERSION > 1 || PANDA_MINOR_VERSION >= 10
    CPT(TextGlyph) glyph;
#else
    const TextGlyph* glyph;
#endif
    if (!_font->get_glyph(char_code, glyph)) {
      lui_cat.error() << "Font does not support character with char code " << char_code << ", ignoring .. target = " << _debug_name << endl;
      continue;
    }

    nassertr(glyph != nullptr, 0);
    cursor += glyph->get_advance() * _font_size;

    if (cursor > pos) {
      return i;
    }
  }
  return _text.size();
}

float LUIText::get_char_pos(int char_index) const {
  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Trying to resolve " << char_index << " into a character position .." << endl;
  }
  nassertr(_font != nullptr, 0);

  // Make sure we don't iterate over the text bounds
  int iterate_max = min(char_index, (int)_text.size());

  float cursor = 0.0f;

  for (int i = 0; i < iterate_max; i++) {
    int char_code = (int)_text.at(i);

#if PANDA_MAJOR_VERSION > 1 || PANDA_MINOR_VERSION >= 10
    CPT(TextGlyph) glyph;
#else
    const TextGlyph* glyph;
#endif
    if (!_font->get_glyph(char_code, glyph)) {
      lui_cat.error() << "Font does not support character with char code " << char_code << ", ignoring .. target = " << _debug_name << endl;
      continue;
    }
    nassertr(glyph != nullptr, 0);
    cursor += glyph->get_advance() * _font_size;
  }

  return cursor;
}

// Returns a list of character indexes where linebreaks should occur when wrapping.
vector<int> LUIText::get_line_breaks() {

  vector<int> result;

  if (_wordwrap) {

    // Unreference all current glyphs
    _glyphs.clear();

    int char_idx = 0;
    int word_start = 0;
    float word_start_pos = 0.0f;
    float future_x_pos = 0.0f;
    float line_start = 0.0f;
    float ppu = _font_size;

    for (auto it = _children.begin(); it != _children.end(); ++it) {
      if (*it == _background_sprite) {
        continue;
      }

      LUIBaseElement* child = *it;
      LUISprite* sprite = DCAST(LUISprite, child);

      nassertr(sprite != nullptr, result);
      nassertr(char_idx < (int)_text.size(), result);

      int char_code = (int)_text.at(char_idx);
      int glyph_char_idx = char_idx;
      ++char_idx;

#if PANDA_MAJOR_VERSION > 1 || PANDA_MINOR_VERSION >= 10
      CPT(TextGlyph) const_glyph;
#else
      const TextGlyph* const_glyph;
#endif

      // Character is a newline, so lets include this in our list.
      if (char_code == 10) {
        result.push_back(glyph_char_idx);
        word_start = glyph_char_idx;
        word_start_pos = future_x_pos;
        line_start = future_x_pos;
        continue;
      }

      if (!_font->get_glyph(char_code, const_glyph)) {
        sprite->set_texture(nullptr);
        lui_cat.error() << "Font does not support character with char code " << char_code
                        << ", ignoring .. target = " << _debug_name << endl;
        continue;
      }

      CPT(DynamicTextGlyph) dynamic_glyph = DCAST(DynamicTextGlyph, const_glyph);
      nassertr(dynamic_glyph != nullptr, result);

      _glyphs.push_back(dynamic_glyph);

      // If a space, mark this as the start of the next word.
      if (dynamic_glyph->get_page() == nullptr) {
        word_start = glyph_char_idx;
        word_start_pos = future_x_pos + dynamic_glyph->get_advance() * ppu;
      }

      // If adding the glyph will force overflow, break at word start.
      if (((future_x_pos - line_start) + (dynamic_glyph->get_advance() * ppu)) > get_parent_width()) {
        result.push_back(word_start);
        line_start = word_start_pos;
      }

      // Move the cursor along by the glyph width.
      future_x_pos += dynamic_glyph->get_advance() * ppu;
    }
  }

  return result;
}
