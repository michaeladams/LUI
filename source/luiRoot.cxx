#include "luiRoot.h"
#include "shader.h"

bool LUIRoot::_use_glsl_130 = false;


LUIRoot::LUIRoot(float width, float height) : 
  _requested_focus(nullptr),
  _explicit_blur(false),
  _sprites_rendered(0),
  _frame_count(0),
  _render_index(0),
  _sprite_vertex_pointer(nullptr),
  _index_buffer_size(1000000) {

  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Constructing new LUIRoot ..\n";
  }
  _root = new LUIObject(nullptr, 0.0f, 0.0f, width, height);
  _root->set_root(this);

  // Create vertex chunks
  // CPT(GeomVertexFormat) format = GeomVertexFormat::get_v3c4t2();

  PT(GeomVertexArrayFormat) array_format = new GeomVertexArrayFormat();
  array_format->add_column(InternalName::make("vertex"), 3, Geom::NT_float32, Geom::C_point);
  array_format->add_column(InternalName::make("texcoord"), 2, Geom::NT_float32, Geom::C_texcoord);
  array_format->add_column(InternalName::make("atlasUV"), 2, Geom::NT_float32, Geom::C_other);
  array_format->add_column(InternalName::make("subregionSize"), 2, Geom::NT_float32, Geom::C_other); // subregion_u_width, subregion_v_height
  array_format->add_column(InternalName::make("color"), 4, Geom::NT_uint8, Geom::C_color);
  array_format->add_column(InternalName::make("texindex"), 1, Geom::NT_uint16, Geom::C_other);
  array_format->add_column(InternalName::make("wrap_flag"), 1, Geom::NT_int32, Geom::C_other);
  array_format->add_column(InternalName::make("clipBounds"), 4, Geom::NT_float32, Geom::C_other);
  array_format->set_stride(80);

  PT(GeomVertexFormat) unregistered_format = new GeomVertexFormat();
  unregistered_format->add_array(array_format);

  CPT(GeomVertexFormat) format = GeomVertexFormat::register_format(unregistered_format);

  _vertex_data = new GeomVertexData("VertexPool", format, Geom::UH_dynamic);
  _vertex_data->reserve_num_rows(_index_buffer_size);

  _triangles = new GeomTriangles(Geom::UH_dynamic);
  _triangles->set_index_type(GeomEnums::NT_uint16);
  _triangles->make_indexed();

  _geom = new Geom(_vertex_data);
  _geom->add_primitive(_triangles);
  _geom->set_bounds(new OmniBoundingVolume());

  _triangle_index_buffer = new LUITriangleIndex[_index_buffer_size];
}

void LUIRoot::prepare_render() {

  if (lui_cat.is_spam()) {
    lui_cat.spam() << "\nPreparing sprite render" << endl;
  }

  PT(GeomVertexArrayDataHandle) array_data_handle = _vertex_data->modify_array(0)->modify_handle();
  _sprite_vertex_pointer = array_data_handle->get_write_pointer();

  _min_rendered_vertex = 999999999;
  _max_rendered_vertex = 0;
  _render_index = 0;
  _frame_count += 1;

  // prepare the geom triangle
  _sprites_rendered = 0;

  _triangles->clear_vertices();
  _triangles->make_indexed();
  _triangles->set_index_type(GeomEnums::NT_uint16);

  // Update lui elements graph
  _root->update_downstream();
  _root->update_dimensions_upstream();
  // for (int i = 0; i < 20; ++i)
    _root->update_upstream();
  _root->update_clip_bounds();


  // Render normal elements
  _root->render_recursive(false, false);

  // Render topmost elements
  int sprites_before = _sprites_rendered;
  _root->render_recursive(true, false);
  int sprites_topmost = _sprites_rendered - sprites_before;

  if (_sprites_rendered > 0) {
    _triangles->modify_vertices()->unclean_set_num_rows(_sprites_rendered * 2 * 3);

    memcpy(_triangles->modify_vertices()->modify_handle()->get_write_pointer(), _triangle_index_buffer, _sprites_rendered * sizeof(LUITriangleIndex) * 2);

    nassertv(_min_rendered_vertex < _max_rendered_vertex);
    _triangles->set_minmax(_min_rendered_vertex, _max_rendered_vertex, nullptr, nullptr);
  }

  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Rendered " << _triangles->get_num_vertices() << " vertices (" << _sprites_rendered << " Sprites)" << endl;
    lui_cat.spam() << sprites_topmost << " sprites rendered topmost vs " << sprites_before << " normal" << endl;
  }
}

