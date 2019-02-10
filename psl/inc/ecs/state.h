#pragma once
#include "entity.h"
#include "selectors.h"
#include "pack.h"
#include "IDGenerator.h"
#include "vector.h"
#include "details/component_key.h"
#include "details/entity_info.h"
#include "details/component_info.h"
#include "unique_ptr.h"
#include "bytell_map.h"
#include "array.h"
#include <chrono>
#include "task.h"
#include "template_utils.h"
#include <functional>

namespace psl::async
{
	class scheduler;
}

namespace psl::ecs
{
	enum class threading
	{
		seq = 0,
		sequential = seq,
		par = 1,
		parallel = par,
		main = 2
	};

	template<typename T>
	struct empty{};


	class state
	{
	public:
		state(size_t workers = 0);
		~state() = default;
		state(const state&) = delete;
		state(state&&) = delete;
		state& operator=(const state&) = delete;
		state& operator=(state&&) = delete;

		template<typename... Ts>
		void add_components(psl::array_view<entity> entities)
		{
			(add_component<Ts>(entities), ... );
		}
		template<typename... Ts>
		void add_components(psl::array_view<entity> entities, Ts&&... prototype)
		{
			( add_component(entities, std::forward<Ts>(prototype)), ...);
		}

		template<typename... Ts>
		void remove_components(psl::array_view<entity> entities) noexcept
		{
			( remove_component(details::key_for<Ts>(), entities), ... );
		}

		template<typename... Ts>
		psl::ecs::pack<Ts...> get_components(psl::array_view<entity> entities) const noexcept;


		template<typename... Ts>
		bool has_components(psl::array_view<entity> entities) const noexcept;

		template <typename T, typename... Ts>
		bool is_owned_by(entity e, const T& component, const Ts&... components) const noexcept;

		template<typename... Ts>
		psl::array<entity> create(size_t count = 1)
		{
			psl::array<entity> entities;
			auto id = m_Generator.create(count);
			for(auto i = 0; i < count; ++i)
			{
				entities.emplace_back(entity{id});
				m_Entities.emplace(entity{id}, details::entity_info{});
				++id;
			}

			add_components<Ts>(entities);
			return entities;
		}
		template<typename... Ts>
		psl::array<entity> create(size_t count = 1, Ts&&... prototype)
		{
			psl::array<entity> entities;
			auto id = m_Generator.create(count);
			for(auto i = 0; i < count; ++i)
			{
				entities.emplace_back(entity{id});
				m_Entities.emplace(entity{id}, details::entity_info{});
				++id;
			}

			add_components(entities, std::forward<Ts>(prototype)...);

			return entities;
		}

		void destroy(psl::array_view<entity> entities) noexcept;

		template<typename... Ts>
		psl::vector<entity> filter()  const noexcept;

		template<typename... Ts>
		psl::vector<entity> filter(psl::array_view<entity> entities)  const noexcept;

		template<typename... Ts>
		void set(psl::array_view<entity> entities, psl::array_view<Ts>... data) noexcept;


		void tick(std::chrono::duration<float> dTime);

		void reset(psl::array_view<entity> entities) noexcept;
	private:

		//------------------------------------------------------------
		// helpers
		//------------------------------------------------------------
		details::component_info& get_component_info(details::component_key_t key, size_t size);

		//------------------------------------------------------------
		// add_component
		//------------------------------------------------------------
		template<typename T>
		void add_component(psl::array_view<entity> entities, T&& prototype)
		{
			if constexpr(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value && std::is_trivially_destructible<T>::value)
			{
				static_assert(!std::is_empty_v<T>, "Unnecessary initialization of component tag, you likely didn't mean this. Wrap tags in psl::ecs::empty<T>{} to avoid initialization.");
				add_component_impl(details::key_for<T>(), entities, sizeof(T), &prototype);
			}
			else if constexpr(std::is_constructible<decltype(std::function(prototype)), T>::value)
			{
				using tuple_type = typename psl::templates::func_traits<T>::arguments_t;
				static_assert(std::tuple_size<tuple_type>::value == 1, "only one argument is allowed in the prototype invocable");
				using arg0_t = std::tuple_element_t<0, tuple_type>;
				static_assert(std::is_reference_v<arg0_t> && !std::is_const_v<arg0_t>, "the argument type should be of 'T&'");
				using type = typename std::remove_reference<arg0_t>::type;
				static_assert(!std::is_empty_v<type>, "Unnecessary initialization of component tag, you likely didn't mean this. Wrap tags in psl::ecs::empty<T>{} to avoid initialization.");
				add_component_impl(details::key_for<type>(), entities, sizeof(type), [prototype](std::uintptr_t location, size_t count)
							  {
								  for(auto i = size_t{0}; i < count; ++i)
								  {
									  std::invoke(prototype, *((type*)(location)+i));
								  }
							  });
			}
			else
			{
				static_assert(psl::templates::always_false<T>::value, "could not figure out if the template type was an invocable or a component prototype");
			}
		}

		template<typename T>
		void add_component(psl::array_view<entity> entities, psl::ecs::empty<T>&& prototype)
		{
			if constexpr(std::is_trivially_constructible_v<T>)
			{
				add_component_impl(details::key_for<T>(), entities, (std::is_empty<T>::value) ? 0 : sizeof(T));
			}
			else
			{
				T v{};
				add_component_impl(details::key_for<T>(), entities, sizeof(T), &v);
			}
		}

		template<typename T>
		void add_component(psl::array_view<entity> entities)
		{
			if constexpr(std::is_trivially_constructible_v<T>)
			{
				add_component_impl(details::key_for<T>(), entities, (std::is_empty<T>::value) ? 0 : sizeof(T));
			}
			else
			{
				T v{};
				add_component_impl(details::key_for<T>(), entities, sizeof(T), &v);
			}
		}

		void add_component_impl(details::component_key_t key, psl::array_view<entity> entities, size_t size);
		void add_component_impl(details::component_key_t key, psl::array_view<entity> entities, size_t size, std::function<void(std::uintptr_t, size_t)> invocable);
		void add_component_impl(details::component_key_t key, psl::array_view<entity> entities, size_t size, void* prototype);

		//------------------------------------------------------------
		// remove_component
		//------------------------------------------------------------
		void remove_component(details::component_key_t key, psl::array_view<entity> entities) noexcept;

		void set(psl::array_view<entity> entities, details::component_key_t key, void* data) noexcept;

		struct difference_set
		{
			psl::array<entity> created;
			psl::array<entity> destroyed;

			psl::bytell_map<details::component_key_t, psl::array<entity>> added_components;
			psl::bytell_map<details::component_key_t, psl::array<entity>> removed_components;
		};

		psl::bytell_map<entity, details::entity_info> m_Entities;
		psl::bytell_map<details::component_key_t, details::component_info> m_Components;

		psl::generator<typename entity::value_type> m_Generator;
		psl::unique_ptr<psl::async::scheduler> m_Scheduler;

		psl::static_array<difference_set, 2> m_Changes;
		size_t m_Tick;
	};	
}