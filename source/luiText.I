
INLINE void LUIText::set_font(const string& font_name) {
    _font = LUIFontPool::get_global_ptr()->get_font(font_name);
    update_text();
}

INLINE void LUIText::set_text(const wstring& text) {
    if (_text != text) {
        _text = text;
        update_text();
    }
}

INLINE void LUIText::set_font_size(float size) {
  if (size != _font_size) {
    _font_size = size;
    update_text();
  }
}

INLINE DynamicTextFont* LUIText::get_font() const{
    return _font;
}

INLINE const wstring& LUIText::get_text() const {
    return _text;
}

INLINE float LUIText::get_font_size() const {
    return _font_size;
}

INLINE void LUIText::set_wordwrap(bool wrap) {
  _wordwrap = wrap;
  update_text();
}

INLINE bool LUIText::get_wordwrap() const {
  return _wordwrap;
}

INLINE void LUIText::set_glow(float radius, const LColor& color) {
  _glow_radius_px = radius;
  _glow_color = color;
  update_text();
}

INLINE void LUIText::set_glow_radius(float radius) {
  _glow_radius_px = radius;
  update_text();
}

INLINE void LUIText::set_glow_color(const LColor& color) {
  _glow_color = color;
  update_text();
}

INLINE float LUIText::get_glow_radius() const {
  return _glow_radius_px;
}

INLINE const LColor& LUIText::get_glow_color() const {
  return _glow_color;
}

INLINE void LUIText::set_glow_pass_only(bool pass_only) {
  if (_glow_pass_only != pass_only) {
    _glow_pass_only = pass_only;
    update_text();
  }
}

INLINE bool LUIText::get_glow_pass_only() const {
  return _glow_pass_only;
}
