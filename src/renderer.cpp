#include "renderer.h"


using namespace GTR;

void Renderer::render_skybox(Camera *camera) {
	// Render skybox
	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj");
	Shader* shader = Shader::Get("skybox");
	shader->enable();

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	mat4 model;
	model.translate(camera->eye.x, camera->eye.y, camera->eye.z);
	model.scale(10.0f, 10.0f, 10.0f);

	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_texture", skybox_texture, 5);

	sphere->render(GL_TRIANGLES);

	shader->disable();

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

	//render entities
	CULLING::sSceneCulling culling_result;

	CULLING::frustrum_culling(scene->entities, &culling_result, camera);
	entity_list = &scene->entities;
	current_scene = scene;

	// Render shadows
	shadowmap_renderer.add_scene_data(&culling_result);
	shadowmap_renderer.render_scene_shadows(camera);

	//reflections_component.capture_all_probes(*entity_list);

	// Render scene
	switch (current_pipeline) {
	case FORWARD:
		forwardRenderScene(scene, camera, final_illumination_fbo, &culling_result, use_irradiance);
		break;
	case DEFERRED:
		deferredRenderScene(scene, camera, final_illumination_fbo, &culling_result);
		break;
	default:
		break;
	}

	// Irradiance test
	final_illumination_fbo->bind();

	irradiance_component.debug_render_all_probes(10.0f, camera);

	reflections_component.render_probes(*camera);

	final_illumination_fbo->unbind();


	if (deferred_output != RESULT && current_pipeline == DEFERRED) {
		return;
	}


	// Post-processing Tonemmaping
	Texture* end_result = final_illumination_fbo->color_textures[0];

	if (current_pipeline == DEFERRED) {
		//end_result = volumetric_component.render(camera, vec2(), &culling_result, &shadowmap_renderer, end_result, deferred_gbuffer->depth_texture);
	}

	// Only add tonemapping if its the final image
	if (deferred_output == RESULT) {
		end_result = tonemapping_component.pass(end_result);
	}

	end_result->toViewport();

	// Cleanup & debug
	culling_result.clear();

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

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
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
	ao_component.init();
	tonemapping_component.init();
	irradiance_component.init(this);
	reflections_component.init(this);
	volumetric_component.init(vec2(Application::instance->window_width, Application::instance->window_height));

	final_illumination_fbo = new FBO();

	final_illumination_fbo->create(Application::instance->window_width, Application::instance->window_height,
		1,
		GL_RGBA,
		GL_FLOAT,
		true);

	// No need to add any preparations for forward renderer
	_init_deferred_renderer();

	skybox_texture = CubemapFromHDRE("data/night.hdre");
}