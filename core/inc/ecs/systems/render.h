#pragma once
#include "ecs/state.h"
#include "gfx/pass.h"
#include "systems/resource.h"
#include "gfx/drawgroup.h"
#include "math/vec.h"
#include "math/matrix.h"

namespace core::gfx
{
	class context;
	class swapchain;
	class buffer;
} // namespace core::gfx
namespace core::os
{
	class surface;
}
namespace core::ecs::components
{
	struct transform;
	struct renderable;
	struct camera;
} // namespace core::ecs::components

namespace core::ecs::systems
{
	class render
	{
		const psl::mat4x4 clip{1.0f,  0.0f, 0.0f, 0.0f, +0.0f, -1.0f, 0.0f, 0.0f,
							   +0.0f, 0.0f, 0.5f, 0.0f, +0.0f, 0.0f,  0.5f, 1.0f};

	  public:
		struct framedata
		{
			psl::mat4x4 clipMatrix;
			psl::mat4x4 projectionMatrix;
			psl::mat4x4 modelMatrix;
			psl::mat4x4 viewMatrix;
			psl::mat4x4 WVP;
			psl::mat4x4 VP;
			psl::vec4 ScreenParams;
			psl::vec4 GameTimer;
			psl::vec4 Parameters;
			psl::vec4 FogColor;
			psl::vec4 viewPos;
			psl::vec4 viewDir;
		};

		render(psl::ecs::state& state, core::resource::handle<core::gfx::context> context,
			   core::resource::handle<core::gfx::swapchain> swapchain,
			   core::resource::handle<core::os::surface> surface, core::resource::handle<core::gfx::buffer> buffer);
		~render() = default;

		render(const render&) = delete;
		render(render&&)	  = delete;
		render& operator=(const render&) = delete;
		render& operator=(render&&) = delete;

	  private:
		void tick_cameras(psl::ecs::info& info,
			psl::ecs::pack<const core::ecs::components::camera, const core::ecs::components::transform> cameras);
		void tick_draws(psl::ecs::info& info,
			psl::ecs::pack<const core::ecs::components::transform, const core::ecs::components::renderable,
							psl::ecs::on_combine<core::ecs::components::transform, core::ecs::components::renderable>>
				renderables,
			psl::ecs::pack<const core::ecs::components::transform, const core::ecs::components::renderable,
							psl::ecs::on_break<core::ecs::components::transform, core::ecs::components::renderable>>
				broken_renderables);
		void update_buffer(size_t index, const core::ecs::components::transform& transform,
						   const core::ecs::components::camera& camera);

		core::gfx::pass m_Pass;
		core::resource::handle<core::gfx::swapchain> m_Swapchain;
		core::resource::handle<core::os::surface> m_Surface;

		std::vector<memory::segment> fdatasegment;
		core::resource::handle<core::gfx::buffer> m_Buffer;
		core::gfx::drawgroup m_DrawGroup{};
	};
} // namespace core::ecs::systems