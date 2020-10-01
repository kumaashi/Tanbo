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
#define VKWIN32_DEBUG
#include "vkcontext.h"

inline void
fork_process_wait(
	const char *command)
{
	PROCESS_INFORMATION pi;

	STARTUPINFO si = {};
	si.cb = sizeof(si);
	auto ret = CreateProcess(
			NULL, (LPTSTR)command,
			NULL, NULL, FALSE,
			NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
	if (!ret) {
		printf("failed command:\n%s\n\n", command);
		return;
	}
	while (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
		Sleep(1);
	}
}

inline void
compile_glsl2spirv(
	std::string shaderfile,
	std::string type,
	std::vector<uint8_t> &vdata)
{
	auto tempfilename = shaderfile + type + std::string("temp.spv");
	auto basecmd = std::string("glslangValidator -V -S ");
	auto soption = std::string("null");
	/*
		.vert   for a vertex shader
		.tesc   for a tessellation control shader
		.tese   for a tessellation evaluation shader
		.geom   for a geometry shader
		.frag   for a fragment shader
		.comp   for a compute shader
		.mesh   for a mesh shader
		.task   for a task shader
		.rgen    for a ray generation shader
		.rint    for a ray intersection shader
		.rahit   for a ray any hit shader
		.rchit   for a ray closest hit shader
		.rmiss   for a ray miss shader
		.rcall   for a ray callable shader
	*/

	if (type == "_VS_")
		soption = "vert";
	if (type == "_GS_")
		soption = "geom";
	if (type == "_PS_")
		soption = "frag";
	if (type == "_CS_")
		soption = "comp";

	basecmd += soption;
	basecmd += " --D " + type + " " + shaderfile + " -o " + tempfilename;
	printf("spawncmd : %s\n", basecmd.c_str());

	fork_process_wait((char *)basecmd.c_str());
	{
		FILE *fp = fopen(tempfilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			vdata.resize(ftell(fp));
			fseek(fp, 0, SEEK_SET);
			if (vdata.size() > 0)
				fread(vdata.data(), 1, vdata.size(), fp);
			fclose(fp);
		}
	}
	DeleteFile(tempfilename.c_str());
}

static LRESULT WINAPI
window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_SYSCOMMAND:
		switch ((wParam & 0xFFF0)) {
		case SC_MONITORPOWER:
		case SC_SCREENSAVE:
			return 0;
		default:
			break;
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_IME_SETCONTEXT:
		lParam &= ~ISC_SHOWUIALL;
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

[[ nodiscard ]]
static HWND
init_window(const char *name, int w, int h)
{
	HINSTANCE instance = GetModuleHandle(NULL);
	auto style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
	auto ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	RECT rc = {0, 0, w, h};
	WNDCLASSEX twc = {
		sizeof(WNDCLASSEX), CS_CLASSDC, window_proc, 0L, 0L, instance,
		LoadIcon(NULL, IDI_APPLICATION), LoadCursor(NULL, IDC_ARROW),
		(HBRUSH) GetStockObject(BLACK_BRUSH), NULL, name, NULL
	};

	RegisterClassEx(&twc);
	AdjustWindowRectEx(&rc, style, FALSE, ex_style);
	rc.right -= rc.left;
	rc.bottom -= rc.top;
	HWND hwnd = CreateWindowEx(
			ex_style, name, name, style,
			(GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2,
			rc.right, rc.bottom, NULL, NULL, instance, NULL);
	ShowWindow(hwnd, SW_SHOW);
	SetFocus(hwnd);
	return hwnd;
};

static int
window_update()
{
	MSG msg;
	int is_active = 1;

	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			is_active = 0;
			break;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return is_active;
}

static void
compile_glsl2spirv_ex(
	std::string name,
	vkcontext_t::create_info::shader_layer_t & shader)
{
	compile_glsl2spirv(name, "_VS_", shader.vs);
	compile_glsl2spirv(name, "_PS_", shader.ps);
}

int
main(int argc, char *argv[])
{
	const char *appname = argv[0];

	auto frand = []() {
		return float(rand()) / float(0x7FFF);
	};
	auto frandom = [ = ]() {
		return frand() * 2.0f - 1.0f;
	};

	vkcontext_t ctx;
	vkcontext_t::create_info cinfo = {};
	vkcontext_t::create_info::shader_layer_t shader_draw_rect;
	vkcontext_t::create_info::shader_layer_t shader_present;

	cinfo.ScreenW = 1024;
	cinfo.ScreenH = 1024;
	cinfo.FrameFifoMax = 2;
	cinfo.Width = 480;
	cinfo.Height = 640;
	cinfo.BitsSize = 4;
	cinfo.LayerMax = 4;
	cinfo.UserImageMax = 32;
	cinfo.ObjectMax = 8192;
	cinfo.DescriptorArrayMax = 32;
	cinfo.GpuMemoryMax = 256 * 1024 * 1024;

	std::string shaderpath = "./shaders/";
	compile_glsl2spirv(shaderpath + "update_buffer.glsl", "_CS_", cinfo.cs_update);
	compile_glsl2spirv_ex(shaderpath + "draw_rect.glsl", shader_draw_rect);
	compile_glsl2spirv_ex(shaderpath + "present.glsl", shader_present);
	for (int i = 0 ; i < cinfo.LayerMax - 1; i++)
		cinfo.shader_layers.push_back(shader_draw_rect);
	cinfo.shader_layers.push_back(shader_present);

	cinfo.appname = argv[0];
	cinfo.hwnd = init_window(cinfo.appname, cinfo.ScreenW, cinfo.ScreenH);
	cinfo.hinst = GetModuleHandle(NULL);
	ctx.init(cinfo);
	{
		std::vector<uint32_t> testtex;
		for (int y = 0; y < 256; y++) {
			for (int x = 0; x < 256; x++) {
				uint8_t a = x ^ y;
				testtex.push_back(x ^ y);
			}
		}
		ctx.upload_user_image(0, 256, 256, testtex.data());
		ctx.upload_user_image(1, 256, 256, testtex.data());
	}
	ctx.create_cmdbuf();

	printf("START\n");
	uint64_t frame_count = 0;
	double phase = 0.0;
	static int tex_id = 20;
	while (window_update()) {
		if (GetAsyncKeyState(VK_DOWN) & 0x0001) {
			tex_id--;
			if (tex_id < 0)
				tex_id = 0;
		}
		if (GetAsyncKeyState(VK_UP) & 0x0001) {
			tex_id++;
		}
		phase += 0.01;
		srand(0);
		for (int i = 0 ; i < cinfo.LayerMax - 1; i++) {
			auto p = ctx.get_object_format_address(i);
			for (int i = 0 ; i < cinfo.ObjectMax; i++) {
				p->metadata[0] = 1;
				p->pos[0] = cos(3 * cos(123.0f * frandom() + frandom() * phase * 2.0 * 0.05));
				p->pos[1] = cos(3 * sin(456.0f * frandom() + frandom() * phase * 3.0 * 0.05));
				p->scale[0] = 0.1 + frand() * 0.05;
				p->scale[1] = 0.001 + frand() * 0.05;
				p->rotate[0] = frandom() * 10.0 + phase * 5.0;
				p->color[0] = frand();
				p->color[1] = frand();
				p->color[2] = frand();
				p->color[3] = 1.0;

				//for test 8x8 tex
				p->uvinfo[0] = tex_id % 8;
				p->uvinfo[1] = tex_id / 8;
				p->uvinfo[2] = 8;
				p->uvinfo[3] = 8;

				//all
				p->uvinfo[0] = 0;
				p->uvinfo[1] = 0;
				p->uvinfo[2] = 1;
				p->uvinfo[3] = 1;


				p++;
			}
			ctx.draw_triangles(i, cinfo.ObjectMax * 6);
		}
		auto last_index = cinfo.LayerMax - 1;
		auto p = ctx.get_object_format_address(last_index);
		p->metadata[0] = 1;
		p->pos[0] = 0;
		p->pos[1] = 0;
		p->scale[0] = 1;
		p->scale[1] = 1;
		p->rotate[0] = 0;
		p->uvinfo[0] = 0;
		p->uvinfo[1] = 0;
		p->uvinfo[2] = 1;
		p->uvinfo[3] = 1;
		ctx.draw_triangles(last_index, 6);
		ctx.submit();

		frame_count++;
		if ((frame_count % 60) == 0) {
			printf("frame_count=%lld\n", frame_count);
		}
	}
}
