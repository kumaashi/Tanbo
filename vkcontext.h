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

#include "vkwin32.h"

struct vkcontext_t {
	struct vertex_format {
		float pos[4];
		float uv[4];
		float color[4];
	};

	struct object_format {
		float pos[4];
		float scale[4];
		float rotate[4];
		float color[4];
		uint32_t metadata[16];
	};

	struct create_info {
		const char *appname;
		HWND hwnd;
		HINSTANCE hinst;
		uint32_t ScreenW;
		uint32_t ScreenH;
		uint32_t FrameFifoMax;
		uint32_t Width;
		uint32_t Height;
		uint32_t BitsSize;
		uint32_t ObjectMax;
		uint32_t LayerMax;
		uint32_t DescriptorArrayMax;
		uint32_t DescriptorPoolMax;
		uint32_t DrawIndirectCommandSize;
		uint64_t GpuMemoryMax;
		uint64_t ObjectMaxBytes;
		uint64_t VertexMaxBytes;
		std::vector<uint8_t> cs_update;
		struct shader_layer_t {
			std::vector<uint8_t> vs;
			std::vector<uint8_t> ps;
		};
		std::vector<shader_layer_t> shader_layers;
	};

	struct frame_infos_t {
		VkImage backbuffer_image = VK_NULL_HANDLE;
		VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
		VkSemaphore sem = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;

		VkDescriptorSet descriptor_set_cbv = VK_NULL_HANDLE;
		VkDescriptorSet descriptor_set_srv = VK_NULL_HANDLE;
		
		VkDeviceMemory devmem_host = VK_NULL_HANDLE;
		VkDeviceMemory devmem_local_vertex = VK_NULL_HANDLE;

		uint64_t devmem_host_offset = 0;
		uint64_t devmem_local_vertex_offset = 0;

		VkBuffer indirect_draw_cmd_buffer = VK_NULL_HANDLE;
		VkDrawIndirectCommand *host_draw_indirect_cmd = nullptr;
		VkDeviceMemory devmem_host_draw_indirect_cmd = VK_NULL_HANDLE;

		struct layer_t {
			VkDescriptorSet descriptor_set_uav = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView image_view = VK_NULL_HANDLE;
			VkFramebuffer framebuffer = VK_NULL_HANDLE;
			VkBuffer buffer = VK_NULL_HANDLE;
			void *host_memory_addr = nullptr;
			VkBuffer vertex_buffer = VK_NULL_HANDLE;
		};
		std::vector<layer_t> layers;
	};

	enum {
		RDT_SLOT_SRV = 0,
		RDT_SLOT_CBV,
		RDT_SLOT_UAV,
		RDT_SLOT_MAX,
	};

	uint32_t graphics_queue_family_index = -1;
	uint32_t gpu_count = 0;
	VkPhysicalDevice gpudev = VK_NULL_HANDLE;
	VkInstance inst = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties gpu_props = {};
	VkBool32 presentSupport = false;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkCommandPool cmd_pool = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkQueue graphics_queue = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDeviceMemory devmem_host = VK_NULL_HANDLE;
	VkDeviceMemory devmem_local = VK_NULL_HANDLE;
	uint64_t devmem_local_offset = 0;
	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> vdescriptor_layouts;
	VkRenderPass render_pass = VK_NULL_HANDLE;
	VkPipeline cp_update_buffer = VK_NULL_HANDLE;
	std::vector<VkPipeline> vgp_draw_rects;
	std::vector<frame_infos_t> frame_infos;
	
	void init(create_info & info) {
		frame_infos.resize(info.FrameFifoMax);
		VkInstance inst = create_instance(info.appname);
		auto err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
		err = vkEnumeratePhysicalDevices(inst, &gpu_count, &gpudev);
		vkGetPhysicalDeviceProperties(gpudev, &gpu_props);

		surface = create_win32_surface(inst, info.hwnd, GetModuleHandle(NULL));
		vkGetPhysicalDeviceSurfaceSupportKHR(gpudev, 0, surface, &presentSupport);
		VkSurfaceCapabilitiesKHR capabilities = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpudev, surface, &capabilities);

