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


void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

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
				add_to_render_queue(ent->model, &(pent->prefab->root), camera);
			}
			else {
				assert("PREFAB IS NULL");
			}
		} else if (ent->entity_type == LIGHT) {
			_scene_lights.push_back((LightEntity*)ent);
		}
	}

	// Iterate all the lights on the scene
	for (uint16_t light_i = 0; light_i < _scene_lights.size(); light_i++) {
		LightEntity *curr_light =  _scene_lights[light_i];
		uint16_t light_id = 0;
		if (curr_light->light_type == SPOT_LIGHT) {
			light_id = shadowmap_renderer.add_light(curr_light);
		}
		
		// Check if the opaque object is in range of the light
		for (uint16_t i = 0; i < _opaque_objects.size(); i++) {
			if (curr_light->is_in_range(_opaque_objects[i].aabb)) {
				_opaque_objects[i].add_light(curr_light);

				if (curr_light->light_type == SPOT_LIGHT) {
					shadowmap_renderer.add_instance_to_light(light_id, _opaque_objects[i].mesh, _opaque_objects[i].model);
				}
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
	shadowmap_renderer.render_scene_shadows();

	// Order the opaque & translucent
	std::sort(_opaque_objects.begin(), _opaque_objects.end(), opaque_draw_call_distance_comp);
	std::sort(_translucent_objects.begin(), _translucent_objects.end(), translucent_draw_call_distance_comp);

	// First, render the opaque object
	for (uint16_t i = 0; i < _opaque_objects.size(); i++) {
		renderDrawCall(_opaque_objects[i]);
	}

	// then, render the translucnet, and masked objects
	for (uint16_t i = 0; i < _translucent_objects.size(); i++) {
		renderDrawCall(_translucent_objects[i]);
	}

	_opaque_objects.clear();
	_translucent_objects.clear();
	_scene_lights.clear();

	// Show the shadowmap
	if (show_shadowmap) {
		glViewport(0, 0, 256, 256);
		Shader* shad = Shader::getDefaultShader("depth");
		shad->enable();
		shad->setUniform("u_camera_nearfar", vec2(0.01, 70.0f));
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

inline void Renderer::renderDrawCall(const sDrawCall& draw_call) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	texture = draw_call.material->color_texture.texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	//select the blending
	if (draw_call.material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("phong");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// Upload light data
	// Common data of the lights
	shader->setUniform3Array("u_light_pos", (float*) draw_call.light_positions, draw_call.light_count);
	shader->setUniform3Array("u_light_color", (float*) draw_call.light_color, draw_call.light_count);
	shader->setUniform1Array("u_light_type", (int*) draw_call.light_type, draw_call.light_count);
	shader->setUniform("u_num_lights", draw_call.light_count);

	// Spotlight data of the lights
	shader->setUniform3Array("u_light_direction", (float*)draw_call.light_direction, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_angle", (float*)draw_call.light_cone_angle, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_decay", (float*)draw_call.light_cone_decay, draw_call.light_count);
	
	//upload uniforms
	shader->setUniform("u_viewprojection", draw_call.camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", draw_call.camera->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setUniform("u_color", draw_call.material->color);
	if (texture)
		shader->setUniform("u_texture", texture, 0);

	// Set the shadowmap
	shader->setUniform("u_shadow_map", shadowmap_renderer.get_shadowmap(), 8);
	shader->setUniform("u_shadow_vp", shadowmap_renderer.draw_call_stack[0].light_view_projection);
	shader->setUniform("u_shadow_bias", shadowmap_renderer.shadow_bias);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	draw_call.mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
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
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
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
	if(material->two_sided)
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
	shader->setUniform("u_model", model );
	float t = getTime();
	shader->setUniform("u_time", t );

	shader->setUniform("u_color", material->color);
	if(texture)
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


// =================================
//   CUSTOM METHODS
// =================================

void Renderer::init() {
	shadowmap_renderer.init();
}

void Renderer::add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera) {
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
			// Add to the queue

			// Compute the closest distance between the bounding box of the mesh and the camera
			float camera_distance = Min((world_bounding.center + world_bounding.halfsize).distance(camera->eye), world_bounding.center.distance(camera->eye));
			camera_distance = Min(camera_distance, (world_bounding.center - world_bounding.halfsize).distance(camera->eye));
			add_draw_instance(node_model, node->mesh, node->material, camera, world_bounding.center.distance(camera->eye), world_bounding);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		add_to_render_queue(prefab_model, node->children[i], camera);
}

void Renderer::add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox &aabb) {
	// Based on the material, we add it to the translucent or the opaque queue
	if (material->alpha_mode != NO_ALPHA) {
		_translucent_objects.push_back(sDrawCall{ model, mesh, material, camera, camera_distance, aabb });
	}
	else {
		_opaque_objects.push_back(sDrawCall{ model, mesh, material, camera,  camera_distance, aabb });
	}
}



// =============================
//  SHADOW RENDERER
// =============================


void GTR::ShadowRenderer::init() {
	shadowmap = new FBO();
	shadowmap->setDepthOnly(SHADOW_MAP_RES, SHADOW_MAP_RES);
}
void GTR::ShadowRenderer::clean() {
	shadowmap->freeTextures();
}

void GTR::ShadowRenderer::render_light(sShadowDrawCall& draw_call) {
	//define locals to simplify coding
	Shader* shader = Shader::Get("flat");

	Matrix44 view_matrix;

	Matrix44& light_model = draw_call.light->get_model();

	view_matrix.lookAt(light_model.getTranslation(), light_model * vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));

	// Set the type of the view-projection
	if (draw_call.light->light_type == SPOT_LIGHT) {
		Matrix44 projection;
		projection.perspective(draw_call.light->cone_angle,
			1.0f, 
			0.1f, 
			draw_call.light->max_distance);
		 view_matrix = view_matrix * projection;
	}
	
	draw_call.light_view_projection = view_matrix;

	shader->enable();

	assert(glGetError() == GL_NO_ERROR);
	glEnable(GL_CULL_FACE);
	for (uint16_t i = 0; i < draw_call.obj_cout; i++) {
		//upload uniforms
		shader->setUniform("u_viewprojection", view_matrix);
		shader->setUniform("u_model", draw_call.models[i]);

		//shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

		//do the draw call that renders the mesh into the screen
		draw_call.meshes[i]->render(GL_TRIANGLES);
	}

	assert(glGetError() == GL_NO_ERROR);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void GTR::ShadowRenderer::render_scene_shadows() {
	// Disable color writing
	glColorMask(false, false, false, false);

	// Enable write to FBO
	shadowmap->bind();

	glClear(GL_DEPTH_BUFFER_BIT);

	for (uint16_t i = 0; i < draw_call_stack.size(); i++) {
		render_light(draw_call_stack[i]);
		draw_call_stack[i].clear();
	}

	// Reenable write to framebuffer
	shadowmap->unbind();

	// Re-enable color writing
	glColorMask(true, true, true, true);
}