// Runtime main
#include "Core/Handle.h"
#include "Core/Logger.h"

struct Mesh {
	std::string name;
	int vertex_count;
};

struct Light {
	float intensity;
	float color[3];
};

int main()
{
	koi::Logger::Init();

	koi::StaticHandlePool<Mesh, 2> mPool;
	koi::HandlePool<Mesh> mesh_pool;
	koi::HandlePool<Light> light_pool;

	auto player_mesh = mesh_pool.Emplace("Player", 5000);
	auto enemy_mesh = mesh_pool.Emplace("Enemy", 3000);

	auto m1 = mPool.Emplace("Static Player", 5000);
	auto m2 = mPool.Emplace("Static Enemy", 3000);

	for (auto& m : mPool.GetData())
	{
		KOI_LOG_INFO("Updating mesh in batch: {}", m.name);
	}

	if (Mesh* mesh = mesh_pool.Get(player_mesh))
	{
		KOI_LOG_INFO("Mesh: {}", mesh->name);
	}

	mesh_pool.Erase(player_mesh);

	if (Mesh* mesh = mesh_pool.Get(player_mesh))
	{
		KOI_LOG_INFO("Shouldn't print");
	}
	else
	{
		KOI_LOG_INFO("Player mesh handle correctly identified as stale");
	}

	for (auto& mesh : mesh_pool.GetData())
	{
		// Blazing fast linear iteration over actual data
		KOI_LOG_INFO("Updating mesh in batch: ",mesh.name);
	}

	koi::Logger::Shutdown();

	return 0;
}
