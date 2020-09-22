glslangValidator -V -S comp --D _CS_ shaders/update_buffer.glsl -o update_buffer.glsl_CS_temp.spv
glslangValidator -V -S vert --D _VS_ shaders/draw_rect.glsl -o draw_rect.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ shaders/draw_rect.glsl -o draw_rect.glsl_PS_temp.spv
glslangValidator -V -S vert --D _VS_ shaders/present.glsl -o draw_rect.glsl_VS_temp.spv
glslangValidator -V -S frag --D _PS_ shaders/present.glsl -o draw_rect.glsl_PS_temp.spv
