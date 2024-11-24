/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

extern "C" {
#include "vitaGL.h"
#include "shared.h"
#ifdef HAVE_RAZOR
extern razor_results razor_metrics;
#endif
};

#ifdef HAVE_RAZOR_INTERFACE
#include <imgui_vita.h>
#endif

#ifdef HAVE_DEBUG_INTERFACE
#if !defined(HAVE_RAZOR_INTERFACE) || defined(HAVE_LIGHT_RAZOR)
#include "utils/font_utils.h"
int dbg_y = 8;
uint32_t *frame_buf;

void vgl_debugger_draw_character(int character, int x, int y) {
	for (int yy = 0; yy < 10; yy++) {
		int xDisplacement = x;
		int yDisplacement = (y + (yy<<1)) * DISPLAY_STRIDE;
		uint32_t* screenPos = frame_buf + xDisplacement + yDisplacement;

		uint8_t charPos = font[character * 10 + yy];
		for (int xx = 7; xx >= 2; xx--) {
			uint32_t clr = ((charPos >> xx) & 1) ? 0xFFFFFFFF : 0x00000000;
			*(screenPos) = clr;
			*(screenPos+1) = clr;
			*(screenPos+DISPLAY_STRIDE) = clr;
			*(screenPos+DISPLAY_STRIDE+1) = clr;			
			screenPos += 2;
		}
	}
}

void vgl_debugger_draw_string(int x, int y, const char *str) {
	for (size_t i = 0; i < strlen(str); i++)
		vgl_debugger_draw_character(str[i], x + i * 12, y);
}

void vgl_debugger_draw_string_format(int x, int y, const char *format, ...) {
	char str[512] = { 0 };
	va_list va;

	va_start(va, format);
	vsnprintf(str, 512, format, va);
	va_end(va);

	for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += 20)
		vgl_debugger_draw_string(x, y, text);
}
#endif

void vgl_debugger_draw_mem_usage(const char *str, vglMemType type) {
	uint32_t tot = vgl_mem_get_total_space(type) / (1024 * 1024);
	uint32_t used = tot - (vgl_mem_get_free_space(type) / (1024 * 1024));
	float ratio = ((float)used / (float)tot) * 100.0f;
#if defined(HAVE_RAZOR_INTERFACE) && !defined(HAVE_LIGHT_RAZOR)
	ImGui::Text("%s: %luMBs / %luMBs (%.2f%%)", str, used, tot, ratio);
#else
	vgl_debugger_draw_string_format(5, dbg_y, "%s: %luMBs / %luMBs (%.2f%%)", str, used, tot, ratio);
	dbg_y += 20;
#endif
}

#if !defined(HAVE_RAZOR_INTERFACE) || defined(HAVE_LIGHT_RAZOR)
void vgl_debugger_light_draw(uint32_t *fb) {
	frame_buf = fb;
	dbg_y = 8;
	vgl_debugger_draw_mem_usage("RAM Usage", VGL_MEM_RAM);
	vgl_debugger_draw_mem_usage("VRAM Usage", VGL_MEM_VRAM);
	vgl_debugger_draw_mem_usage("Phycont RAM Usage", VGL_MEM_SLOW);
	vgl_debugger_draw_mem_usage("CDLG RAM Usage", VGL_MEM_BUDGET);
#ifndef SKIP_ERROR_HANDLING
	vgl_debugger_draw_string_format(5, dbg_y, "Frame Number: %lu", vgl_framecount);
	dbg_y += 20;
#elif defined(HAVE_LIGHT_RAZOR)
	vgl_debugger_draw_string_format(5, dbg_y, "Frame Number: %lu", razor_metrics.frame_number);
	dbg_y += 20;
#endif
#ifdef HAVE_LIGHT_RAZOR
	vgl_debugger_draw_string_format(5, dbg_y, "GPU activity: %dus (%.0f%%)", razor_metrics.gpu_activity_duration_time, 100.f * razor_metrics.gpu_activity_duration_time / razor_metrics.frame_duration);
	vgl_debugger_draw_string_format(5, dbg_y + 20, "Partial Rendering: %s", razor_metrics.partial_render ? "Yes" : "No");
	vgl_debugger_draw_string_format(5, dbg_y + 40, "Param Buffer Outage: %s", razor_metrics.vertex_job_paused ? "Yes" : "No");
	vgl_debugger_draw_string_format(5, dbg_y + 60, "Param Buffer Peak Usage: %lu Bytes", razor_metrics.peak_usage_value);
#endif
}
#endif
#endif