		graphics_queue_family_index = get_graphics_queue_index(gpudev);
		device = create_device(gpudev, graphics_queue_family_index);
		cmd_pool = create_cmd_pool(device, graphics_queue_family_index);
		swapchain = create_swapchain(device, surface, info.ScreenW, info.ScreenH, info.FrameFifoMax);
		sampler = create_sampler(device, true);
		vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
		std::vector<VkImage> temp;
		{
			uint32_t swapchain_count = 0;
			vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, nullptr);
			temp.resize(swapchain_count);
			vkGetSwapchainImagesKHR(device, swapchain, &swapchain_count, temp.data());
		}

		devmem_local = alloc_device_memory(gpudev, device, info.GpuMemoryMax, false);
		descriptor_pool = create_descriptor_pool(device, info.DescriptorPoolMax);
		{
			std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_srv;
			std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_cbv;
			std::vector<VkDescriptorSetLayoutBinding> vdesc_setlayout_binding_uav;
			VkShaderStageFlags shader_stages = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
			vdesc_setlayout_binding_srv.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.DescriptorArrayMax, shader_stages, nullptr});
			vdesc_setlayout_binding_cbv.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info.DescriptorArrayMax, shader_stages, nullptr});
			vdesc_setlayout_binding_uav.push_back({0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, shader_stages, nullptr});
			vdesc_setlayout_binding_uav.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, shader_stages, nullptr});

			vdescriptor_layouts.resize(RDT_SLOT_MAX);
			vdescriptor_layouts[RDT_SLOT_SRV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_srv);
			vdescriptor_layouts[RDT_SLOT_CBV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_cbv);
			vdescriptor_layouts[RDT_SLOT_UAV] = create_descriptor_set_layout(device, vdesc_setlayout_binding_uav);
			pipeline_layout = create_pipeline_layout(device, vdescriptor_layouts.data(), vdescriptor_layouts.size());
		}
		render_pass = create_render_pass(device, VK_FORMAT_R8G8B8A8_UNORM);
		cp_update_buffer = create_cpipeline(device, pipeline_layout, info.cs_update);
		vgp_draw_rects.resize(info.LayerMax);
		for (int i = 0 ; i < info.LayerMax; i++) {
			auto & shader = info.shader_layers[i];
			vgp_draw_rects[i] = create_gpipeline(device, pipeline_layout, render_pass, shader.vs, shader.ps);
		}

		for (int i = 0 ; i < info.FrameFifoMax; i++) {
			auto & ref = frame_infos[i];
			ref.layers.resize(info.LayerMax);
			uint8_t *temp_addr = nullptr;
			ref.backbuffer_image = temp[i];
			ref.fence = create_fence(device);
			ref.sem = create_semaphore(device);
			ref.devmem_host = alloc_device_memory(gpudev, device, info.LayerMax * info.ObjectMaxBytes, true);
			ref.devmem_local_vertex = alloc_device_memory(gpudev, device, info.LayerMax * info.VertexMaxBytes, false);
			ref.devmem_host_draw_indirect_cmd = alloc_device_memory(gpudev, device, info.DrawIndirectCommandSize, true);
			ref.indirect_draw_cmd_buffer = create_buffer(device, info.DrawIndirectCommandSize);
			vkBindBufferMemory(device, ref.indirect_draw_cmd_buffer, ref.devmem_host_draw_indirect_cmd, 0);
			vkMapMemory(device, ref.devmem_host, 0, info.LayerMax * info.ObjectMaxBytes, 0, (void **)&temp_addr);
			vkMapMemory(device, ref.devmem_host_draw_indirect_cmd, 0, info.DrawIndirectCommandSize, 0, (void **)&ref.host_draw_indirect_cmd);
			
			auto image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			ref.descriptor_set_srv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_SRV]);
			ref.descriptor_set_cbv = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_CBV]);
			for (auto & layer : ref.layers) {
				layer.descriptor_set_uav = create_descriptor_set(device, descriptor_pool, vdescriptor_layouts[RDT_SLOT_UAV]);
				layer.image = create_image(device, info.Width, info.Height, VK_FORMAT_R8G8B8A8_UNORM, image_usage_flags);
				layer.buffer = create_buffer(device, info.ObjectMaxBytes);
				layer.vertex_buffer = create_buffer(device, info.VertexMaxBytes);
				vkBindImageMemory(device, layer.image, devmem_local, devmem_local_offset);
				vkBindBufferMemory(device, layer.buffer, ref.devmem_host, ref.devmem_host_offset);
				vkBindBufferMemory(device, layer.vertex_buffer, ref.devmem_local_vertex, ref.devmem_local_vertex_offset);

				devmem_local_offset += get_image_memreq_size(device, layer.image);
				ref.devmem_host_offset += get_buffer_memreq_size(device, layer.buffer);
				ref.devmem_local_vertex_offset += get_buffer_memreq_size(device, layer.vertex_buffer);
				layer.host_memory_addr = temp_addr;
				temp_addr += get_buffer_memreq_size(device, layer.buffer);
			}

			for (uint32_t layer_num = 0 ; layer_num < ref.layers.size(); layer_num++) {
				auto & layer = ref.layers[layer_num];
				layer.image_view = create_image_view(device, layer.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
				std::vector<VkImageView> vimageview = { layer.image_view, };
				layer.framebuffer = create_framebuffer(device, render_pass, vimageview, info.Width, info.Height);
				update_descriptor_combined_image_sample(device, ref.descriptor_set_srv, 0, layer_num, layer.image_view, sampler);
				update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 0, 0, layer.buffer, info.ObjectMaxBytes);
				update_descriptor_storage_buffer(device, layer.descriptor_set_uav, 1, 0, layer.vertex_buffer, info.VertexMaxBytes);
			}

			ref.cmdbuf = create_command_buffer(device, cmd_pool);
			vkResetCommandBuffer(ref.cmdbuf, 0);
			VkCommandBufferBeginInfo cmdbegininfo = {};
			cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			vkBeginCommandBuffer(ref.cmdbuf, &cmdbegininfo);
			for (uint32_t layer_num = 0 ; layer_num < ref.layers.size(); layer_num++) {
				auto & layer = ref.layers[layer_num];
				VkDeviceSize vertex_offsets[1] = {0};
				std::vector<VkDescriptorSet> vdescriptor_sets = {
					ref.descriptor_set_srv,
					ref.descriptor_set_cbv,
					layer.descriptor_set_uav,
				};
				vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, cp_update_buffer);
				vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
				vkCmdDispatch(ref.cmdbuf, info.ObjectMax, 1, 1);

				cmd_set_viewport(ref.cmdbuf, 0, 0, info.Width, info.Height);
				set_image_memory_barrier(ref.cmdbuf, layer.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
				cmd_clear_image(ref.cmdbuf, layer.image, 0, 0, 0, 0);
				vkCmdBindPipeline(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vgp_draw_rects[layer_num]);
				vkCmdBindDescriptorSets(ref.cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, vdescriptor_sets.size(), vdescriptor_sets.data(), 0, NULL);
				vkCmdBindVertexBuffers(ref.cmdbuf, 0, 1, &layer.vertex_buffer, vertex_offsets);
				cmd_begin_render_pass(ref.cmdbuf, render_pass, layer.framebuffer, info.Width, info.Height);
				vkCmdDrawIndirect(ref.cmdbuf, ref.indirect_draw_cmd_buffer, sizeof(VkDrawIndirectCommand) * layer_num, 1, sizeof(VkDrawIndirectCommand));
				cmd_end_render_pass(ref.cmdbuf);
			}
			auto & last_layer = ref.layers[info.LayerMax - 1];
			set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			cmd_clear_image(ref.cmdbuf, ref.backbuffer_image, 0, 0, 0, 0);
			set_image_memory_barrier(ref.cmdbuf, last_layer.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			cmd_blit_image(ref.cmdbuf, ref.backbuffer_image, last_layer.image, info.ScreenW, info.ScreenH, info.Width, info.Height);
			set_image_memory_barrier(ref.cmdbuf, last_layer.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			set_image_memory_barrier(ref.cmdbuf, ref.backbuffer_image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			vkEndCommandBuffer(ref.cmdbuf);
		}
	}
	
	void submit(uint32_t index) {
		auto & ref = frame_infos[index];
		vkWaitForFences(device, 1, &ref.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &ref.fence);
		uint32_t present_index = 0;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, ref.sem, VK_NULL_HANDLE, &present_index);
		//printf("SUBMIT : present_index=%d\r", present_index);
		submit_command(device, ref.cmdbuf, graphics_queue, ref.fence, ref.sem);
		present_surface(graphics_queue, swapchain, present_index);
	}
};
