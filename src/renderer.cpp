#include "renderer.h"

#include "fbo.h"
#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "scene.h"
#include "extra/hdre.h"


using namespace GTR;

bool translucent_draw_call_distance_comp(const GTR::sDrawCall& d1, const GTR::sDrawCall& d2) {
	return d1.camera_distance > d2.camera_distance;
}
bool opaque_draw_call_distance_comp(const GTR::sDrawCall& d1, const GTR::sDrawCall& d2) {
	return d1.camera_distance < d2.camera_distance;
}

void Renderer::compute_visible_objects(Scene* base_scene, Camera* camera, std::vector<sDrawCall>* opaque_calls, std::vector<sDrawCall>* translucent_calls) {

}


void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

	std::vector<PrefabEntity*> scene_prefabs;

	//render entities
	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		//is a prefab!
		if (ent->entity_type == PREFAB)
		{
			PrefabEntity* pent = (GTR::PrefabEntity*)ent;
			if (pent->prefab) {
				scene_prefabs.push_back(pent);
			}
			else {
				assert("PREFAB IS NULL");
			}
		} else if (ent->entity_type == LIGHT) {
			LightEntity* curr_light = (LightEntity*)ent;

			if (curr_light->light_type == DIRECTIONAL_LIGHT) {
				_scene_directional_lights.push_back(curr_light);
			}
			else {
				_scene_non_directonal_lights.push_back(curr_light);
			}
			
			uint16_t light_id = 0;
			if (curr_light->light_type == DIRECTIONAL_LIGHT || curr_light->light_type == SPOT_LIGHT) {
				light_id = shadowmap_renderer.add_light(curr_light);
			}
		}
	}

	for (uint16_t i = 0; i < scene_prefabs.size(); i++) {
		PrefabEntity* pent = scene_prefabs[i];
		add_to_render_queue(pent->model, &(pent->prefab->root), camera, pent->pbr_structure);
	}

	scene_prefabs.clear();

	// Iterate all the lights on the scene
	for (uint16_t light_i = 0; light_i < _scene_non_directonal_lights.size(); light_i++) {
		LightEntity *curr_light = _scene_non_directonal_lights[light_i];

		// Check if the opaque object is in range of the light
		for (uint16_t i = 0; i < _opaque_objects.size(); i++) {
			if (curr_light->is_in_range(_opaque_objects[i].aabb)) {
				_opaque_objects[i].add_light(curr_light);
			}
		}

		// Check if the translucent object is in range of the light
		for (uint16_t i = 0; i < _translucent_objects.size(); i++) {
			if (curr_light->is_in_range(_translucent_objects[i].aabb)) {
				_translucent_objects[i].add_light(curr_light);
			}
		}
	}

	for (uint16_t light_i = 0; light_i < _scene_directional_lights.size(); light_i++) {
		LightEntity* curr_light = _scene_directional_lights[light_i];

		// Check if the opaque object is in range of the light
		for (uint16_t i = 0; i < _opaque_objects.size(); i++) {
			if (curr_light->is_in_range(_opaque_objects[i].aabb)) {
				_opaque_objects[i].add_light(curr_light);
			}
		}

		// Check if the translucent object is in range of the light
		for (uint16_t i = 0; i < _translucent_objects.size(); i++) {
			if (curr_light->is_in_range(_translucent_objects[i].aabb)) {
				_translucent_objects[i].add_light(curr_light);
			}
		}
	}

	// Render shadows
	shadowmap_renderer.render_scene_shadows(camera);

	// Order the opaque & translucent
	std::sort(_opaque_objects.begin(), _opaque_objects.end(), opaque_draw_call_distance_comp);
	std::sort(_translucent_objects.begin(), _translucent_objects.end(), translucent_draw_call_distance_comp);

	switch(current_pipeline) {
	case FORWARD:
		forwardRenderScene(scene, final_illumination_fbo);
		break;
	case DEFERRED:
		deferredRenderScene(scene, camera);
		break;
	default:
		break;
	}

	// Tonemapping pass

	if (deferred_output == RESULT && current_pipeline == DEFERRED) {
		tonemapping_fbo->bind();

		tonemapping_fbo->enableSingleBuffer(0);

		glDisable(GL_DEPTH_TEST);
		Mesh* quad = Mesh::getQuad();

		Shader* shader = Shader::Get("tonemapping_pass");

		shader->enable();
		shader->setUniform("u_albedo_tex", final_illumination_fbo->color_textures[0], 0);
		quad->render(GL_TRIANGLES);
		shader->disable();

		glEnable(GL_DEPTH_TEST);

		tonemapping_fbo->unbind();

		tonemapping_fbo->color_textures[0]->toViewport();
	}

	_opaque_objects.clear();
	_translucent_objects.clear();
	_scene_directional_lights.clear();
	_scene_non_directonal_lights.clear();

	// Show the shadowmap
	if (show_shadowmap) {
		glViewport(0, 0, 356, 356);
		Shader* shad = Shader::getDefaultShader("depth");
		shad->enable();
		shad->setUniform("u_camera_nearfar", vec2(0.1, 1000.0f));
		shad->setUniform("u_linearize", (liniearize_shadowmap_vis) ? 0 : 1);
		shadowmap_renderer.shadowmap->depth_texture->toViewport(shad);
		glViewport(0, 0, Application::instance->window_width, Application::instance->window_height);
	}

	shadowmap_renderer.clear_shadowmap();
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	assert(prefab && "PREFAB IS NULL");
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			//render node mesh
			renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = HDRE::Get(filename);
	if (!hdre)
		return NULL;

	Texture* texture = new Texture();
	if (hdre->getFacesf(0))
	{
		texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFacesf(0),
			hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT);
		for (int i = 1; i < hdre->levels; ++i)
			texture->uploadCubemap(texture->format, texture->type, false,
				(Uint8**)hdre->getFacesf(i), GL_RGBA32F, i);
	}
	else
		if (hdre->getFacesh(0))
		{
			texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFacesh(0),
				hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_HALF_FLOAT);
			for (int i = 1; i < hdre->levels; ++i)
				texture->uploadCubemap(texture->format, texture->type, false,
					(Uint8**)hdre->getFacesh(i), GL_RGBA16F, i);
		}
	return texture;
}


