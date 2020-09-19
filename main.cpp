/*
 *
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

#pragma once

#include <stdio.h>
#include <windows.h>
#include <map>
#include <vector>
#include <string>
#include <vector>
#include <algorithm>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(lib, "vulkan-1.lib")

static void fork_process_wait(const char *command)
{
	PROCESS_INFORMATION pi;

	STARTUPINFO si = {};
	si.cb = sizeof(si);
	if (CreateProcess(NULL, (LPTSTR)command, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
		while (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
			Sleep(0);
		}
	}
}

static void compile_glsl2spirv(
	std::string shaderfile, std::string type, std::vector<unsigned char> &vdata)
{
	printf("DEBUG : ===========================\n%s\n===========================\n", shaderfile.c_str());
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
	basecmd += " --D " + type + " " + shaderfile + std::string(" -o ") + tempfilename;
	printf("basecmd : %s\n", basecmd.c_str());

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

static VKAPI_ATTR VkBool32
VKAPI_CALL vk_callback_printf(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT,
	uint64_t object,
	size_t location,
	int32_t messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		printf("\n\nvkdbg: ERROR : ");
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		printf("\n\nvkdbg: WARNING : ");
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		printf("\n\nvkdbg: PERFORMANCE : ");
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		printf("INFO : ");
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		printf("DEBUG : ");
	printf("%s", pMessage);
	printf("\n");
	return VK_FALSE;
}

VkDebugReportCallbackEXT
bind_debug_fn(
	VkInstance instance,
	VkDebugReportCallbackCreateInfoEXT ext)
{
	VkDebugReportCallbackEXT callback = NULL;
	auto cb = PFN_vkCreateDebugReportCallbackEXT(
			vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));

	if (cb)
		cb(instance, &ext, nullptr, &callback);
	else
		printf("PFN_vkCreateDebugReportCallbackEXT IS NULL\n");
	return callback;
}


[[ nodiscard ]] static VkInstance
create_instance(const char *appname) {
	const char *vinstance_ext_names[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		

		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	};
	VkInstance inst = VK_NULL_HANDLE;
	VkApplicationInfo vkapp = {};
	VkInstanceCreateInfo inst_info = {};
	VkDebugReportCallbackCreateInfoEXT drcc_info = {};

	vkapp.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkapp.pNext = &drcc_info;
	vkapp.pApplicationName = appname;
	vkapp.pEngineName = appname;
	vkapp.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	vkapp.apiVersion = VK_API_VERSION_1_0;
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext = NULL;
	inst_info.pApplicationInfo = &vkapp;
	inst_info.enabledExtensionCount = (uint32_t)_countof(vinstance_ext_names);
	inst_info.ppEnabledExtensionNames = vinstance_ext_names;

	static const char *debuglayers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
	inst_info.enabledLayerCount = _countof(debuglayers);
	inst_info.ppEnabledLayerNames = debuglayers;

	drcc_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	drcc_info.flags = 0;
	drcc_info.flags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
	drcc_info.flags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
	drcc_info.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	drcc_info.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
	drcc_info.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	drcc_info.pfnCallback = &vk_callback_printf;
	
	auto err = vkCreateInstance(&inst_info, NULL, &inst);
	
	bind_debug_fn(inst, drcc_info);

	return inst;
}

[[ nodiscard ]] static VkDevice
create_device(VkPhysicalDevice gpudev, uint32_t graphics_queue_family_index)
{

	VkDevice ret = VK_NULL_HANDLE;
	VkDeviceCreateInfo device_info = {};
	VkDeviceQueueCreateInfo queue_info = {};
	const char *ext_names[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	float queue_priorities[1] = {0.0};
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = graphics_queue_family_index;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = queue_priorities;

	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.enabledExtensionCount = (uint32_t)_countof(ext_names);
	device_info.ppEnabledExtensionNames = ext_names;
	auto err = vkCreateDevice(gpudev, &device_info, NULL, &ret);
	return ret;
}

[[ nodiscard ]] static VkSampler
create_sampler(VkDevice device, bool isfilterd)
{
	VkSampler ret = VK_NULL_HANDLE;
	VkSamplerCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = NULL;
	info.magFilter = VK_FILTER_NEAREST;
	info.minFilter = VK_FILTER_NEAREST;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.mipLodBias = 0.0f;
	info.anisotropyEnable = VK_FALSE;
	info.maxAnisotropy = 0;
	info.compareOp = VK_COMPARE_OP_NEVER;
	info.minLod = 0.0f;
	info.maxLod = FLT_MAX;
	info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	info.unnormalizedCoordinates = VK_FALSE;
	if (isfilterd) {
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}

	auto err = vkCreateSampler(device, &info, NULL, &ret);
	return (ret);
}

[[ nodiscard ]] static VkCommandPool
create_cmd_pool(VkDevice device, uint32_t index_qfi)
{
	VkCommandPoolCreateInfo info = {};
	VkCommandPool ret = nullptr;

	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = index_qfi;
	auto err = vkCreateCommandPool(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]] static VkCommandBuffer
create_command_buffer(VkDevice device, VkCommandPool cmd_pool)
{
	VkCommandBufferAllocateInfo info = {};
	VkCommandBuffer ret = nullptr;

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandPool = cmd_pool;
	info.commandBufferCount = 1;
	auto err = vkAllocateCommandBuffers(device, &info, &ret);

	return (ret);
}

[[ nodiscard ]] static VkDeviceMemory
alloc_device_memory(
	VkPhysicalDevice gpudev,
	VkDevice device,
	VkDeviceSize size,
	bool is_host)
{
	VkDeviceMemory ret = VK_NULL_HANDLE;
	VkMemoryAllocateInfo ma_info = {};
	VkPhysicalDeviceMemoryProperties devicememoryprop = {};
	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	ma_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ma_info.allocationSize = size;
	vkGetPhysicalDeviceMemoryProperties(gpudev, &devicememoryprop);

	if (is_host) {
		flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}
	for (uint32_t i = 0; i < devicememoryprop.memoryTypeCount; i++) {
		if ((devicememoryprop.memoryTypes[i].propertyFlags & flags) == flags) {
			ma_info.memoryTypeIndex = i;
			break;
		}
	}
	vkAllocateMemory(device, &ma_info, nullptr, &ret);
	return ret;
};

[[ nodiscard ]] static VkSemaphore
create_semaphore(VkDevice device)
{
	VkSemaphore ret = VK_NULL_HANDLE;

	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(device, &info, nullptr, &ret);
	return ret;
}


[[ nodiscard ]] static VkFence
create_fence(VkDevice device)
{
	VkFence ret = VK_NULL_HANDLE;
	VkFenceCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(device, &info, NULL, &ret);

	return ret;
}

[[ nodiscard ]] static VkSurfaceKHR
create_win32_surface(VkInstance inst, HWND hwnd, HINSTANCE hinst)
{
	VkSurfaceKHR ret = VK_NULL_HANDLE;
	VkWin32SurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = hinst;
	info.hwnd = hwnd;
	vkCreateWin32SurfaceKHR(inst, &info, NULL, &ret);
	return ret;
}

static VkSwapchainKHR
create_swapchain(VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height, uint32_t fifomax)
{
	static VkSwapchainKHR ret = VK_NULL_HANDLE;
	VkSwapchainCreateInfoKHR info = {};

	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = fifomax;
	info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	info.imageExtent.width = width;
	info.imageExtent.height = height;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	//info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	
	info.clipped = VK_TRUE;
	vkCreateSwapchainKHR(device, &info, nullptr, &ret);
	return ret;
}

static void
submit_command(VkDevice device, VkCommandBuffer cmdbuf, VkQueue queue, VkFence fence, VkSemaphore sem)
{
	VkSubmitInfo submit_info = {};
	VkPipelineStageFlags wait_mask[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore waitSemaphores[] = { sem };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitDstStageMask = wait_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pWaitSemaphores = waitSemaphores;
	submit_info.pCommandBuffers = &cmdbuf;
	submit_info.signalSemaphoreCount = 0;

	vkResetFences(device, 1, &fence);
	vkQueueSubmit(queue, 1, &submit_info, fence);
}

static void
present_surface(VkQueue queue, VkSwapchainKHR swapchain, uint32_t present_index)
{
	VkPresentInfoKHR info = {};

	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pSwapchains = &swapchain;
	info.swapchainCount = 1;
	info.pImageIndices = &present_index;
	vkQueuePresentKHR(queue, &info);
}

[[ nodiscard ]]
static void
set_image_memory_barrier(
	VkCommandBuffer cmdbuf,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout old_image_layout,
	VkImageLayout new_image_layout,
	uint32_t baseMipLevel = 0,
	uint32_t levelCount = 1,
	uint32_t baseArrayLayer = 0,
	uint32_t layerCount = 1)
{
	VkImageMemoryBarrier ret = {};

	ret.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ret.image = image;
	ret.oldLayout = old_image_layout;
	ret.newLayout = new_image_layout;
	ret.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ret.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ret.subresourceRange = { aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount };
	vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &ret);
}

[[ nodiscard ]]
static VkBuffer
create_buffer(
	VkDevice device,
	VkDeviceSize size,
	VkBufferCreateInfo *pinfo = nullptr)
{
	VkBuffer ret = VK_NULL_HANDLE;
	VkBufferCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size  = size;
	info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	vkCreateBuffer(device, &info, nullptr, &ret);
	if (ret && pinfo)
		*pinfo = info;

	return (ret);
}


[[ nodiscard ]]
static VkImage
create_image(
	VkDevice device,
	uint32_t width,
	uint32_t height,
	VkFormat format,
	VkImageUsageFlags usageFlags,
	VkImageCreateInfo *pinfo = nullptr)
{
	VkImage ret = VK_NULL_HANDLE;
	VkImageCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = 1;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkCreateImage(device, &info, NULL, &ret);
	if (ret && pinfo)
		*pinfo = info;
	return (ret);
}

[[ nodiscard ]]
static VkImageView
create_image_view(
	VkDevice device,
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspectMask,
	VkImageViewCreateInfo *pinfo = nullptr)
{
	VkImageView ret = VK_NULL_HANDLE;
	VkImageViewCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.components.r = VK_COMPONENT_SWIZZLE_R;
	info.components.g = VK_COMPONENT_SWIZZLE_G;
	info.components.b = VK_COMPONENT_SWIZZLE_B;
	info.components.a = VK_COMPONENT_SWIZZLE_A;
	info.subresourceRange.aspectMask = aspectMask;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;
	vkCreateImageView(device, &info, NULL, &ret);
	return (ret);
}


[[ nodiscard ]]
static VkRenderPass
create_render_pass(
	VkDevice device,
	VkFormat color_format, 
	uint32_t color_num = 1)
{
	VkRenderPass ret = VK_NULL_HANDLE;

	int attachment_index = 0;
	std::vector<VkAttachmentDescription> vattachments;
	std::vector<VkAttachmentReference> vattachment_refs;
	std::vector<VkSubpassDependency> vsubpassdepends;
	VkAttachmentDescription color_attachment = {};

	color_attachment.flags = 0;
	color_attachment.format = color_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkAttachmentReference color_reference = {};
	color_reference.attachment = 0;
	color_reference.layout = VK_IMAGE_LAYOUT_UNDEFINED;

	for (int i = 0 ; i < color_num; i++) {
		auto ref = color_reference;
		ref.attachment = attachment_index;
		vattachments.push_back(color_attachment);
		vattachment_refs.push_back(ref);
		attachment_index++;
	}

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = (uint32_t)vattachment_refs.size();
	subpass.pColorAttachments = vattachment_refs.data();

	VkRenderPassCreateInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rp_info.attachmentCount = (uint32_t)vattachments.size();
	rp_info.pAttachments = vattachments.data();
	rp_info.subpassCount = 1;
	rp_info.pSubpasses = &subpass;
	auto err = vkCreateRenderPass(device, &rp_info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkFramebuffer
create_framebuffer(
	VkDevice device,
	VkRenderPass render_pass,
	std::vector<VkImageView> vimageview,
	uint32_t width,
	uint32_t height)
{
	VkFramebuffer fb = nullptr;
	VkFramebufferCreateInfo fb_info = {};

	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = NULL;
	fb_info.renderPass = render_pass;
	fb_info.attachmentCount = (uint32_t)vimageview.size();
	fb_info.pAttachments = vimageview.data();
	fb_info.width = width;
	fb_info.height = height;
	fb_info.layers = 1;
	auto err = vkCreateFramebuffer(device, &fb_info, NULL, &fb);

	return (fb);
}

[[ nodiscard ]]
static VkDescriptorPool
create_descriptor_pool(
	VkDevice device, uint32_t heapcount, uint32_t max_sets = 0xFF)
{
	VkDescriptorPool ret = nullptr;
	VkDescriptorPoolCreateInfo info = {};
	std::vector<VkDescriptorPoolSize> vpoolsizes;

	heapcount = 0xFFFFF;

	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, heapcount});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, heapcount});

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.maxSets = max_sets;
	info.poolSizeCount = (uint32_t)vpoolsizes.size();
	info.pPoolSizes = vpoolsizes.data();
	auto err = vkCreateDescriptorPool(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkDescriptorSetLayout
create_descriptor_set_layout(
	VkDevice device,
	std::vector<VkDescriptorSetLayoutBinding> & vdesc_setlayout_binding)
{
	VkDescriptorSetLayout ret = nullptr;
	VkDescriptorSetLayoutCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pBindings = vdesc_setlayout_binding.data();
	info.bindingCount = (uint32_t)vdesc_setlayout_binding.size();
	auto err = vkCreateDescriptorSetLayout(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkPipelineLayout
create_pipeline_layout(
	VkDevice device,
	VkDescriptorSetLayout *descriptor_layouts,
	size_t count)
{
	VkPipelineLayout ret = nullptr;
	VkPipelineLayoutCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = count;
	info.pSetLayouts = descriptor_layouts;
	auto err = vkCreatePipelineLayout(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkDescriptorSet
create_descriptor_set(
	VkDevice device,
	VkDescriptorPool descriptor_pool,
	VkDescriptorSetLayout descriptor_layout)
{
	VkDescriptorSet ret = VK_NULL_HANDLE;
	VkDescriptorSetAllocateInfo alloc_info = {};

	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	alloc_info.pNext = NULL,
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1,
	alloc_info.pSetLayouts = &descriptor_layout;
	auto err = vkAllocateDescriptorSets(device, &alloc_info, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkShaderModule
create_shader_module(
	VkDevice device, void *data, size_t size)
{
	VkShaderModule ret = nullptr;
	VkShaderModuleCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.pCode = (uint32_t *)data;
	info.codeSize = size;
	vkCreateShaderModule(device, &info, nullptr, &ret);

	return (ret);
}

[[ nodiscard ]]
static VkPipeline
create_cpipeline_from_file(
	VkDevice device,
	const char *filename,
	VkPipelineLayout pipeline_layout)
{
	VkPipeline ret = nullptr;
	VkComputePipelineCreateInfo info = {};
	std::vector<uint8_t> cs;
	std::vector<VkShaderModule> vshadermodules;
	auto fname = std::string(filename);

	compile_glsl2spirv((fname + ".glsl").c_str(), "_CS_", cs);
	if (cs.empty())
		return VK_NULL_HANDLE;

	auto module = create_shader_module(device, cs.data(), cs.size());
	vshadermodules.push_back(module);
	info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage.pName = "main";
	info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	info.stage.module = module;
	info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.layout = pipeline_layout;
	vkCreateComputePipelines(device, nullptr, 1, &info, nullptr, &ret);

	for (auto & modules : vshadermodules)
		if (modules)
			vkDestroyShaderModule(device, modules, nullptr);

	return (ret);
}

[[ nodiscard ]]
static VkPipeline
create_gpipeline_from_file(
	VkDevice device,
	const char *filename,
	VkPipelineLayout pipeline_layout,
	VkRenderPass render_pass)
{
	VkPipeline ret = nullptr;
	VkPipelineCacheCreateInfo pipelineCache = {};
	VkPipelineVertexInputStateCreateInfo vi = {};
	VkPipelineInputAssemblyStateCreateInfo ia = {};
	VkPipelineRasterizationStateCreateInfo rs = {};
	VkPipelineColorBlendStateCreateInfo cb = {};
	VkPipelineDepthStencilStateCreateInfo ds = {};
	VkPipelineViewportStateCreateInfo vp = {};
	VkPipelineMultisampleStateCreateInfo ms = {};
	VkPipelineDynamicStateCreateInfo dyns = {};

	//setup vp, sc
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	std::vector<VkDynamicState> vdynamic_state_enables;
	vdynamic_state_enables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	vdynamic_state_enables.push_back(VK_DYNAMIC_STATE_SCISSOR);

	dyns.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyns.pDynamicStates = vdynamic_state_enables.data();
	dyns.dynamicStateCount = vdynamic_state_enables.size();

	//SETUP RS
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;//VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.depthClampEnable = VK_FALSE;
	rs.rasterizerDiscardEnable = VK_FALSE;
	rs.depthBiasEnable = VK_FALSE;
	rs.lineWidth = 1.0f;

	//SETUP CBA todo
	VkPipelineColorBlendAttachmentState att_state[1] = {};
	att_state[0].colorWriteMask = 0xf;
	att_state[0].blendEnable = VK_TRUE;
	att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR; //VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
	att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
	att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = att_state;

	//SETUP DS
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.back.failOp = VK_STENCIL_OP_KEEP;
	ds.back.passOp = VK_STENCIL_OP_KEEP;
	ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
	ds.stencilTestEnable = VK_FALSE;
	ds.front = ds.back;

	//SETUP MS
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.pSampleMask = NULL;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//SETUP SM
	std::vector<VkPipelineShaderStageCreateInfo> vsstageinfo;
	std::vector<VkShaderModule> vshadermodules;

	std::vector<uint8_t> vs;
	std::vector<uint8_t> ps;
	auto fname = std::string(filename);
	compile_glsl2spirv((fname + ".glsl").c_str(), "_VS_", vs);
	compile_glsl2spirv((fname + ".glsl").c_str(), "_PS_", ps);

	VkPipelineShaderStageCreateInfo sstage = {};
	sstage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sstage.pName = "main"; //glsl spec

	if (!vs.empty()) {
		auto module = create_shader_module(device, vs.data(), vs.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		sstage.module = module;
		vsstageinfo.push_back(sstage);
	}
	if (!ps.empty()) {
		auto module = create_shader_module(device, ps.data(), ps.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		sstage.module = module;
		vsstageinfo.push_back(sstage);
	}

	//SETUP IA
	uint32_t stride_size = 0;
	std::vector<VkVertexInputAttributeDescription> vvia_desc;
	vvia_desc.push_back({0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;
	vvia_desc.push_back({1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;
	vvia_desc.push_back({2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;

	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkVertexInputBindingDescription vi_ibdesc = {};
	vi_ibdesc.binding = 0;
	vi_ibdesc.stride = stride_size;
	vi_ibdesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &vi_ibdesc;
	vi.vertexAttributeDescriptionCount = vvia_desc.size();
	vi.pVertexAttributeDescriptions = vvia_desc.data();

	//Create Pipeline
	if (vsstageinfo.empty()) {
		printf("failed create pipeline fname=%s\n", fname.c_str());
		return nullptr;
	}
	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.layout = pipeline_layout;
	info.pVertexInputState = &vi;
	info.pInputAssemblyState = &ia;
	info.pRasterizationState = &rs;
	info.pColorBlendState = &cb;
	info.pMultisampleState = &ms;
	info.pViewportState = &vp;
	info.pDepthStencilState = &ds;
	info.stageCount = vsstageinfo.size();
	info.pStages = vsstageinfo.data();
	info.pDynamicState = &dyns;
	info.renderPass = render_pass;
	auto pipeline_result = vkCreateGraphicsPipelines(
			device, nullptr, 1, &info, NULL, &ret);
	for (auto & module : vshadermodules)
		if (module)
			vkDestroyShaderModule(device, module, nullptr);
	return (ret);
}

static void
update_descriptor_sets(
	VkDevice device,
	VkDescriptorSet descriptor_sets,
	const void *pinfo,
	uint32_t index,
	uint32_t binding,
	VkDescriptorType type)
{
	VkWriteDescriptorSet wd_sets = {};

	wd_sets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	wd_sets.pNext = NULL;
	wd_sets.descriptorType = type;
	wd_sets.descriptorCount = 1;
	wd_sets.dstSet = descriptor_sets;
	wd_sets.dstBinding = binding;
	wd_sets.dstArrayElement = index;
	if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		wd_sets.pImageInfo = (const VkDescriptorImageInfo *)pinfo;
	if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		wd_sets.pImageInfo = (const VkDescriptorImageInfo *)pinfo;
	if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		wd_sets.pBufferInfo = (const VkDescriptorBufferInfo *)pinfo;
	if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		wd_sets.pBufferInfo = (const VkDescriptorBufferInfo *)pinfo;
	vkUpdateDescriptorSets(device, 1, &wd_sets, 0, NULL);
}

static void
update_descriptor_storage_buffer(VkDevice device, VkDescriptorSet dset, uint32_t binding, VkBuffer buf, VkDeviceSize bytes)
{
	VkDescriptorBufferInfo info = {};

	info.buffer = buf;
	info.offset = 0;
	info.range = bytes;
	update_descriptor_sets(device, dset, &info, 0, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

static void
cmd_clear_image(VkCommandBuffer cmdbuf, VkImage image, float r, float g, float b, float a)
{
	VkClearColorValue clear_color = {};
	clear_color.float32[0] = r;
	clear_color.float32[1] = g;
	clear_color.float32[2] = b;
	clear_color.float32[3] = a;
	VkImageSubresourceRange image_range_color = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	set_image_memory_barrier(cmdbuf, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkCmdClearColorImage(cmdbuf, image, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &image_range_color);
	set_image_memory_barrier(cmdbuf, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
}

static void
cmd_set_viewport(VkCommandBuffer cmdbuf, float x, float y, float w, float h)
{
	VkViewport viewport = {};
	viewport.width = (float)w;
	viewport.height = (float)h;
	viewport.minDepth = (float)0.0f;
	viewport.maxDepth = (float)1.0f;
	vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent.width = w;
	scissor.extent.height = h;
	scissor.offset.x = x;
	scissor.offset.y = y;
	vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
}

static void
cmd_begin_render_pass(VkCommandBuffer cmdbuf, VkRenderPass rp, VkFramebuffer framebuffer, uint32_t width, uint32_t height)
{
	VkRenderPassBeginInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = rp;
	info.framebuffer = framebuffer;
	info.renderArea.extent.width = width;
	info.renderArea.extent.height = height;
	vkCmdBeginRenderPass(cmdbuf, &info, VK_SUBPASS_CONTENTS_INLINE);
}

static void
cmd_end_render_pass(VkCommandBuffer cmdbuf)
{
	vkCmdEndRenderPass(cmdbuf);
}


int
main(int argc, char *argv[])
{
	const char *appname = argv[0];

	auto frand = []() {
		return float(rand()) / float(0x7FFF);
	};
	auto frandom = [=]() {
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
		Width = 640,
		Height = 480,
		BitsSize = 4,
		ImageSize = Width * Width * BitsSize,
		FrameFifoMax = 2,
		ObjectMax = 256,
		LayerMax = 8,
		GpuMemoryMax = ImageSize * LayerMax * 4,
		DescriptorArrayMax = 32,
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
		uint64_t offset_devmem_host = 0;
		VkDeviceMemory devmem_dest = VK_NULL_HANDLE;
		uint64_t offset_devmem_dest = 0;
		
		struct layer_t {
			VkDescriptorSet descriptor_set_srv = VK_NULL_HANDLE;
			VkDescriptorSet descriptor_set_cbv = VK_NULL_HANDLE;
			VkDescriptorSet descriptor_set_uav = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView image_view = VK_NULL_HANDLE;
			VkFramebuffer framebuffer = VK_NULL_HANDLE;

			VkBuffer buffer = VK_NULL_HANDLE;
			void *host_memory_addr = nullptr;
			VkBuffer vertex_buffer = VK_NULL_HANDLE;
		} layer[LayerMax];
	} frame_info[FrameFifoMax];

	uint32_t gpu_count = 0;
	uint32_t queue_family_count = 0;
	uint32_t graphics_queue_family_index = -1;
	static auto hwnd = InitWindow(argv[0], Width, Height);
	static auto inst = create_instance(appname);
	static VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	auto err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
	err = vkEnumeratePhysicalDevices(inst, &gpu_count, &gpudev);
	VkPhysicalDeviceProperties gpu_props = {};
	vkGetPhysicalDeviceProperties(gpudev, &gpu_props);
	vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> vqueue_props(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &queue_family_count, vqueue_props.data());
	for (uint32_t i = 0; i < queue_family_count; i++) {
		auto flags = vqueue_props[i].queueFlags;
		if (flags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_queue_family_index = i;
			break;
		}
	}
	static auto device = create_device(gpudev, graphics_queue_family_index);
	static auto cmd_pool = create_cmd_pool(device, graphics_queue_family_index);
	static auto surface = create_win32_surface(inst, hwnd, GetModuleHandle(NULL));
	static auto swapchain = create_swapchain(device, surface, Width, Height, FrameFifoMax);
	static VkQueue graphics_queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
	std::vector<VkImage> temp;
	{
		uint32_t swapchain_count = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, nullptr);
		temp.resize(swapchain_count);
		vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, temp.data());
	}

	auto devmem_local = alloc_device_memory(gpudev, device, GpuMemoryMax, false);
	uint64_t offset_devmem_local = 0;

	static auto descriptor_pool = create_descriptor_pool(device, 32);
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
	static auto cp_update_buffer = create_cpipeline_from_file(device, "update_buffer", pipeline_layout);
	static auto gp_draw_rect = create_gpipeline_from_file(device, "draw_rect", pipeline_layout, render_pass);
	
	for(int i = 0 ; i < FrameFifoMax; i++) {
		auto & ref = frame_info[i];
		ref.backbuffer_image = temp[i];
		ref.fence = create_fence(device);
		ref.sem = create_semaphore(device);
		
		ref.devmem_host = alloc_device_memory(gpudev, device, LayerMax * ObjectMaxBytes, true);
		ref.devmem_dest = alloc_device_memory(gpudev, device, LayerMax * VertexMaxBytes, false);
		uint8_t *temp_addr = nullptr;
		vkMapMemory(device, ref.devmem_host, 0, LayerMax * ObjectMaxBytes, 0, (void **)&temp_addr);

		auto image_usage_flags = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		for(auto & layer : ref.layer) {
			layer.descriptor_set_srv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_SRV]);
			layer.descriptor_set_cbv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_CBV]);
			layer.descriptor_set_uav = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_UAV]);
			layer.image = create_image(device, Width, Height, VK_FORMAT_R8G8B8A8_UNORM, image_usage_flags);
			vkBindImageMemory(device, layer.image, devmem_local, offset_devmem_local);
			offset_devmem_local += ImageSize;
			layer.image_view = create_image_view(device, layer.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
			std::vector<VkImageView> vimageview = { layer.image_view, };
			layer.framebuffer = create_framebuffer(device, render_pass, vimageview, Width, Height);

			layer.buffer = create_buffer(device, ObjectMaxBytes);
			layer.vertex_buffer = create_buffer(device, VertexMaxBytes);
			vkBindBufferMemory(device, layer.buffer, ref.devmem_host, ref.offset_devmem_host);
			vkBindBufferMemory(device, layer.vertex_buffer, ref.devmem_dest, ref.offset_devmem_dest);
			
			ref.offset_devmem_host += ObjectMaxBytes;
			ref.offset_devmem_dest += VertexMaxBytes;

			layer.host_memory_addr = temp_addr;
			temp_addr += ObjectMaxBytes;

			update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 0, layer.buffer, ObjectMaxBytes);
			update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 1, layer.vertex_buffer, VertexMaxBytes);
		}

		ref.cmdbuf = create_command_buffer(device, cmd_pool);
		vkResetCommandBuffer(ref.cmdbuf, 0);
		VkCommandBufferBeginInfo cmdbegininfo = {};
		cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);
		for(auto & layer : ref.layer) {
			std::vector<VkDescriptorSet> vdescriptor_sets = {
				layer.descriptor_set_srv,
				layer.descriptor_set_cbv,
				layer.descriptor_set_uav,
			};
			cmd_set_viewport(ref.cmdbuf, 0, 0, Width, Height);
			cmd_clear_image(ref.cmdbuf, layer.image, 0, 0, 0, 0);
			vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, cp_update_buffer);
			vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
			vkCmdDispatch(ref.cmdbuf, ObjectMax, 1, 1);

			VkDeviceSize offsets[1] = {0};
			vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp_draw_rect);
			vkCmdBindVertexBuffers(ref.cmdbuf, 0, 1, &layer.vertex_buffer, offsets);
			vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
			cmd_begin_render_pass(ref.cmdbuf, render_pass, layer.framebuffer, Width, Height);
			vkCmdDraw(ref.cmdbuf, ObjectMax * 6, ObjectMax, 0, 0);
			cmd_end_render_pass(ref.cmdbuf);
		}
		
		cmd_clear_image(ref.cmdbuf, ref.backbuffer_image, !i, !!i, !i, 1);
		set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		vkEndCommandBuffer(ref.cmdbuf);
		printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
		printf("frame=%d\n", i);
		printf("ref.image=0x%p\n", ref.backbuffer_image);
		printf("ref.cmdbuf=0x%p\n", ref.cmdbuf);
		printf("ref.fence=0x%p\n", ref.fence);
		printf("ref.sem=0x%p\n", ref.sem);
		printf("ref.devmem_host=0x%p\n", ref.devmem_host);
		for(int index = 0 ; auto & layer : ref.layer) {
			//descset
			printf("layer%d:layer.descriptor_set_srv = 0x%p\n", index, layer.descriptor_set_srv);
			printf("layer%d:layer.descriptor_set_cbv = 0x%p\n", index, layer.descriptor_set_cbv);
			printf("layer%d:layer.descriptor_set_uav = 0x%p\n", index, layer.descriptor_set_uav);

			printf("layer%d:layer.image = 0x%p\n", index, layer.image);
			printf("layer%d:layer.buffer = 0x%p\n", index, layer.buffer);
			printf("layer%d:layer.vertex_buffer = 0x%p\n", index, layer.vertex_buffer);
			printf("layer%d:layer.host_memory_addr = 0x%p\n", index, layer.host_memory_addr);
			printf("layer%d:layer.framebuffer = 0x%p\n", index, layer.framebuffer);
			
			index++;
		}
	}
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	printf("inst=0x%p\n", inst);
	printf("gpudev=0x%p\n", gpudev);
	printf("minTexelBufferOffsetAlignment  =0x%p\n", (void *)gpu_props.limits.minTexelBufferOffsetAlignment);
	printf("minUniformBufferOffsetAlignment=0x%p\n", (void *)gpu_props.limits.minUniformBufferOffsetAlignment);
	printf("minStorageBufferOffsetAlignment=0x%p\n", (void *)gpu_props.limits.minStorageBufferOffsetAlignment);
	printf("maxDescriptorSetUniformBuffersDynamic=0x%p\n", (void *)gpu_props.limits.maxDescriptorSetUniformBuffersDynamic);
	
	printf("graphics_queue_family_index=%d\n", graphics_queue_family_index);
	printf("queue_family_count=%d\n", queue_family_count);
	printf("gpudev=0x%p\n", gpudev);
	printf("device=0x%p\n", device);
	printf("surface=0x%p\n", surface);
	printf("swapchain=0x%p\n", swapchain);
	printf("graphics_queue=0x%p\n", graphics_queue);
	printf("cmd_pool=0x%p\n", cmd_pool);
	printf("devmem_local=0x%p\n", devmem_local);
	printf("descriptor_pool=0x%p\n", descriptor_pool);
	printf("cp_update_buffer=0x%p\n", cp_update_buffer);
	printf("pipeline_layout=0x%p\n", pipeline_layout);
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");

	uint64_t frame_count = 0;
	uint64_t backbuffer_index = 0;
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	printf("START\n");
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	while(Update()) {
		backbuffer_index = frame_count % FrameFifoMax;
		uint32_t present_index = 0;
		auto & ref = frame_info[backbuffer_index];
		for(int index = 0 ; auto & layer : ref.layer) {
			printf("ref.host_memory_addr=0x%p, buffer=0x%p\n", layer.host_memory_addr, layer.buffer);
		}
		
		//test update
		{
			for(auto & layer : ref.layer) {
				ObjectFormat *p = (ObjectFormat *)layer.host_memory_addr;
				for(int i = 0 ; i < ObjectMax; i++) {
					p->pos[0] = frandom();
					p->pos[1] = frandom();
					p->scale[0] = 0.1;
					p->scale[1] = 0.1;
					p->rotate[0] = frandom() * 4.0;
					
					p->color[0] = frand();
					p->color[1] = frand();
					p->color[2] = frand();
					p->color[3] = frand();
					p++;
				}
			}
		}
		vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &ref.fence);
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, ref.sem, VK_NULL_HANDLE, &present_index);
		printf("present_index=%d\n", present_index);
		submit_command(device, ref.cmdbuf, graphics_queue, ref.fence, ref.sem);
		present_surface(graphics_queue, swapchain, present_index);
		frame_count++;
		Sleep(0);
		if((frame_count % 60) == 0) {
			printf("frame_count=%lld\n", frame_count);
		}
	}
}
