#include "virasa/renderer/geometry/Tangents.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace virasa
{
	namespace
	{
		struct CornerTangent
		{
			math::Vec3 tangent{};
			float sign = 1.0f;
		};

		[[nodiscard]] float Dot(const math::Vec3& a, const math::Vec3& b) noexcept
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		[[nodiscard]] math::Vec3 Cross(const math::Vec3& a, const math::Vec3& b) noexcept
		{
			return math::Vec3(
				a.y * b.z - a.z * b.y,
				a.z * b.x - a.x * b.z,
				a.x * b.y - a.y * b.x
			);
		}

		[[nodiscard]] float LengthSquared(const math::Vec3& v) noexcept
		{
			return Dot(v, v);
		}

		[[nodiscard]] math::Vec3 NormalizeOrFallback(
			const math::Vec3& v,
			const math::Vec3& fallback
		) noexcept
		{
			const float lengthSquared = LengthSquared(v);
			if (lengthSquared > 0.0f)
			{
				const float inverseLength = 1.0f / std::sqrt(lengthSquared);
				return math::Vec3(v.x * inverseLength, v.y * inverseLength, v.z * inverseLength);
			}

			return fallback;
		}

		[[nodiscard]] math::Vec3 BuildOrthogonalFallback(const math::Vec3& normal) noexcept
		{
			const math::Vec3 axis = (std::fabs(normal.z) < 0.999f)
				? math::Vec3(0.0f, 0.0f, 1.0f)
				: math::Vec3(0.0f, 1.0f, 0.0f);
			return NormalizeOrFallback(Cross(axis, normal), math::Vec3(1.0f, 0.0f, 0.0f));
		}

		[[nodiscard]] CornerTangent ComputeCornerTangent(
			const Vertex& v0,
			const Vertex& v1,
			const Vertex& v2
		) noexcept
		{
			const math::Vec3 edge1(
				v1.position.x - v0.position.x,
				v1.position.y - v0.position.y,
				v1.position.z - v0.position.z
			);
			const math::Vec3 edge2(
				v2.position.x - v0.position.x,
				v2.position.y - v0.position.y,
				v2.position.z - v0.position.z
			);

			const float deltaU1 = v1.uv.x - v0.uv.x;
			const float deltaV1 = v1.uv.y - v0.uv.y;
			const float deltaU2 = v2.uv.x - v0.uv.x;
			const float deltaV2 = v2.uv.y - v0.uv.y;

			const float determinant = deltaU1 * deltaV2 - deltaU2 * deltaV1;
			if (std::fabs(determinant) <= 1.0e-20f)
			{
				return CornerTangent{BuildOrthogonalFallback(v0.normal), 1.0f};
			}

			const float inverseDeterminant = 1.0f / determinant;
			const math::Vec3 tangent(
				(edge1.x * deltaV2 - edge2.x * deltaV1) * inverseDeterminant,
				(edge1.y * deltaV2 - edge2.y * deltaV1) * inverseDeterminant,
				(edge1.z * deltaV2 - edge2.z * deltaV1) * inverseDeterminant
			);
			const math::Vec3 bitangent(
				(edge2.x * deltaU1 - edge1.x * deltaU2) * inverseDeterminant,
				(edge2.y * deltaU1 - edge1.y * deltaU2) * inverseDeterminant,
				(edge2.z * deltaU1 - edge1.z * deltaU2) * inverseDeterminant
			);

			const math::Vec3 normalizedTangent = NormalizeOrFallback(
				tangent,
				BuildOrthogonalFallback(v0.normal)
			);
			const math::Vec3 reconstructedBitangent = Cross(v0.normal, normalizedTangent);
			const float sign = (Dot(reconstructedBitangent, bitangent) < 0.0f) ? -1.0f : 1.0f;

			return CornerTangent{normalizedTangent, sign};
		}
	}

	void GenerateTangents(MeshData& mesh_data)
	{
		std::vector<math::Vec3> accumulatedTangents(mesh_data.vertices.size(), math::Vec3(0.0f, 0.0f, 0.0f));
		std::vector<bool> signAssigned(mesh_data.vertices.size(), false);

		for (std::size_t triangleIndex = 0; triangleIndex < mesh_data.indices.size(); triangleIndex += 3)
		{
			const uint32_t index0 = mesh_data.indices[triangleIndex + 0];
			const uint32_t index1 = mesh_data.indices[triangleIndex + 1];
			const uint32_t index2 = mesh_data.indices[triangleIndex + 2];

			const Vertex& v0 = mesh_data.vertices[index0];
			const Vertex& v1 = mesh_data.vertices[index1];
			const Vertex& v2 = mesh_data.vertices[index2];

			const CornerTangent corner0 = ComputeCornerTangent(v0, v1, v2);
			const CornerTangent corner1 = ComputeCornerTangent(v1, v2, v0);
			const CornerTangent corner2 = ComputeCornerTangent(v2, v0, v1);

			accumulatedTangents[index0].x += corner0.tangent.x;
			accumulatedTangents[index0].y += corner0.tangent.y;
			accumulatedTangents[index0].z += corner0.tangent.z;
			if (!signAssigned[index0])
			{
				mesh_data.vertices[index0].tangent.w = corner0.sign;
				signAssigned[index0] = true;
			}

			accumulatedTangents[index1].x += corner1.tangent.x;
			accumulatedTangents[index1].y += corner1.tangent.y;
			accumulatedTangents[index1].z += corner1.tangent.z;
			if (!signAssigned[index1])
			{
				mesh_data.vertices[index1].tangent.w = corner1.sign;
				signAssigned[index1] = true;
			}

			accumulatedTangents[index2].x += corner2.tangent.x;
			accumulatedTangents[index2].y += corner2.tangent.y;
			accumulatedTangents[index2].z += corner2.tangent.z;
			if (!signAssigned[index2])
			{
				mesh_data.vertices[index2].tangent.w = corner2.sign;
				signAssigned[index2] = true;
			}
		}

		for (std::size_t vertexIndex = 0; vertexIndex < mesh_data.vertices.size(); ++vertexIndex)
		{
			Vertex& vertex = mesh_data.vertices[vertexIndex];
			const math::Vec3& normal = vertex.normal;
			const math::Vec3& accumulated = accumulatedTangents[vertexIndex];

			const float projection = Dot(normal, accumulated);
			const math::Vec3 orthogonalized(
				accumulated.x - normal.x * projection,
				accumulated.y - normal.y * projection,
				accumulated.z - normal.z * projection
			);
			const math::Vec3 tangent = NormalizeOrFallback(
				orthogonalized,
				BuildOrthogonalFallback(normal)
			);

			vertex.tangent.x = tangent.x;
			vertex.tangent.y = tangent.y;
			vertex.tangent.z = tangent.z;
			if (!signAssigned[vertexIndex])
			{
				vertex.tangent.w = 1.0f;
			}
		}
	}
}
