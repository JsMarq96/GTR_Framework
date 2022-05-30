#include "tonemapping.h"
#include "texture.h"
#include <ctime>
namespace GTR {

	const char* mapping_shader[2] = { "perception_tonemapper", "uncharted_tonemapper" };

	void Tonemapping_Component::init() {
		fbo = new FBO();
		compute_fbo = new FBO();
		ilumination_fbo = new FBO();

		fbo->create(Application::instance->window_width, Application::instance->window_height,
			1,
			GL_RGBA,
			GL_FLOAT,
			true);

		ilumination_fbo->create(Application::instance->window_width, Application::instance->window_height,
			1,
			GL_RGBA,
			GL_FLOAT,
			true);

		compute_fbo->create(1, 1, // 1 by 1 texture 
			2, // 2 textures for the swapchain
			GL_RGBA, // 2 channels 
			GL_FLOAT, // of 32 bits
			false); // without a depth texture

		compute_fbo->bind();
		compute_fbo->enableSingleBuffer(0);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		compute_fbo->enableSingleBuffer(1);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		compute_fbo->unbind();
	}

	 Texture* Tonemapping_Component::pass(Texture* prev_pass) {
		glDisable(GL_DEPTH_TEST);
		
		// First, compute the luma of the scene
		ilumination_fbo->bind();
		ilumination_fbo->enableSingleBuffer(0);
		Mesh* quad = Mesh::getQuad();
		Shader* shader = Shader::Get("compute_lum");
		shader->enable();
		shader->setUniform("u_albedo_tex", prev_pass, 0);
		quad->render(GL_TRIANGLES);
		shader->disable();
		ilumination_fbo->unbind();

		// Compute the average and the maximun luminace, while storing it on the
		// luminance swapchain
		get_avg_max_lum_of_texture(ilumination_fbo->color_textures[0]);

		fbo->bind();
		fbo->enableSingleBuffer(0);

		shader = Shader::Get(mapping_shader[current_mapper]);
		shader->enable();
		shader->setUniform("u_albedo_tex", prev_pass, 0);
		shader->setUniform("u_max_avg_lum_tex", compute_fbo->color_textures[compute_fbo_swapchain], 1);
		shader->setUniform("u_max_avg_lum_prev_tex", compute_fbo->color_textures[!compute_fbo_swapchain], 2);
		quad->render(GL_TRIANGLES);

		shader->disable();
		fbo->unbind();

		// Update the swapchain index
		compute_fbo_swapchain = !compute_fbo_swapchain;

		glEnable(GL_DEPTH_TEST);
		first_frame = true;

		return fbo->color_textures[0];
	}

	 Texture* Tonemapping_Component::get_avg_max_lum_of_texture(Texture* text) {
		 compute_fbo->bind();

		 text->generateMipmaps();

		 compute_fbo->enableAllBuffers();

		 Mesh* quad = Mesh::getQuad();

		 Shader* shader = Shader::Get("compute_max_and_avg_lum");

		 int numLevels = (log2(max(text->width, text->height)));

		 shader->enable();
		 shader->setUniform("u_write_to_swap", (float)compute_fbo_swapchain);
		 shader->setUniform("u_tex", text, 0);
		 shader->setUniform("u_text_size", vec2(text->width, text->height));
		 shader->setUniform("u_lowest_mip_level", numLevels);
		 shader->setUniform("u_is_first_frame", (int)first_frame);

		 quad->render(GL_TRIANGLES);
		 shader->disable();

		 compute_fbo->unbind();


		 return compute_fbo->color_textures[compute_fbo_swapchain];
	 }
};