LUIRoot::~LUIRoot() {
  if (lui_cat.is_spam()) {
    lui_cat.spam() << "Destructed LUIRoot\n";
  }

  // Remove all children first
  _root->remove_all_children();
  _sprites.clear();
  _textures.clear();
}


PT(Shader) LUIRoot::create_object_shader() {
  if (_use_glsl_130) {
    return Shader::make(Shader::SL_GLSL,
      // Vertex
      "#version 130\n"
      "uniform mat4 p3d_ModelViewProjectionMatrix;\n"
      "in vec4 p3d_Vertex;\n"
      "in uint texindex;\n"
      "in vec4 color;\n"
      "in vec2 p3d_MultiTexCoord0;\n"
      "in int wrap_flag;\n"
      "in vec2 atlasUV;\n"
      "in vec2 subregionSize;\n"
      "in vec4 clipBounds;\n"
      "out vec2 texcoord;\n"
      "flat out uint vtx_texindex;\n"
      "out vec4 color_scale;\n"
      "flat out int wrap_flag_fs;\n"
      "out vec2 fragAtlasUV;\n"
      "out vec2 fragSubregionSize;\n"
      "out vec4 fragClipBounds;\n"
      "void main() {\n"
      "  texcoord = p3d_MultiTexCoord0;\n"
      "  color_scale = color;\n"
      "  vtx_texindex = texindex;\n"
      "  gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;\n"
      "  wrap_flag_fs = wrap_flag;\n"
      "  fragAtlasUV = atlasUV;\n"
      "  fragClipBounds = clipBounds;\n"
      "  fragSubregionSize = subregionSize;\n"
      "}\n"
      ,
      // Fragment
      "#version 130\n"
      "in vec2 texcoord;\n"
      "flat in uint vtx_texindex;\n"
      "flat in int wrap_flag_fs;\n"
      "in vec4 color_scale;\n"
      "in vec2 fragAtlasUV;\n"
      "in vec2 fragSubregionSize;\n"
      "in vec4 fragClipBounds;\n"
      "uniform sampler2D lui_texture_0;\n"
      "uniform sampler2D lui_texture_1;\n"
      "uniform sampler2D lui_texture_2;\n"
      "uniform sampler2D lui_texture_3;\n"
      "uniform sampler2D lui_texture_4;\n"
      "uniform sampler2D lui_texture_5;\n"
      "uniform sampler2D lui_texture_6;\n"
      "uniform sampler2D lui_texture_7;\n"
      "uniform float screen_width;\n"
      "uniform float screen_height;\n"
      "out vec4 color;\n"
      "void main() {\n"
      "  vec2 fragPos = gl_FragCoord.xy;\n"
      "  vec4 skipColor = vec4(1, 1, 1, 1);\n"
      "  // Check if the fragment is outside the clipping bounds\n"
      "  if ((fragPos.x < fragClipBounds.x || fragPos.x > fragClipBounds.z ||\n"
      "      screen_height - fragPos.y < fragClipBounds.y || screen_height - fragPos.y > fragClipBounds.w)) {\n"
      "    skipColor = vec4(0,0,0,0); // Skip rendering by setting transparent.\n"
      "  }\n"
      "  vec2 mod_uv = vec2(mod(texcoord, fragSubregionSize)) + (fragAtlasUV);\n"
      "  vec2 final_uv = (wrap_flag_fs == 1) ? mod_uv : texcoord;\n"
      "  vec4 texcolor = vec4(0, 0, 0, 1);\n"
      "  switch (vtx_texindex) {\n"
      "    case 0u: texcolor = texture(lui_texture_0, final_uv); break;\n"
      "    case 1u: texcolor = texture(lui_texture_1, final_uv); break;\n"
      "    case 2u: texcolor = texture(lui_texture_2, final_uv); break;\n"
      "    case 3u: texcolor = texture(lui_texture_3, final_uv); break;\n"
      "    case 4u: texcolor = texture(lui_texture_4, final_uv); break;\n"
      "    case 5u: texcolor = texture(lui_texture_5, final_uv); break;\n"
      "    case 6u: texcolor = texture(lui_texture_6, final_uv); break;\n"
      "    case 7u: texcolor = texture(lui_texture_7, final_uv); break;\n"
      "  }\n"
      "  color = texcolor * color_scale * skipColor;\n"
      "}\n"
      );
  } else {
    return Shader::make(Shader::SL_GLSL,
      // Vertex
      "#version 100\n"
      "uniform mat4 p3d_ModelViewProjectionMatrix;\n"
      "attribute vec4 p3d_Vertex;\n"
      "attribute float texindex;\n"
      "attribute vec4 color;\n"
      "attribute vec4 clipBounds;\n"
      "attribute uint wrap_flag;\n"
      "attribute vec2 atlasUV;\n"
      "attribute vec2 subregionSize;\n"
      "attribute vec2 p3d_MultiTexCoord0;\n"
      "varying vec2 texcoord;\n"
      "varying float vtx_texindex;\n"
      "varying vec4 color_scale;\n"
      "varying vec4 fragClipBounds;\n"
      "flat varying uint wrap_flag_fs;\n"
      "varying vec2 fragAtlasUV;\n"
      "varying vec2 fragSubregionSize;\n"
      "void main() {\n"
      "  texcoord = p3d_MultiTexCoord0;\n"
      "  color_scale = color;\n"
      "  vtx_texindex = texindex;\n"
      "  wrap_flag_fs = wrap_flag;\n"
      "  fragAtlasUV = atlasUV;\n"
      "  fragClipBounds = clipBounds;\n"
      "  fragSubregionSize = subregionSize;\n"
      "  gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;\n"
      "}\n"
      ,
      // Fragment
      "#version 100\n"
      "precision mediump float;\n"
      "varying vec2 texcoord;\n"
      "varying float vtx_texindex;\n"
      "varying vec4 color_scale;\n"
      "varying vec4 fragClipBounds;\n"
      "uniform sampler2D lui_texture_0;\n"
      "uniform sampler2D lui_texture_1;\n"
      "uniform sampler2D lui_texture_2;\n"
      "uniform sampler2D lui_texture_3;\n"
      "uniform sampler2D lui_texture_4;\n"
      "uniform sampler2D lui_texture_5;\n"
      "uniform sampler2D lui_texture_6;\n"
      "uniform sampler2D lui_texture_7;\n"
      "uniform float screen_width;\n"
      "uniform float screen_height;\n"
      "flat varying uint wrap_flag_fs;\n"
      "varying vec2 fragAtlasUV;\n"
      "varying vec2 fragSubregionSize;\n"
      "void main() {\n"
      "  vec2 fragPos = gl_FragCoord.xy;\n"
      "  vec4 skipColor = vec4(1, 1, 1, 1);\n"
      "  // Check if the fragment is outside the clipping bounds\n"
      "  if ((fragPos.x < fragClipBounds.x || fragPos.x > fragClipBounds.z ||\n"
      "      screen_height - fragPos.y < fragClipBounds.y || screen_height - fragPos.y > fragClipBounds.w)) {\n"
      "    skipColor = vec4(0,0,0,0); // Skip rendering by setting transparent.\n"
      "  }\n"
      "  vec2 mod_uv = vec2(mod(texcoord, fragSubregionSize)) + (fragAtlasUV);\n"
      "  vec2 final_uv = (wrap_flag_fs == 1u) ? mod_uv : texcoord;\n"
      "  vec4 texcolor = vec4(0.1, 0.0, 0.0, 1.0);\n"
      // We can't even properly use defines with the GLES compiler .. that sucks
      "  if (vtx_texindex > -0.5 && vtx_texindex < 0.5) texcolor = texture2D(lui_texture_0, final_uv); \n"
      "  if (vtx_texindex >  0.5 && vtx_texindex < 1.5) texcolor = texture2D(lui_texture_1, final_uv); \n"
      "  if (vtx_texindex >  1.5 && vtx_texindex < 2.5) texcolor = texture2D(lui_texture_2, final_uv); \n"
      "  if (vtx_texindex >  2.5 && vtx_texindex < 3.5) texcolor = texture2D(lui_texture_3, final_uv); \n"
      "  if (vtx_texindex >  3.5 && vtx_texindex < 4.5) texcolor = texture2D(lui_texture_4, final_uv); \n"
      "  if (vtx_texindex >  4.5 && vtx_texindex < 5.5) texcolor = texture2D(lui_texture_5, final_uv); \n"
      "  if (vtx_texindex >  5.5 && vtx_texindex < 6.5) texcolor = texture2D(lui_texture_6, final_uv); \n"
      "  if (vtx_texindex >  6.5 && vtx_texindex < 7.5) texcolor = texture2D(lui_texture_7, final_uv); \n"

      "  gl_FragColor = vec4(texcolor * color_scale * skipColor);\n"
      "}\n"
      );
  }
}
