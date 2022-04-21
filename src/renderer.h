#pragma once
#include "prefab.h"

#include <algorithm>

//forward declarations
class Camera;

namespace GTR {

	struct sDrawCall {
		Matrix44 model;
		Mesh* mesh;
		Material* material;
		Camera* camera;
		float camera_distance;
	};

	inline float Min(const float x, const float y) {
		return (x < y) ? x : y;
	}

	class Prefab;
	class Material;
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
		std::vector<sDrawCall> _opaque_objects;
		std::vector<sDrawCall> _translucent_objects;
	public:

		//add here your functions
		//...
		inline void renderDrawCall(const sDrawCall& draw_call) {
			renderMeshWithMaterial(draw_call.model, draw_call.mesh, draw_call.material, draw_call.camera);
		}

		void add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera);

		void add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance);


		// ===============================================
		// ===============================================

		//renders several elements of the scene
		void renderScene(GTR::Scene* scene, Camera* camera);
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};