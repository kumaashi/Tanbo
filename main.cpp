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

[[ nodiscard ]] static VkInstance
create_instance(const char *appname) {
	const char *vinstance_ext_names[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	};
	VkInstance inst = VK_NULL_HANDLE;
	VkApplicationInfo vkapp = {};
	VkInstanceCreateInfo inst_info = {};
	vkapp.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkapp.pApplicationName = appname;
	vkapp.pEngineName = appname;
	vkapp.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	vkapp.apiVersion = VK_API_VERSION_1_0;
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext = NULL;
	inst_info.pApplicationInfo = &vkapp;
	inst_info.enabledExtensionCount = (uint32_t)_countof(vinstance_ext_names);
	inst_info.ppEnabledExtensionNames = vinstance_ext_names;
	auto err = vkCreateInstance(&inst_info, NULL, &inst);
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
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(device, &info, nullptr, &ret);
	if (ret && pinfo)
		*pinfo = info;

	return (ret);
}

int main(int argc, char *argv[]) {
	const char *appname = argv[0];
	enum {
		Width = 640,
		Height = 480,
		FrameFifoMax = 2,
	};

	struct frame_info_t {
		enum {
			LayerMax = 4,
		};
		VkImage image = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		VkSemaphore sem = VK_NULL_HANDLE;
		VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
		VkDeviceMemory devmem_host = VK_NULL_HANDLE;
		VkDeviceMemory devmem_local = VK_NULL_HANDLE;
		VkImage image_layer[LayerMax];
	} frame_info[FrameFifoMax];

	uint32_t gpu_count = 0;
	uint32_t queue_family_count = 0;
	uint32_t graphics_queue_family_index = -1;
	static auto hwnd = InitWindow("Tanbo", Width, Height);
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
	uint32_t swapchain_count = 0;
	std::vector<VkImage> temp;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, nullptr);
	temp.resize(swapchain_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, temp.data());
	enum {
		GpuMemoryMax = 1024 * 1024 * 1024,
	};
	for(int i = 0 ; i < FrameFifoMax; i++) {
		auto & ref = frame_info[i];
		ref.cmdbuf = create_command_buffer(device, cmd_pool);
		ref.fence = create_fence(device);
		ref.sem = create_semaphore(device);
		ref.devmem_host = alloc_device_memory(gpudev, device, GpuMemoryMax, true);
		ref.devmem_local = alloc_device_memory(gpudev, device, GpuMemoryMax, false);
		
		//added command
		vkResetCommandBuffer(ref.cmdbuf, 0);
		VkCommandBufferBeginInfo cmdbegininfo = {};
		cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);
		
		ref.image = temp[i];
		set_image_memory_barrier(ref.cmdbuf, ref.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		VkClearColorValue clearColor = {};
		clearColor.float32[0] = !i;
		clearColor.float32[1] = !!i;
		clearColor.float32[2] = !i;
		clearColor.float32[3] = !!i;
		VkImageSubresourceRange image_range_color = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdClearColorImage(ref.cmdbuf, ref.image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_range_color);
		set_image_memory_barrier(ref.cmdbuf, ref.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkEndCommandBuffer(ref.cmdbuf);
		printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
		printf("frame=%d\n", i);
		printf("ref.image=%p\n", ref.image);
		printf("ref.cmdbuf=%p\n", ref.cmdbuf);
		printf("ref.fence=%p\n", ref.fence);
		printf("ref.sem=%p\n", ref.sem);
		printf("ref.devmem_host=%p\n", ref.devmem_host);
		printf("ref.devmem_local=%p\n", ref.devmem_local);
	}
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");
	printf("inst=%p\n", inst);
	printf("gpudev=%p\n", gpudev);
	printf("minTexelBufferOffsetAlignment  =%p\n", (void *)gpu_props.limits.minTexelBufferOffsetAlignment);
	printf("minUniformBufferOffsetAlignment=%p\n", (void *)gpu_props.limits.minUniformBufferOffsetAlignment);
	printf("minStorageBufferOffsetAlignment=%p\n", (void *)gpu_props.limits.minStorageBufferOffsetAlignment);
	printf("graphics_queue_family_index=%d\n", graphics_queue_family_index);
	printf("queue_family_count=%d\n", queue_family_count);
	printf("gpudev=%p\n", gpudev);
	printf("device=%p\n", device);
	printf("surface=%p\n", surface);
	printf("swapchain=%p\n", swapchain);
	printf("graphics_queue=%p\n", graphics_queue);
	printf("cmd_pool=%p\n", cmd_pool);
	printf("=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====8<=====\n");

	uint64_t frame_count = 0;
	uint64_t backbuffer_index = 0;
	while(Update()) {
		uint32_t present_index = 0;
		auto & ref = frame_info[backbuffer_index];
		vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &ref.fence);
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, ref.sem, VK_NULL_HANDLE, &present_index);
		submit_command(device, ref.cmdbuf, graphics_queue, ref.fence, ref.sem);
		present_surface(graphics_queue, swapchain, present_index);
		backbuffer_index = frame_count % FrameFifoMax;
		frame_count++;
		Sleep(0);
		if((frame_count % 60) == 0)
			printf("frame_count=%lld\n", frame_count);
	}
}
