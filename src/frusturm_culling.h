#pragma once

#include "prefab.h"
#include "fbo.h"
#include "mesh.h"
#include "application.h"
#include "draw_call.h"
#include <algorithm>


namespace GTR {

	namespace CULLING {

		inline float Min(const float x, const float y) {
			return (x < y) ? x : y;
		}

		struct sSceneCulling;

		void frustrum_culling(std::vector<GTR::BaseEntity*> entities, sSceneCulling *culling_result, Camera *cam);

		struct sSceneCulling {
			std::vector<PrefabEntity*> scene_prefabs;
			std::vector<sDrawCall> _opaque_objects;
			std::vector<sDrawCall> _translucent_objects;
			std::vector<LightEntity*> _scene_non_directonal_lights;
			std::vector<LightEntity*> _scene_directional_lights;

			inline void clear() {
				_opaque_objects.clear();
				_translucent_objects.clear();
				_scene_directional_lights.clear();
				_scene_non_directonal_lights.clear();
				scene_prefabs.clear();
			}


			void add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, ePBR_Type pbr);

			void add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox& aabb, const ePBR_Type pbr);
		};
	};
};