#ifdef HAVE_RAZOR_INTERFACE
bool razor_dbg_window = true; // Current state for sceRazor debugger window
int metrics_mode = SCE_RAZOR_GPU_LIVE_METRICS_GROUP_PBUFFER_USAGE; // Current live metrics to show

#ifdef HAVE_DEVKIT
void vgl_debugger_set_metrics(int mode) {
	metrics_mode = mode;
	sceRazorGpuLiveStop();
	sceRazorGpuLiveSetMetricsGroup(metrics_mode);
	sceRazorGpuLiveStart();
	frame_idx = 0;
}
#endif

void vgl_debugger_init() {
#ifndef HAVE_LIGHT_RAZOR
	// Initializing dear ImGui
	ImGui::CreateContext();
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(GL_TRUE);
	ImGui_ImplVitaGL_UseIndirectFrontTouch(GL_TRUE);
	ImGui::StyleColorsDark();
#endif
}

void vgl_debugger_draw() {
#ifndef HAVE_LIGHT_RAZOR
	// Initializing a new ImGui frame
	ImGui_ImplVitaGL_NewFrame();
		
	// Drawing debugging window
	ImGui::SetNextWindowPos(ImVec2(150, 20), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiSetCond_Once);
	ImGui::Begin("vitaGL debugger", &razor_dbg_window);
	if (ImGui::Button("Perform GPU capture")) {
		SceDateTime date;
		sceRtcGetCurrentClockLocalTime(&date);
		char titleid[16];
		sceAppMgrAppParamGetString(0, 12, titleid , 256);
		char fname[256];
		sprintf(fname, "ux0:data/cap_%s-%02d_%02d_%04d-%02d_%02d_%02d.sgx", titleid, date.day, date.month, date.year, date.hour, date.minute, date.second);
		sceRazorGpuCaptureSetTriggerNextFrame(fname);
	}
#ifdef HAVE_DEVKIT
	if (has_razor_live) {
		if (ImGui::BeginMenu("Change metrics type")) {
			if (ImGui::MenuItem("Param Buffer", nullptr, metrics_mode == SCE_RAZOR_GPU_LIVE_METRICS_GROUP_PBUFFER_USAGE)) {
				vgl_debugger_set_metrics(SCE_RAZOR_GPU_LIVE_METRICS_GROUP_PBUFFER_USAGE);
			}
			if (ImGui::MenuItem("USSE", nullptr, metrics_mode == SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_1)) {
				vgl_debugger_set_metrics(SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_1);
			}
			if (ImGui::MenuItem("I/O", nullptr, metrics_mode == SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_2)) {
				vgl_debugger_set_metrics(SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_2);
			}
			if (ImGui::MenuItem("Memory", nullptr, metrics_mode == SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_3)) {
				vgl_debugger_set_metrics(SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_3);
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
	
		ImGui::Text("Frame number: %d", razor_metrics.frame_number);
		ImGui::Text("Frame duration: %dus", razor_metrics.frame_duration);
		ImGui::Text("GPU activity: %dus (%.0f%%)", razor_metrics.gpu_activity_duration_time, 100.f * razor_metrics.gpu_activity_duration_time / razor_metrics.frame_duration);
		ImGui::Text("Scenes per frame: %lu", razor_metrics.scene_count);
		ImGui::Separator();
	
		switch (metrics_mode) {
		case SCE_RAZOR_GPU_LIVE_METRICS_GROUP_PBUFFER_USAGE:
			ImGui::Text("Partial Rendering: %s", razor_metrics.partial_render ? "Yes" : "No");
			ImGui::Text("Param Buffer Outage: %s", razor_metrics.vertex_job_paused ? "Yes" : "No");
			ImGui::Text("Param Buffer Peak Usage: %lu Bytes", razor_metrics.peak_usage_value);
			break;
		case SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_1:
			ImGui::Text("Vertex jobs: %d (Time: %lluus)", razor_metrics.vertex_job_count, razor_metrics.vertex_job_time / 4);
			ImGui::Text("USSE Vertex Processing: %.2f%%", razor_metrics.usse_vertex_processing_percent / razor_metrics.vertex_job_count);
			ImGui::Separator();
			ImGui::Text("Fragment jobs: %d (Time: %lluus)", razor_metrics.fragment_job_count, razor_metrics.fragment_job_time / 4);
			ImGui::Text("USSE Fragment Processing: %.2f%%", razor_metrics.usse_fragment_processing_percent / razor_metrics.fragment_job_count);
			ImGui::Separator();
			ImGui::Text("USSE Dependent Texture Read: %.2f%%", razor_metrics.usse_dependent_texture_reads_percent / razor_metrics.fragment_job_count);
			ImGui::Text("USSE Non-Dependent Texture Read: %.2f%%", razor_metrics.usse_non_dependent_texture_reads_percent / razor_metrics.fragment_job_count);
			ImGui::Separator();
			ImGui::Text("Firmware jobs: %d (Time: %lluus)", razor_metrics.firmware_job_count, razor_metrics.firmware_job_time / 4);
			break;
		case SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_2:
			ImGui::Text("Vertex jobs: %d (Time: %lluus)", razor_metrics.vertex_job_count, razor_metrics.vertex_job_time / 4);
			ImGui::Text("VDM primitives (Input): %d", razor_metrics.vdm_primitives_input_num);
			ImGui::Text("MTE primitives (Output): %d", razor_metrics.mte_primitives_output_num);
			ImGui::Text("VDM vertices (Input): %d", razor_metrics.vdm_vertices_input_num);
			ImGui::Text("MTE vertices (Output): %d", razor_metrics.mte_vertices_output_num);
			ImGui::Separator();
			ImGui::Text("Fragment jobs: %d (Time: %lluus)", razor_metrics.fragment_job_count, razor_metrics.fragment_job_time / 4);
			ImGui::Text("Rasterized pixels before HSR: %d", razor_metrics.rasterized_pixels_before_hsr_num);
			ImGui::Text("Rasterized output pixels: %d", razor_metrics.rasterized_output_pixels_num);
			ImGui::Text("Rasterized output samples: %d", razor_metrics.rasterized_output_samples_num);
			ImGui::Separator();
			ImGui::Text("Firmware jobs: %d (Time: %lluus)", razor_metrics.firmware_job_count, razor_metrics.firmware_job_time / 4);
			break;
		case SCE_RAZOR_GPU_LIVE_METRICS_GROUP_OVERVIEW_3:
			ImGui::Text("Vertex jobs: %d (Time: %lluus)", razor_metrics.vertex_job_count, razor_metrics.vertex_job_time / 4);
			ImGui::Text("BIF: Tiling accelerated memory writes: %d bytes", razor_metrics.tiling_accelerated_mem_writes);
			ImGui::Separator();
			ImGui::Text("Fragment jobs: %d (Time: %lluus)", razor_metrics.fragment_job_count, razor_metrics.fragment_job_time / 4);
			ImGui::Text("BIF: ISP parameter fetch memory reads: %d bytes", razor_metrics.isp_parameter_fetches_mem_reads);
			ImGui::Separator();
			ImGui::Text("Firmware jobs: %d (Time: %lluus)", razor_metrics.firmware_job_count, razor_metrics.firmware_job_time / 4);
			break;
		default:
			break;
		}
		ImGui::Separator();
	}
#endif
	
	vgl_debugger_draw_mem_usage("RAM Usage", VGL_MEM_RAM);
	vgl_debugger_draw_mem_usage("VRAM Usage", VGL_MEM_VRAM);
	vgl_debugger_draw_mem_usage("Phycont RAM Usage", VGL_MEM_SLOW);
	vgl_debugger_draw_mem_usage("CDLG RAM Usage", VGL_MEM_BUDGET);
		
	ImGui::End();
	
	// Invalidating current GL machine state
	GLuint program_bkp = 0;
	if (cur_program) {
		program_bkp = cur_program;
		glUseProgram(0);
	}
	GLboolean blend_state_bkp = blend_state;
	SceGxmBlendFactor sfactor_bkp = blend_sfactor_rgb;
	SceGxmBlendFactor dfactor_bkp = blend_dfactor_rgb;
	GLboolean cull_face_state_bkp = cull_face_state;
	GLboolean depth_test_state_bkp = depth_test_state;
	GLboolean scissor_test_state_bkp = scissor_test_state;
	
	// Sending job to ImGui renderer
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	
	// Restoring invalidated GL machine state
	if (program_bkp)
		glUseProgram(program_bkp);
	blend_sfactor_rgb = blend_sfactor_a = sfactor_bkp;
	blend_dfactor_rgb = blend_dfactor_a = dfactor_bkp;
	if (!blend_state_bkp)
		glDisable(GL_BLEND);
	else
		change_blend_factor();
	if (!scissor_test_state_bkp)
		glDisable(GL_SCISSOR_TEST);
	if (depth_test_state_bkp)
		glEnable(GL_DEPTH_TEST);
	if (cull_face_state_bkp)
		glEnable(GL_CULL_FACE);
#endif
}
#endif

#ifdef FILE_LOG
static char msg[512 * 1024];
void vgl_file_log(const char *format, ...) {
	__gnuc_va_list arg;
	va_start(arg, format);
	vsnprintf(msg, sizeof(msg), format, arg);
	va_end(arg);
	SceUID log = sceIoOpen("ux0:/data/vitaGL.log", SCE_O_WRONLY | SCE_O_APPEND | SCE_O_CREAT, 0777);
	if (log >= 0) {
		sceIoWrite(log, msg, strlen(msg));
		sceIoClose(log);
	}
}
#endif

#ifdef LOG_ERRORS
#define ERROR_CASE(x) \
	case x: \
		return "##x";
char *get_gxm_error_literal(uint32_t code) {
	switch (code) {
	ERROR_CASE(SCE_GXM_ERROR_UNINITIALIZED)
	ERROR_CASE(SCE_GXM_ERROR_ALREADY_INITIALIZED)
	ERROR_CASE(SCE_GXM_ERROR_OUT_OF_MEMORY)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_VALUE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_POINTER)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_ALIGNMENT)
	ERROR_CASE(SCE_GXM_ERROR_NOT_WITHIN_SCENE)
	ERROR_CASE(SCE_GXM_ERROR_WITHIN_SCENE)
	ERROR_CASE(SCE_GXM_ERROR_NULL_PROGRAM)
	ERROR_CASE(SCE_GXM_ERROR_UNSUPPORTED)
	ERROR_CASE(SCE_GXM_ERROR_PATCHER_INTERNAL)
	ERROR_CASE(SCE_GXM_ERROR_RESERVE_FAILED)
	ERROR_CASE(SCE_GXM_ERROR_PROGRAM_IN_USE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_INDEX_COUNT)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_POLYGON_MODE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_COMPONENT_COUNT)
	ERROR_CASE(SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_AUXILIARY_SURFACE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_PRECOMPUTED_DRAW)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_PRECOMPUTED_VERTEX_STATE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_PRECOMPUTED_FRAGMENT_STATE)
	ERROR_CASE(SCE_GXM_ERROR_DRIVER)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_TEXTURE)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER)
	ERROR_CASE(SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER)
	ERROR_CASE(SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS)
	default:
		return "Unknown Error";
	}
}
#endif
