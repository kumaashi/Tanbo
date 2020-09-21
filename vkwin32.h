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
	std::vector<unsigned char> &vdata)
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


#ifdef VKWIN32_DEBUG
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
#endif //VKWIN32_DEBUG


[[ nodiscard ]] static VkInstance
create_instance(const char *appname)
{
	const char *vinstance_ext_names[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,


#ifdef VKWIN32_DEBUG
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif //VKWIN32_DEBUG
	};
	VkInstance inst = VK_NULL_HANDLE;
	VkApplicationInfo vkapp = {};
	VkInstanceCreateInfo inst_info = {};
	vkapp.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

#ifdef VKWIN32_DEBUG
	VkDebugReportCallbackCreateInfoEXT drcc_info = {};
	vkapp.pNext = &drcc_info;
#endif //VKWIN32_DEBUG
	vkapp.pApplicationName = appname;
	vkapp.pEngineName = appname;
	vkapp.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	vkapp.apiVersion = VK_API_VERSION_1_0;
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext = NULL;
	inst_info.pApplicationInfo = &vkapp;
	inst_info.enabledExtensionCount = (uint32_t)_countof(vinstance_ext_names);
	inst_info.ppEnabledExtensionNames = vinstance_ext_names;
#ifdef VKWIN32_DEBUG
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
#endif //VKWIN32_DEBUG
	auto err = vkCreateInstance(&inst_info, NULL, &inst);

#ifdef VKWIN32_DEBUG
	bind_debug_fn(inst, drcc_info);
#endif //VKWIN32_DEBUG

	return inst;
}

[[ nodiscard ]]
inline VkDevice
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
	return (ret);
}

[[ nodiscard ]]
inline uint32_t
get_graphics_queue_index(VkPhysicalDevice gpudev)
{
	uint32_t ret = 0;
	uint32_t cnt = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &cnt, nullptr);
	std::vector<VkQueueFamilyProperties> temp(cnt);
	vkGetPhysicalDeviceQueueFamilyProperties(gpudev, &cnt, temp.data());
	for (uint32_t i = 0; i < cnt; i++) {
		auto flags = temp[i].queueFlags;
		if (flags & VK_QUEUE_GRAPHICS_BIT) {
			ret = i;
			break;
		}
	}
	return (ret);
}

[[ nodiscard ]]
inline uint32_t
get_buffer_memreq_size(VkDevice device, VkBuffer buffer)
{
	VkMemoryRequirements memreqs = {};

	vkGetBufferMemoryRequirements(device, buffer, &memreqs);
	memreqs.size = memreqs.size + (memreqs.alignment - 1);
	memreqs.size &= ~(memreqs.alignment - 1);
	return (memreqs.size);
}

[[ nodiscard ]]
inline uint32_t
get_image_memreq_size(VkDevice device, VkImage image)
{
	VkMemoryRequirements memreqs = {};

	vkGetImageMemoryRequirements(device, image, &memreqs);
	memreqs.size = memreqs.size + (memreqs.alignment - 1);
	memreqs.size &= ~(memreqs.alignment - 1);
	return (memreqs.size);
}


[[ nodiscard ]]
inline VkSampler
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

[[ nodiscard ]]
inline VkCommandPool
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

[[ nodiscard ]]
inline VkCommandBuffer
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

[[ nodiscard ]]
inline VkDeviceMemory
alloc_device_memory(
	VkPhysicalDevice gpudev,
	VkDevice device,
	VkDeviceSize size,
	bool is_host)
{
	VkDeviceMemory ret = VK_NULL_HANDLE;
	VkMemoryAllocateInfo ma_info = {};
	VkPhysicalDeviceMemoryProperties devprop = {};
	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	ma_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ma_info.allocationSize = size;
	vkGetPhysicalDeviceMemoryProperties(gpudev, &devprop);

	if (is_host) {
		flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}
	for (uint32_t i = 0; i < devprop.memoryTypeCount; i++) {
		if ((devprop.memoryTypes[i].propertyFlags & flags) == flags) {
			ma_info.memoryTypeIndex = i;
			break;
		}
	}
	vkAllocateMemory(device, &ma_info, nullptr, &ret);
	return (ret);
};

