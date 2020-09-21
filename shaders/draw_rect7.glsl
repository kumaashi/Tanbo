/*
 * Copyright (c) 2020 gyabo <gyaboyan@gmail.com>
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */
#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable

layout(set=0, binding=0) uniform sampler2D tex[];

#ifdef _VS_
layout(location=0) in vec4 position;
layout(location=1) in vec4 uv;
layout(location=2) in vec4 color;

layout(location=0) out vec4 v_pos;
layout(location=1) out vec2 v_uv;
layout(location=2) out vec4 v_color;

void main()
{
	v_pos = vec4(position.xyz * vec3(640.0 / 480.0, 1.0, 1.0), 1.0);
	v_uv = uv.xy;
	v_color = color;
	gl_Position = v_pos;
}
#endif //_VS_


#ifdef _PS_

layout(location=0) in vec4 v_pos;
layout(location=1) in vec2 v_uv;
layout(location=2) in vec4 v_color;

layout(location=0) out vec4 out_color;

void main(){
	vec2 uv = v_uv * 0.5 + 0.5;
	//out_color.xyz = vec3(0.0);
	out_color.xyz += pow(texture(tex[0], uv).xyz, vec3(1.5));
	out_color.xyz -= pow(texture(tex[1], uv).xyz, vec3(1.5));
	out_color.xyz += pow(texture(tex[2], uv).xyz, vec3(1.5));
	out_color.xyz -= pow(texture(tex[3], uv).xyz, vec3(1.5));
	out_color.xyz += pow(texture(tex[4], uv).xyz, vec3(1.5));
	out_color.xyz -= pow(texture(tex[5], uv).xyz, vec3(1.5));
	out_color.xyz += pow(texture(tex[6], uv).xyz, vec3(1.5));
	out_color.xyz = vec3(1.0) - out_color.xyz;
	out_color.a = 1.0;
}
#endif //_PS_
