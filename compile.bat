glslangValidator -V -S comp --D _CS_ update_buffer.glsl -o update_buffer.glsl_CS_temp.spv
glslangValidator -V -S vert --D _VS_ draw_rect.glsl -o draw_rect.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ draw_rect.glsl -o draw_rect.glsl_PS_temp.spv
