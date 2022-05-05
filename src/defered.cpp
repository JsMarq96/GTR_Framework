#include "renderer.h"

// Defintion of the Deferred rendering functions

void GTR::Renderer::_init_deferred_renderer() {
	deferred_gbuffer = new FBO();

	deferred_gbuffer->create(Application::instance->window_width, Application::instance->window_height, 
		3,
		GL_RGBA, 
		GL_UNSIGNED_BYTE,  // Enought..?
		true);
}

void GTR::Renderer::deferredRenderScene(const Scene* scene) {
	deferred_gbuffer->bind();

	// Clean last frame
	deferred_gbuffer->enableSingleBuffer(0);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	deferred_gbuffer->enableSingleBuffer(1);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	deferred_gbuffer->enableAllBuffers();

	// Note, only render the opaque drawcalls

	for (uint16_t i = 0; i < _opaque_objects.size(); i++) {
		renderDeferredPlainDrawCall(_opaque_objects[i], scene);
	}

	deferred_gbuffer->unbind();

	deferred_gbuffer->color_textures[0]->toViewport();
}


void GTR:: Renderer::renderDeferredPlainDrawCall(const sDrawCall& draw_call, const Scene* scene) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;

	glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("deferred_plane_opaque");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", draw_call.camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", draw_call.camera->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	// Material properties
	shader->setUniform("u_color", draw_call.material->color);
	bind_textures(draw_call.material, shader);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	draw_call.mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}