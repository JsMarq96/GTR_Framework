#pragma once
#include "includes.h"
#include "texture.h"
#include "shader.h"
#include "mesh.h"
#include "fbo.h"
#include "scene.h"

namespace GTR {
	
	enum ePostFX : int {
		CHROM_ABERR = 0,
		VIGNET,
		LUT,
		GRAIN,
		POSTFX_COUNT
	};
	
	struct sPostFX_Component {
		const char* names[POSTFX_COUNT] = {
			"Chromatic aberration",
			"LUT",
			"Grain"
		};

		const char* shaders[POSTFX_COUNT] = {
			"chrom_aberr"
		};

		FBO* post_fx_1 = NULL;
		FBO* post_fx_2 = NULL;

		bool enabled[POSTFX_COUNT];

		bool enable_postFX = true;

		void init(const vec2 &size);
		
		Texture* add_postFX(Texture *text);

		inline void render_imgui() {
			if (ImGui::TreeNode("Post FX")) {
				ImGui::Text("Enable or disable PostFX");
				for (int i = 0; i < POSTFX_COUNT; i++) {
					ImGui::Checkbox(names[i], &enabled[i]);
				}

				ImGui::Checkbox("Disable ALL postFx", &enable_postFX);

				ImGui::TreePop();
			}
			
		};
	};
};