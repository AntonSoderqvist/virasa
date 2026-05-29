#ifndef VIRASA_RENDERER_GEOMETRY_TANGENTS_H
#define VIRASA_RENDERER_GEOMETRY_TANGENTS_H

#include "virasa/renderer/Types.h"

namespace virasa
{
	/**
	 * @brief Generates per-vertex tangents for the supplied mesh data.
	 * @param mesh_data Mesh whose vertex tangent fields are overwritten in place.
	 */
	void GenerateTangents(MeshData& mesh_data);
}

#endif