[[ nodiscard ]]
inline VkSemaphore
create_semaphore(VkDevice device)
{
	VkSemaphore ret = VK_NULL_HANDLE;
	VkSemaphoreCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(device, &info, nullptr, &ret);
	return (ret);
}


[[ nodiscard ]]
inline VkFence
create_fence(VkDevice device)
{
	VkFence ret = VK_NULL_HANDLE;
	VkFenceCreateInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(device, &info, NULL, &ret);

	return (ret);
}

[[ nodiscard ]]
inline VkSurfaceKHR
create_win32_surface(VkInstance inst, HWND hwnd, HINSTANCE hinst)
{
	VkSurfaceKHR ret = VK_NULL_HANDLE;
	VkWin32SurfaceCreateInfoKHR info = {};

	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = hinst;
	info.hwnd = hwnd;
	vkCreateWin32SurfaceKHR(inst, &info, NULL, &ret);
	return (ret);
}

[[ nodiscard ]]
inline VkSwapchainKHR
create_swapchain(
	VkDevice device,
	VkSurfaceKHR surface,
	uint32_t width, uint32_t height,
	uint32_t fifomax)
{
	VkSwapchainKHR ret = VK_NULL_HANDLE;
	VkSwapchainCreateInfoKHR info = {};

	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = fifomax;
	info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	info.imageExtent.width = width;
	info.imageExtent.height = height;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	//info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	info.clipped = VK_TRUE;
	vkCreateSwapchainKHR(device, &info, nullptr, &ret);
	return (ret);
}

inline void
submit_command(
	VkDevice device,
	VkCommandBuffer cmdbuf,
	VkQueue queue,
	VkFence fence,
	VkSemaphore sem)
{
	VkSubmitInfo info = {};
	VkPipelineStageFlags wait_mask[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	VkSemaphore waitSemaphores[] = { sem };

	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.waitSemaphoreCount = 1;
	info.pWaitDstStageMask = wait_mask;
	info.commandBufferCount = 1;
	info.pWaitSemaphores = waitSemaphores;
	info.pCommandBuffers = &cmdbuf;
	info.signalSemaphoreCount = 0;

	vkResetFences(device, 1, &fence);
	vkQueueSubmit(queue, 1, &info, fence);
}

inline void
present_surface(
	VkQueue queue,
	VkSwapchainKHR swapchain,
	uint32_t present_index)
{
	VkPresentInfoKHR info = {};

	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pSwapchains = &swapchain;
	info.swapchainCount = 1;
	info.pImageIndices = &present_index;
	vkQueuePresentKHR(queue, &info);
}

[[ nodiscard ]]
inline void
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
	ret.subresourceRange = {
		aspectMask,
		baseMipLevel, levelCount,
		baseArrayLayer, layerCount
	};
	vkCmdPipelineBarrier(cmdbuf,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &ret);
}

[[ nodiscard ]]
inline VkBuffer
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
inline VkImage
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
	info.usage = usageFlags |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkCreateImage(device, &info, NULL, &ret);
	if (ret && pinfo)
		*pinfo = info;
	return (ret);
}

