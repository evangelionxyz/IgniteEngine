/* MIT License
*
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "asset.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include <vector>

namespace ignite
{
	class AssetStaticMesh : public Asset
	{
	public:
		AssetStaticMesh() = default;
		virtual ~AssetStaticMesh() = default;

		static Ref<AssetStaticMesh> Create();
		static AssetType GetStaticType() { return AssetType::StaticMesh; }
		virtual AssetType GetType() const { return GetStaticType(); }
		
		const std::vector<Ref<MeshInstance>>& GetMeshInstances() const { return m_MeshInstances; }
		void SetMeshInstance(const std::vector<Ref<MeshInstance>>& meshInstances) { m_MeshInstances = meshInstances; }
		void AddMeshInstance(const Ref<MeshInstance>& meshInstance) { m_MeshInstances.push_back(meshInstance); }

	private:
		std::vector<Ref<MeshInstance>> m_MeshInstances;

	};
}
