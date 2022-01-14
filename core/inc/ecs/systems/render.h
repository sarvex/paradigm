#pragma once
#include "gfx/drawgroup.h"
#include "psl/ecs/state.h"
#include "psl/math/matrix.h"
#include "psl/math/vec.h"
#include "psl/view_ptr.h"
#include "resource/resource.hpp"

namespace core::gfx
{
	class buffer_t;
	class drawpass;
}	 // namespace core::gfx

namespace core::ecs::components
{
	struct transform;
	struct renderable;
	struct camera;
}	 // namespace core::ecs::components

namespace core::ecs::systems
{
	class render
	{
	  public:
		render(psl::ecs::state_t& state, psl::view_ptr<core::gfx::drawpass> pass);

		~render() = default;

		render(const render&) = delete;
		render(render&&)	  = delete;
		render& operator=(const render&) = delete;
		render& operator=(render&&) = delete;

		void add_render_range(uint32_t begin, uint32_t end);
		void remove_render_range(uint32_t begin, uint32_t end);

	  private:
		void tick_draws(psl::ecs::info_t& info,
						psl::ecs::pack<const core::ecs::components::renderable,
									   psl::ecs::on_add<core::ecs::components::renderable>> renderables,
						psl::ecs::pack<const core::ecs::components::renderable,
									   psl::ecs::on_remove<core::ecs::components::renderable>> broken_renderables);

		psl::view_ptr<core::gfx::drawpass> m_Pass;

		core::gfx::drawgroup m_DrawGroup {};
		psl::array<std::pair<uint32_t, uint32_t>> m_RenderRanges;
	};
}	 // namespace core::ecs::systems