[[ nodiscard ]]
inline VkImageView
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
inline VkRenderPass
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
	VkAttachmentDescription col_attachment = {};

	col_attachment.flags = 0;
	col_attachment.format = color_format;
	col_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	col_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	col_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	col_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	col_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	col_attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	col_attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkAttachmentReference color_reference = {};
	color_reference.attachment = 0;
	color_reference.layout = VK_IMAGE_LAYOUT_GENERAL;

	for (int i = 0 ; i < color_num; i++) {
		auto ref = color_reference;
		ref.attachment = attachment_index;
		vattachments.push_back(col_attachment);
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
inline VkFramebuffer
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
inline VkDescriptorPool
create_descriptor_pool(
	VkDevice device, uint32_t cnt, uint32_t max_sets = 0xFF)
{
	VkDescriptorPool ret = nullptr;
	VkDescriptorPoolCreateInfo info = {};
	std::vector<VkDescriptorPoolSize> vpoolsizes;

	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, cnt});
	vpoolsizes.push_back({VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, cnt});

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
inline VkDescriptorSetLayout
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
inline VkPipelineLayout
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
inline VkDescriptorSet
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
inline VkShaderModule
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
inline VkPipeline
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
inline VkPipeline
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

	//att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	//att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;        //
	////VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
	//att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
	//att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	//att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;

	att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
	att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;


	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = att_state;

	//SETUP DS
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_FALSE;
	ds.depthWriteEnable = VK_FALSE;
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
		auto module = create_shader_module(
				device, vs.data(), vs.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		sstage.module = module;
		vsstageinfo.push_back(sstage);
	}
	if (!ps.empty()) {
		auto module = create_shader_module(
				device, ps.data(), ps.size());
		vshadermodules.push_back(module);
		sstage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		sstage.module = module;
		vsstageinfo.push_back(sstage);
	}

	//SETUP IA
	uint32_t stride_size = 0;
	std::vector<VkVertexInputAttributeDescription> via_desc;
	via_desc.push_back({0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;
	via_desc.push_back({1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
	stride_size += sizeof(float) * 4;
	via_desc.push_back({2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, stride_size});
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
	vi.vertexAttributeDescriptionCount = via_desc.size();
	vi.pVertexAttributeDescriptions = via_desc.data();

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

inline void
update_descriptor_sets(
	VkDevice device,
	VkDescriptorSet descriptor_sets,
	const void *pinfo,
	uint32_t binding,
	uint32_t index,
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

inline void
update_descriptor_combined_image_sample(
	VkDevice device,
	VkDescriptorSet dset,
	uint32_t binding,
	uint32_t index,
	VkImageView image_view,
	VkSampler sampler)
{
	VkDescriptorImageInfo info = {};
	info.sampler = sampler;
	info.imageView = image_view;
	info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	update_descriptor_sets(device, dset, &info,
		binding, index, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

inline void
update_descriptor_storage_buffer(
	VkDevice device,
	VkDescriptorSet dset,
	uint32_t binding,
	uint32_t index,
	VkBuffer buf,
	VkDeviceSize bytes)
{
	VkDescriptorBufferInfo info = {};

	info.buffer = buf;
	info.offset = 0;
	info.range = bytes;
	update_descriptor_sets(device, dset, &info,
		binding, index,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

inline void
cmd_clear_image(
	VkCommandBuffer cmdbuf,
	VkImage image,
	float r, float g, float b, float a)
{
	VkClearColorValue clear_color = {};
	clear_color.float32[0] = r;
	clear_color.float32[1] = g;
	clear_color.float32[2] = b;
	clear_color.float32[3] = a;
	VkImageSubresourceRange image_range_color = {
		VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
	};
	vkCmdClearColorImage(cmdbuf, image,
		VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &image_range_color);
}

inline void
cmd_set_viewport(
	VkCommandBuffer cmdbuf,
	float x, float y, float w, float h)
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

inline void
cmd_begin_render_pass(
	VkCommandBuffer cmdbuf,
	VkRenderPass rp,
	VkFramebuffer framebuffer,
	uint32_t width, uint32_t height)
{
	VkRenderPassBeginInfo info = {};

	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = rp;
	info.framebuffer = framebuffer;
	info.renderArea.extent.width = width;
	info.renderArea.extent.height = height;
	vkCmdBeginRenderPass(cmdbuf, &info, VK_SUBPASS_CONTENTS_INLINE);
}

inline void
cmd_end_render_pass(
	VkCommandBuffer cmdbuf)
{
	vkCmdEndRenderPass(cmdbuf);
}

inline void
cmd_blit_image(
	VkCommandBuffer cmdbuf,
	VkImage dst,
	VkImage src,
	uint32_t dst_w, uint32_t dst_h,
	uint32_t src_w, uint32_t src_h)
{
	VkImageBlit image_blit = {};

	image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.srcSubresource.layerCount = 1;
	image_blit.srcOffsets[1].x = int32_t(src_w);
	image_blit.srcOffsets[1].y = int32_t(src_h);
	image_blit.srcOffsets[1].z = 1;

	image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_blit.dstSubresource.layerCount = 1;
	image_blit.dstOffsets[1].x = int32_t(dst_w);
	image_blit.dstOffsets[1].y = int32_t(dst_h);
	image_blit.dstOffsets[1].z = 1;
	vkCmdBlitImage(cmdbuf,
		src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &image_blit, VK_FILTER_LINEAR);
}

