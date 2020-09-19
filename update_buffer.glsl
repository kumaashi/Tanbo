
#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable

struct object_data {
	vec4 pos;
	vec4 scale;
	vec4 rotate;
	vec4 color;
	uint metadata[16];
};

struct vertex_format {
	vec4 pos;
	vec4 uv;
	vec4 color;
};

layout(set=2, binding=0) buffer obj_t {
	object_data obj[];
};

layout(set=2, binding=1) buffer vtx_t {
	vertex_format vtx[];
};

vec2 rotate(vec2 p, float a) {
	float c = cos(a);
	float s = sin(a);
	return vec2(
		p.x * c - p.y * s,
		p.x * s + p.y * c);
}

layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
void main()
{
	uint tid = gl_GlobalInvocationID.x;
	
	vec4 pos = obj[tid].pos;
	vec4 scale = obj[tid].scale;
	vec4 color = obj[tid].color;
	float rotvalue = obj[tid].rotate.x;

	vec2 basepos[4];
	vec2 baseuv[4];

	baseuv[0] = vec2(-1, -1);
	baseuv[1] = vec2(-1,  1);
	baseuv[2] = vec2( 1, -1);
	baseuv[3] = vec2( 1,  1);

	//scale
	basepos[0] = vec2(-1, -1) * scale.xy;
	basepos[1] = vec2(-1,  1) * scale.xy;
	basepos[2] = vec2( 1, -1) * scale.xy;
	basepos[3] = vec2( 1,  1) * scale.xy;
	
	//rotate
	basepos[0] = rotate(basepos[0], rotvalue);
	basepos[1] = rotate(basepos[1], rotvalue);
	basepos[2] = rotate(basepos[2], rotvalue);
	basepos[3] = rotate(basepos[3], rotvalue);

	//trans
	basepos[0] += pos.xy;
	basepos[1] += pos.xy;
	basepos[2] += pos.xy;
	basepos[3] += pos.xy;

	//result
	vec2 aspect = vec2(480.0 / 640.0, 1.0);
	vtx[tid * 6 + 0].pos = vec4(basepos[0] * aspect, 0, 1);
	vtx[tid * 6 + 1].pos = vec4(basepos[1] * aspect, 0, 1);
	vtx[tid * 6 + 2].pos = vec4(basepos[2] * aspect, 0, 1);
	vtx[tid * 6 + 3].pos = vec4(basepos[1] * aspect, 0, 1);
	vtx[tid * 6 + 4].pos = vec4(basepos[3] * aspect, 0, 1);
	vtx[tid * 6 + 5].pos = vec4(basepos[2] * aspect, 0, 1);

	//uv
	vtx[tid * 6 + 0].uv = vec4(baseuv[0], 0, 1);
	vtx[tid * 6 + 1].uv = vec4(baseuv[1], 0, 1);
	vtx[tid * 6 + 2].uv = vec4(baseuv[2], 0, 1);
	vtx[tid * 6 + 3].uv = vec4(baseuv[1], 0, 1);
	vtx[tid * 6 + 4].uv = vec4(baseuv[3], 0, 1);
	vtx[tid * 6 + 5].uv = vec4(baseuv[2], 0, 1);
	
	//color
	vtx[tid * 6 + 0].color = color;
	vtx[tid * 6 + 1].color = color;
	vtx[tid * 6 + 2].color = color;
	vtx[tid * 6 + 3].color = color;
	vtx[tid * 6 + 4].color = color;
	vtx[tid * 6 + 5].color = color;
}