//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	texture = material->color_texture.texture;

	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	//select the blending
	if (material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("texture");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setUniform("u_color", material->color);
	if (texture)
		shader->setUniform("u_texture", texture, 0);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::eAlphaMode::MASK ? material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}


// =================================
//   CUSTOM METHODS
// =================================

void Renderer::init() {
	shadowmap_renderer.init();
	ao_component.init();

	final_illumination_fbo = new FBO();
	tonemapping_fbo = new FBO();

	final_illumination_fbo->create(Application::instance->window_width, Application::instance->window_height,
		1,
		GL_RGBA,
		GL_FLOAT,
		true);

	tonemapping_fbo->create(Application::instance->window_width, Application::instance->window_height,
		1,
		GL_RGBA,
		GL_FLOAT,
		true);

	// No need to add any preparations for forward renderer
	_init_deferred_renderer();
}

void Renderer::add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, ePBR_Type pbr) {
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			// Compute the closest distance between the bounding box of the mesh and the camera
			float camera_distance = Min((world_bounding.center + world_bounding.halfsize).distance(camera->eye), world_bounding.center.distance(camera->eye));
			camera_distance = Min(camera_distance, (world_bounding.center - world_bounding.halfsize).distance(camera->eye));
			add_draw_instance(node_model, node->mesh, node->material, camera, world_bounding.center.distance(camera->eye), world_bounding, pbr);
		}

		// Test if the objects is on the light's frustum
		for (uint16_t light_i = 0; light_i < _scene_non_directonal_lights.size(); light_i++) {
			LightEntity* curr_light = _scene_non_directonal_lights[light_i];

			if (curr_light->is_in_light_frustum(world_bounding)) {
				shadowmap_renderer.add_instance_to_light(curr_light->light_id, node->mesh, node->material->color_texture.texture, node->material->alpha_cutoff, node_model);
			}
		}

		for (uint16_t light_i = 0; light_i < _scene_directional_lights.size(); light_i++) {
			LightEntity* curr_light = _scene_directional_lights[light_i];

			if (curr_light->is_in_light_frustum(world_bounding)) {
				shadowmap_renderer.add_instance_to_light(curr_light->light_id, node->mesh, node->material->color_texture.texture, node->material->alpha_cutoff, node_model);
			}
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		add_to_render_queue(prefab_model, node->children[i], camera, pbr);
}

void Renderer::add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox &aabb, const ePBR_Type pbr) {
	// Based on the material, we add it to the translucent or the opaque queue
	if (material->alpha_mode != NO_ALPHA) {
		_translucent_objects.push_back(sDrawCall{ model, mesh, material, camera, camera_distance, pbr, aabb });
	}
	else {
		_opaque_objects.push_back(sDrawCall{ model, mesh, material, camera,  camera_distance, pbr, aabb });
	}
}
