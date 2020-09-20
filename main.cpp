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

//#define VKWIN32_DEBUG
#include "vkwin32.h"

static LRESULT WINAPI
MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
InitWindow(const char *name, int w, int h)
{
	HINSTANCE instance = GetModuleHandle(NULL);
	auto style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
	auto ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	RECT rc = {0, 0, w, h};
	WNDCLASSEX twc = {
		sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, instance,
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
Update()
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

	struct VertexFormat {
		float pos[4];
		float uv[4];
		float color[4];
	};

	struct ObjectFormat {
		float pos[4];
		float scale[4];
		float rotate[4];
		float color[4];
		uint32_t metadata[16];
	};

	enum {
		ScreenWidth = 1024,
		ScreenHeight = 768,
		FrameFifoMax = 2,
		Width = 640,
		Height = 480,
		BitsSize = 4,
		ImageSize = Width * Width * BitsSize,
		ObjectMax = 4096,
		LayerMax = 8,
		DescriptorArrayMax = 32,
		DrawIndirectCommandSize = LayerMax * sizeof(VkDrawIndirectCommand),
		GpuMemoryMax = ImageSize * LayerMax * 4,
		ObjectMaxBytes = ObjectMax * sizeof(ObjectFormat),
		VertexMaxBytes = ObjectMax * sizeof(VertexFormat) * 6,
	};

	enum {
		RDT_SLOT_SRV = 0,
		RDT_SLOT_CBV,
		RDT_SLOT_UAV,
		RDT_SLOT_MAX,
	};

	struct frame_info_t {
		VkImage backbuffer_image = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		VkSemaphore sem = VK_NULL_HANDLE;
		VkCommandBuffer cmdbuf = VK_NULL_HANDLE;

		VkDeviceMemory devmem_host = VK_NULL_HANDLE;
		VkDeviceMemory devmem_local_vertex = VK_NULL_HANDLE;
		VkDeviceMemory devmem_host_draw_indirect_cmd = VK_NULL_HANDLE;

		uint64_t devmem_host_offset = 0;
		uint64_t devmem_local_vertex_offset = 0;

		VkDescriptorSet descriptor_set_cbv = VK_NULL_HANDLE;
		VkDescriptorSet descriptor_set_srv = VK_NULL_HANDLE;

		VkBuffer indirect_draw_cmd_buffer = VK_NULL_HANDLE;
		VkDrawIndirectCommand *host_draw_indirect_cmd = nullptr;

		struct layer_t {
			VkDescriptorSet descriptor_set_uav = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView image_view = VK_NULL_HANDLE;
			VkFramebuffer framebuffer = VK_NULL_HANDLE;
			VkBuffer buffer = VK_NULL_HANDLE;
			void *host_memory_addr = nullptr;
			VkBuffer vertex_buffer = VK_NULL_HANDLE;
		} layer[LayerMax];
	} frame_info[FrameFifoMax];

	static auto hwnd = InitWindow(argv[0], ScreenWidth, ScreenHeight);
	static auto inst = create_instance(appname);
	uint32_t graphics_queue_family_index = -1;
	uint32_t gpu_count = 0;
	static VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	auto err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
	err = vkEnumeratePhysicalDevices(inst, &gpu_count, &gpudev);
	graphics_queue_family_index = get_graphics_queue_index(gpudev);

	VkPhysicalDeviceProperties gpu_props = {};
	vkGetPhysicalDeviceProperties(gpudev, &gpu_props);

	static auto surface = create_win32_surface(inst, hwnd, GetModuleHandle(NULL));
	VkBool32 presentSupport = false;
	vkGetPhysicalDeviceSurfaceSupportKHR(gpudev, 0, surface, &presentSupport);
	VkSurfaceCapabilitiesKHR capabilities = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpudev, surface, &capabilities);

	static auto device = create_device(gpudev, graphics_queue_family_index);
	static auto cmd_pool = create_cmd_pool(device, graphics_queue_family_index);
	static auto swapchain = create_swapchain(device, surface, ScreenWidth, ScreenHeight, FrameFifoMax);
	static VkQueue graphics_queue = VK_NULL_HANDLE;
	static auto sampler = create_sampler(device, true);
	vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
	std::vector<VkImage> temp;
	{
		uint32_t swapchain_count = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, nullptr);
		temp.resize(swapchain_count);
		vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, temp.data());
	}

	auto devmem_local = alloc_device_memory(gpudev, device, GpuMemoryMax, false);
	uint64_t devmem_local_offset = 0;

	static auto descriptor_pool = create_descriptor_pool(device, DescriptorArrayMax);
	static VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	static std::vector<VkDescriptorSetLayout> vdescriptor_layouts;
	{
		std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_srv;
		std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_cbv;
		std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_uav;
		VkShaderStageFlags shader_stages = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
		vdesc_setlayout_binding_srv.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorArrayMax, shader_stages, nullptr});
		vdesc_setlayout_binding_cbv.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorArrayMax, shader_stages, nullptr});
		vdesc_setlayout_binding_uav.push_back({0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ObjectMax, shader_stages, nullptr});
		vdesc_setlayout_binding_uav.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ObjectMax, shader_stages, nullptr});

		vdescriptor_layouts.resize(RDT_SLOT_MAX);
		vdescriptor_layouts[RDT_SLOT_SRV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_srv);
		vdescriptor_layouts[RDT_SLOT_CBV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_cbv);
		vdescriptor_layouts[RDT_SLOT_UAV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_uav);
		pipeline_layout = create_pipeline_layout(device, vdescriptor_layouts.data(), vdescriptor_layouts.size());
	}
	static auto render_pass = create_render_pass(device, VK_FORMAT_R8G8B8A8_UNORM);
	static auto cp_update_buffer = create_cpipeline_from_file(device, "shaders/update_buffer", pipeline_layout);
	static VkPipeline gp_draw_rects[LayerMax];
	for (int i = 0 ; i < LayerMax; i++) {
		auto shader_name = "shaders/draw_rect" + std::to_string(i);
		gp_draw_rects[i] = create_gpipeline_from_file(device, shader_name.c_str(), pipeline_layout, render_pass);
	}

	for (int i = 0 ; i < FrameFifoMax; i++) {
		auto & ref = frame_info[i];
		uint8_t *temp_addr = nullptr;
		ref.backbuffer_image = temp[i];
		ref.fence = create_fence(device);
		ref.sem = create_semaphore(device);
		ref.devmem_host = alloc_device_memory(gpudev, device, LayerMax * ObjectMaxBytes, true);
		ref.devmem_local_vertex = alloc_device_memory(gpudev, device, LayerMax * VertexMaxBytes, false);
		vkMapMemory(device, ref.devmem_host, 0, LayerMax * ObjectMaxBytes, 0, (void **)&temp_addr);

		ref.devmem_host_draw_indirect_cmd = alloc_device_memory(gpudev, device, DrawIndirectCommandSize, true);
		ref.indirect_draw_cmd_buffer = create_buffer(device, DrawIndirectCommandSize);
		vkBindBufferMemory(device, ref.indirect_draw_cmd_buffer, ref.devmem_host_draw_indirect_cmd, 0);
		vkMapMemory(device, ref.devmem_host_draw_indirect_cmd, 0, DrawIndirectCommandSize, 0, (void **)&ref.host_draw_indirect_cmd);

		auto image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ref.descriptor_set_srv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_SRV]);
		ref.descriptor_set_cbv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_CBV]);
		for (auto & layer : ref.layer) {
			layer.descriptor_set_uav = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_UAV]);
			layer.image = create_image(device, Width, Height, VK_FORMAT_R8G8B8A8_UNORM, image_usage_flags);
			vkBindImageMemory(device, layer.image, devmem_local, devmem_local_offset);
			devmem_local_offset += ImageSize;
			layer.image_view = create_image_view(device, layer.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
			std::vector<VkImageView> vimageview = { layer.image_view, };
			layer.framebuffer = create_framebuffer(device, render_pass, vimageview, Width, Height);

			layer.buffer = create_buffer(device, ObjectMaxBytes);
			vkBindBufferMemory(device, layer.buffer, ref.devmem_host, ref.devmem_host_offset);
			ref.devmem_host_offset += ObjectMaxBytes;
			layer.host_memory_addr = temp_addr;
			temp_addr += ObjectMaxBytes;

			layer.vertex_buffer = create_buffer(device, VertexMaxBytes);
			vkBindBufferMemory(device, layer.vertex_buffer, ref.devmem_local_vertex, ref.devmem_local_vertex_offset);
			ref.devmem_local_vertex_offset += VertexMaxBytes;
		}

		for (uint32_t layer_num = 0 ; auto & layer : ref.layer) {
			update_descriptor_combined_image_sample(device, ref.descriptor_set_srv, 0, layer_num, layer.image_view, sampler);
			update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 0, 0, layer.buffer, ObjectMaxBytes);
			update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 1, 0, layer.vertex_buffer, VertexMaxBytes);
			layer_num++;
		}

		ref.cmdbuf = create_command_buffer(device, cmd_pool);
		vkResetCommandBuffer(ref.cmdbuf, 0);
		VkCommandBufferBeginInfo cmdbegininfo = {};
		cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);
		for (uint32_t layer_num = 0 ; auto & layer : ref.layer) {
			std::vector<VkDescriptorSet> vdescriptor_sets = {
				ref.descriptor_set_srv,
				ref.descriptor_set_cbv,
				layer.descriptor_set_uav,
			};
			vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, cp_update_buffer);
			vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
			vkCmdDispatch(ref.cmdbuf, ObjectMax, 1, 1);

			cmd_set_viewport(ref.cmdbuf, 0, 0, Width, Height);
			set_image_memory_barrier(ref.cmdbuf, layer.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			cmd_clear_image(ref.cmdbuf, layer.image, 0, 0, 0, 0);
			vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp_draw_rects[layer_num]);
			vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
			VkDeviceSize offsets[1] = {0};
			vkCmdBindVertexBuffers(ref.cmdbuf, 0, 1, &layer.vertex_buffer, offsets);
			cmd_begin_render_pass(ref.cmdbuf, render_pass, layer.framebuffer, Width, Height);
			vkCmdDrawIndirect(ref.cmdbuf, ref.indirect_draw_cmd_buffer, sizeof(VkDrawIndirectCommand) * layer_num, 1, sizeof(VkDrawIndirectCommand));
			cmd_end_render_pass(ref.cmdbuf);
			layer_num++;
		}
		set_image_memory_barrier(ref.cmdbuf, ref.layer[LayerMax - 1].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		cmd_blit_image(ref.cmdbuf, ref.backbuffer_image, ref.layer[LayerMax - 1].image, ScreenWidth, ScreenHeight, Width, Height);
		set_image_memory_barrier(ref.cmdbuf, ref.layer[LayerMax - 1].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		vkEndCommandBuffer(ref.cmdbuf);
	}

	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	printf("START\n");
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	uint64_t frame_count = 0;
	uint64_t backbuffer_index = 0;
	double phase = 0.0;
	while (Update()) {
		phase += 0.01;
		backbuffer_index = frame_count % FrameFifoMax;
		auto & ref = frame_info[backbuffer_index];
		//test update
		{
			srand(0);
			auto arg = ref.host_draw_indirect_cmd;
			for (int i = 0 ; i < LayerMax - 1; i++) {
				auto & layer = ref.layer[i];
				ObjectFormat *p = (ObjectFormat *)layer.host_memory_addr;
				for (int i = 0 ; i < ObjectMax; i++) {
					p->metadata[0] = 1;
					p->pos[0] = cos(123.0f * frandom() + phase * 2.0);
					p->pos[1] = sin(456.0f * frandom() + phase * 3.0);
					p->scale[0] = 0.25;
					p->scale[1] = 0.05;
					p->rotate[0] = frandom() * 4.0 + phase;

					p->color[0] = frand();
					p->color[1] = frand();
					p->color[2] = frand();
					p->color[3] = 1.0;
					p++;
				}
				arg->vertexCount = ObjectMax * 6;
				arg->instanceCount = 1;
				arg->firstVertex = 0;
				arg->firstInstance = 0;
				arg++;
			}
			auto & layer = ref.layer[LayerMax - 1];
			arg = &ref.host_draw_indirect_cmd[LayerMax - 1];
			ObjectFormat *p = (ObjectFormat *)layer.host_memory_addr;
			p->pos[0] = 0;
			p->pos[1] = 0;
			p->scale[0] = 1;
			p->scale[1] = 1;
			p->rotate[0] = 0;
			p->metadata[0] = 1;
			arg->vertexCount = 6;
			arg->instanceCount = 1;
			arg->firstVertex = 0;
			arg->firstInstance = 0;
		}

		vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &ref.fence);
		uint32_t present_index = 0;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, ref.sem, VK_NULL_HANDLE, &present_index);
		//printf("SUBMIT : present_index=%d\r", present_index);
		submit_command(device, ref.cmdbuf, graphics_queue, ref.fence, ref.sem);
		present_surface(graphics_queue, swapchain, present_index);
		frame_count++;
		if ((frame_count % 60) == 0) {
			for (int i = 0 ; auto & layer : ref.layer)
				printf("%d:layer.host_memory_addr=%p\n", i, layer.host_memory_addr);
			printf("frame_count=%lld\n", frame_count);
			printf("GpuMemoryMax=%d MByte\n", GpuMemoryMax / 1024 / 1024);
			printf("LocalMemRemain=%d MByte\n", (GpuMemoryMax - devmem_local_offset) / 1024 / 1024);
		}
	}
}
