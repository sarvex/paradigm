
#include "psl/ecs/state.hpp"
#include "psl/algorithm.hpp"
#include "psl/async/async.hpp"
#include "psl/unique_ptr.hpp"

#include <numeric>
using namespace psl::ecs;

using psl::ecs::details::component_key_t;

template <typename T = void>
void invoke(auto&& fn, auto begin, auto end) {
	auto count = end - begin;
	if(count > 0) {
		auto* data = begin.operator->();
		if constexpr(std::is_same_v<T, void>) {
			fn(data, data + count);
		} else {
			fn((T*)data, (T*)(data) + count);
		}
	}
}


state_t::state_t(size_t workers, size_t cache_size, entity_t::size_type min_entities_per_worker)
	: m_Cache(cache_size),
	  m_Scheduler(new psl::async::scheduler((workers == 0) ? std::nullopt : std::optional {workers})),
	  m_MinEntitiesPerWorker(min_entities_per_worker) {
#if !defined(PE_ECS_DISABLE_LOOKUP_CACHE)
	static std::mutex mut {};
	static std::atomic<size_t> generation {1};
	std::lock_guard l {mut};
	m_StateUniqueKey = ++generation;
#endif
	m_ModifiedEntities.reserve(65536);
}

state_t::~state_t() = default;

constexpr auto align(std::uintptr_t& ptr, size_t alignment) noexcept {
#pragma warning(push)
#pragma warning(disable : 4146)
	const auto orig	   = ptr;
	const auto aligned = (ptr - 1u + alignment) & -alignment;
	ptr				   = aligned;
	return aligned - orig;
#pragma warning(pop)
}


psl::array<psl::array<details::dependency_pack>> slice(psl::array<details::dependency_pack>& source,
													   size_t workers = std::numeric_limits<size_t>::max(),
													   entity_t::size_type min_entities_per_worker = 1024) {
	psl::array<psl::array<details::dependency_pack>> packs {};

	if(source.size() == 0)
		return packs;

	auto [smallest_batch, largest_batch] =
	  std::minmax_element(std::begin(source), std::end(source), [](const auto& lhs, const auto& rhs) {
		  return lhs.entities() < rhs.entities();
	  });
	workers = std::min<size_t>(workers, std::thread::hardware_concurrency());
	auto max_workers =
	  std::max<size_t>(1u,
					   std::min(workers,
								(largest_batch->entities() - (largest_batch->entities() % min_entities_per_worker)) /
								  min_entities_per_worker));

	// To guard having systems run with concurrent packs that have no data in them.
	// Doing so would seem counter-intuitive to users
	while((float)smallest_batch->entities() / (float)max_workers < 1.0f && max_workers > 1) {
		--max_workers;
	}
	workers = max_workers;

	packs.resize(workers);
	for(auto& dep_pack : source) {
		if(dep_pack.is_partial_pack()) {
			auto batch_size = dep_pack.entities() / workers;
			size_t processed {0};
			for(auto i = 0; i < workers - 1; ++i) {
				packs[i].emplace_back(dep_pack.slice(processed, processed + batch_size));
				processed += batch_size;
			}
			packs[packs.size() - 1].emplace_back(dep_pack.slice(processed, dep_pack.entities()));
		} else	  // if packs cannot be split, then emplace the 'full' data
		{
			for(auto i = 0; i < workers; ++i) packs[i].emplace_back(dep_pack);
		}
	}
	return packs;
}

void state_t::prepare_system(std::chrono::duration<float> dTime,
							 std::chrono::duration<float> rTime,
							 std::uintptr_t cache_offset,
							 details::system_information& information) {
	auto write_data = [](state_t& state, psl::array<details::dependency_pack> const& dep_packs) {
		for(const auto& dep_pack : dep_packs) {
			for(auto& binding : dep_pack.m_RWBindings) {
				const size_t size	= dep_pack.m_Sizes.at(binding.first);
				std::uintptr_t data = (std::uintptr_t)binding.second.data();
				state.set(dep_pack.m_Entities, binding.first, (void*)data);
			}
		}
	};

	auto pack = information.create_pack();
	bool is_partial_pack =
	  std::any_of(std::begin(pack), std::end(pack), [](const auto& dep_pack) { return dep_pack.is_partial_pack(); });


	auto filter_groups	  = information.filters();
	auto transform_groups = information.transforms();

	auto filter_it	  = begin(filter_groups);
	auto transform_it = begin(transform_groups);

	if(is_partial_pack && information.threading() == threading::par) {
		for(auto& dep_pack : pack) {
			psl::array_view<entity_t> entities;
			auto group_it = std::find_if(
			  begin(m_Filters), end(m_Filters), [filter_it](const auto& data) { return data == **filter_it; });
			if(*transform_it) {
				auto transform		= std::find_if(begin(group_it->transformations),
											   end(group_it->transformations),
											   [transform_it](const auto& data) { return data.group == *transform_it; });
				transform->entities = group_it->entities;
				transform->entities.erase(
				  transform->group->transform(begin(transform->entities), end(transform->entities), *this),
				  end(transform->entities));
				entities = transform->entities;
			} else {
				entities = group_it->entities;
			}

			filter_it	 = std::next(filter_it);
			transform_it = std::next(transform_it);
			if(entities.size() == 0)
				continue;

			cache_offset += prepare_bindings(entities, (void*)cache_offset, dep_pack);
		}

		// main thread participates, so workers + 1
		auto multi_pack = slice(pack, m_Scheduler->workers() + 1, m_MinEntitiesPerWorker);

		auto index = info_buffer.size();
		for(size_t i = 0; i < std::min(m_Scheduler->workers() + 1, multi_pack.size()); ++i)
			info_buffer.emplace_back(new info_t(*this, dTime, rTime, m_Tick));

		auto infoBuffer = std::next(std::begin(info_buffer), index);

		for(auto& mPack : multi_pack) {
			auto t1 = m_Scheduler->schedule([&fn = information.system(), infoBuffer, &mPack]() mutable {
				std::invoke(fn, infoBuffer->get(), mPack);
			});
			auto t2 = m_Scheduler->schedule([&]() { std::invoke(write_data, *this, mPack); });
			t2.after(t1);

			infoBuffer = std::next(infoBuffer);
		}
		m_Scheduler->execute();
	} else {
		bool has_entities = false;
		for(auto& dep_pack : pack) {
			psl::array_view<entity_t> entities;
			auto group_it = std::find_if(
			  begin(m_Filters), end(m_Filters), [filter_it](const auto& data) { return data == **filter_it; });
			if(*transform_it) {
				auto transform		= std::find_if(begin(group_it->transformations),
											   end(group_it->transformations),
											   [transform_it](const auto& data) { return data.group == *transform_it; });
				transform->entities = group_it->entities;
				transform->entities.erase(
				  transform->group->transform(begin(transform->entities), end(transform->entities), *this),
				  end(transform->entities));
				entities = transform->entities;
			} else {
				entities = group_it->entities;
			}

			filter_it	 = std::next(filter_it);
			transform_it = std::next(transform_it);
			if(entities.size() == 0)
				continue;
			has_entities = true;
			cache_offset += prepare_bindings(entities, (void*)cache_offset, dep_pack);
		}

		info_buffer.emplace_back(new info_t(*this, dTime, rTime, m_Tick));
		information.operator()(*info_buffer[info_buffer.size() - 1], pack);

		write_data(*this, pack);
	}
}

void state_t::tick(std::chrono::duration<float> dTime) {
	m_LockState = 1;
	// remove filters that are no longer in use
	m_Filters.erase(std::remove_if(begin(m_Filters),
								   end(m_Filters),
								   [](const filter_result& res) { return res.group.use_count() <= 1; }),
					end(m_Filters));

	auto modified_entities =
	  psl::array<entity_t> {(entity_t*)m_ModifiedEntities.indices().data(),
							(entity_t*)m_ModifiedEntities.indices().data() + m_ModifiedEntities.indices().size()};
	invoke<entity_t::size_type>(
	  [](auto... args) { std::sort(args...); }, modified_entities.begin(), modified_entities.end());

	// apply filterings
	for(auto& filter_result : m_Filters) {
		filter(filter_result, modified_entities);
	}

	m_ModifiedEntities.clear();

	// tick systems;
	for(auto& system : m_SystemInformations) {
		prepare_system(dTime, dTime, (std::uintptr_t)m_Cache.data(), system);
	}

	m_Orphans.insert(std::end(m_Orphans), std::begin(m_ToBeOrphans), std::end(m_ToBeOrphans));
	m_ToBeOrphans.clear();

	for(auto& [key, cInfo] : m_Components) cInfo->purge();

	for(auto& info : info_buffer) {
		execute_command_buffer(*info);
	}
	info_buffer.clear();

	// purge;
	++m_Tick;

	if(m_NewSystemInformations.size() > 0) {
		for(auto& system : m_NewSystemInformations) m_SystemInformations.emplace_back(std::move(system));
		m_NewSystemInformations.clear();
	}

	if(m_ToRevoke.size() > 0) {
		for(auto id : m_ToRevoke) {
			revoke(id);
		}
		m_ToRevoke.clear();
	}
	m_LockState = 0;
}

details::component_container_t* state_t::get_component_container(const details::component_key_t& key) const noexcept {
	if(auto it = m_Components.find(key); it != std::end(m_Components))
		return it->second.get();
	else
		return nullptr;
}

details::component_container_t* state_t::get_component_container(const details::component_key_t& key) noexcept {
	if(auto it = m_Components.find(key); it != std::end(m_Components))
		return it->second.get();
	else
		return nullptr;
}

psl::array<details::component_container_t*>
state_t::get_component_container(psl::array_view<details::cached_container_entry_t> entries) const noexcept {
	psl::array<details::component_container_t*> res {};
	for(const auto& entry : entries) {
		if(entry.container != nullptr) {
			res.emplace_back(entry.container);
		} else {
			res.emplace_back(get_component_container(entry.key));
		}
	}
	return res;
}

psl::array<const details::component_container_t*>
state_t::get_component_container(psl::array_view<details::component_key_t> keys) const noexcept {
	psl::array<const details::component_container_t*> res {};
	size_t count = keys.size();
	for(const auto& [key, cInfo] : m_Components) {
		if(count == 0)
			break;
		if(auto it = std::find(std::begin(keys), std::end(keys), key); it != std::end(keys)) {
			res.push_back(cInfo.get());
			--count;
		}
	}
	return res;
}

// empty construction
void state_t::add_component_impl(details::component_container_t* cInfo, psl::array_view<entity_t> entities) {
	psl_assert(cInfo != nullptr, "component info for key {} was not found", cInfo->id());

	cInfo->add(entities);
	for(size_t i = 0; i < entities.size(); ++i)
		m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(entities[i]));
}

void state_t::add_component_impl(const details::component_key_t& key, psl::array_view<entity_t> entities) {
	auto cInfo = get_component_container(key);
	add_component_impl(cInfo, entities);
}

// prototype based construction
void state_t::add_component_impl(details::component_container_t* cInfo,
								 psl::array_view<entity_t> entities,
								 void* prototype,
								 bool repeat) {
	psl_assert(cInfo != nullptr, "component info for key {} was not found", cInfo->id());
	const auto component_size = cInfo->component_size();
	psl_assert(component_size != 0, "component size was 0");

	auto offset = cInfo->entities().size();

	cInfo->add(entities, prototype, repeat);
	for(size_t i = 0; i < entities.size(); ++i)
		m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(entities[i]));
}
void state_t::add_component_impl(const details::component_key_t& key,
								 psl::array_view<entity_t> entities,
								 void* prototype,
								 bool repeat) {
	auto cInfo = get_component_container(key);
	add_component_impl(cInfo, entities, prototype, repeat);
}


void state_t::remove_component(details::component_container_t* cInfo, psl::array_view<entity_t> entities) noexcept {
	psl_assert(cInfo != nullptr, "component info for key {} was not found", cInfo->id());
	cInfo->destroy(entities);
	for(size_t i = 0; i < entities.size(); ++i)
		m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(entities[i]));
}
void state_t::remove_component(const details::component_key_t& key, psl::array_view<entity_t> entities) noexcept {
	auto cInfo = get_component_container(key);
	remove_component(cInfo, entities);
}

// consider an alias feature
// ie: alias transform = position, rotation, scale components
void state_t::destroy(psl::array_view<entity_t> entities) noexcept {
	if(entities.size() == 0)
		return;

	for(auto& [key, cInfo] : m_Components) {
		cInfo->destroy(entities);
	}

	m_ToBeOrphans.insert(std::end(m_ToBeOrphans), std::begin(entities), std::end(entities));
	for(size_t i = 0; i < entities.size(); ++i)
		m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(entities[i]));
}

void state_t::destroy(entity_t entity) noexcept {
	for(auto& [key, cInfo] : m_Components) {
		cInfo->destroy(entity);
	}
	m_ToBeOrphans.emplace_back(entity);
	m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(entity));
}

void state_t::reset(psl::array_view<entity_t> entities) noexcept {
	for(auto& [key, cInfo] : m_Components) {
		cInfo->destroy(entities);
	}
}

psl::array<entity_t>::iterator state_t::filter_op(details::cached_container_entry_t& entry,
												  psl::array<entity_t>::iterator& begin,
												  psl::array<entity_t>::iterator& end) const noexcept {
	if(!entry.container) {
		entry.container = get_component_container(entry.key);
	}
	return (entry.container == nullptr)
			 ? begin
			 : std::partition(begin, end, [&entry](entity_t e) { return entry.container->has_component(e); });
}

psl::array<entity_t>::iterator state_t::on_add_op(details::cached_container_entry_t& entry,
												  psl::array<entity_t>::iterator& begin,
												  psl::array<entity_t>::iterator& end) const noexcept {
	if(!entry.container) {
		entry.container = get_component_container(entry.key);
	}
	return (entry.container == nullptr)
			 ? begin
			 : std::partition(begin, end, [&entry](entity_t e) { return entry.container->has_added(e); });
}

psl::array<entity_t>::iterator state_t::on_remove_op(details::cached_container_entry_t& entry,
													 psl::array<entity_t>::iterator& begin,
													 psl::array<entity_t>::iterator& end) const noexcept {
	if(!entry.container) {
		entry.container = get_component_container(entry.key);
	}
	return (entry.container == nullptr)
			 ? begin
			 : std::partition(begin, end, [&entry](entity_t e) { return entry.container->has_removed(e); });
}

psl::array<entity_t>::iterator state_t::on_except_op(details::cached_container_entry_t& entry,
													 psl::array<entity_t>::iterator& begin,
													 psl::array<entity_t>::iterator& end) const noexcept {
	if(!entry.container) {
		entry.container = get_component_container(entry.key);
	}
	return (entry.container == nullptr)
			 ? end
			 : std::partition(begin, end, [&entry](entity_t e) { return !entry.container->has_component(e); });
}

psl::array<entity_t>::iterator state_t::on_break_op(psl::array<details::cached_container_entry_t>& entries,
													psl::array<entity_t>::iterator& begin,
													psl::array<entity_t>::iterator& end) const noexcept {
	for(auto& entry : entries) {
		if(entry.container == nullptr) {
			entry.container = get_component_container(entry.key);
		}
	}
	return (std::any_of(entries.begin(), entries.end(), [](const auto& cache) { return cache.container == nullptr; }))
			 ? begin
			 :
			 // for every entity, remove if...
			 std::partition(begin, end, [&entries](entity_t e) {
				 return
				   // any of them have not had an entity removed
				   !(!std::any_of(std::begin(entries),
								  std::end(entries),
								  [e](const auto& entry) { return entry.container->has_removed(e); }) ||
					 // or all of them do not have a component, or had the entity removed
					 !std::all_of(std::begin(entries), std::end(entries), [e](const auto& entry) {
						 return entry.container->has_component(e) || entry.container->has_removed(e);
					 }));
			 });
}

psl::array<entity_t>::iterator state_t::on_combine_op(psl::array<details::cached_container_entry_t>& entries,
													  psl::array<entity_t>::iterator& begin,
													  psl::array<entity_t>::iterator& end) const noexcept {
	for(auto& entry : entries) {
		if(entry.container == nullptr) {
			entry.container = get_component_container(entry.key);
		}
	}
	return (std::any_of(entries.begin(), entries.end(), [](const auto& cache) { return cache.container == nullptr; }))
			 ? begin
			 : std::remove_if(begin, end, [entries](entity_t e) {
				   return !std::any_of(std::begin(entries), std::end(entries), [e](const auto& entry) {
					   return entry.container->has_added(e);
				   }) || !std::all_of(std::begin(entries), std::end(entries), [e](const auto& entry) {
					   return entry.container->has_component(e);
				   });
			   });
}

psl::array<entity_t> state_t::filter(const details::dependency_pack& pack, bool seed_with_previous) const noexcept {
	auto pack_filters = pack.filters;
	for(const auto& [key, arr] : pack.m_RBindings) pack_filters.emplace_back(key);
	for(const auto& [key, arr] : pack.m_RWBindings) pack_filters.emplace_back(key);

	details::filter_group group {
	  pack_filters, pack.on_add, pack.on_remove, pack.except, pack.on_combine, pack.on_break};

	auto it = std::find_if(
	  std::begin(m_Filters), std::end(m_Filters), [&group](const filter_result& data) { return *data.group == group; });

	psl_assert(it != std::end(m_Filters), "could not find filter group for pack, could indicate a sync issue");


	auto entities = it->entities;

	if(seed_with_previous) {
		filter_result data {{}, std::make_shared<details::filter_group>(group)};
		filter(data, seed_with_previous);
		entities = data.entities;
	}

	auto begin = std::begin(entities);
	auto end   = std::end(entities);

	for(const auto& conditional : pack.on_condition) {
		end = std::invoke(conditional, begin, end, *this);
	}

	std::invoke(pack.orderby, begin, end, *this);

	entities.erase(end, std::end(entities));

	psl_assert(std::all_of(std::begin(pack.filters),
						   std::end(pack.filters),
						   [this, &entities](auto filter) {
							   auto cInfo = get_component_container(filter);
							   return std::all_of(std::begin(entities),
												  std::end(entities),
												  [filter, &cInfo](entity_t e) { return cInfo->has_storage_for(e); });
						   }),
			   "not all components had storage for all entities");
	return entities;
}

void state_t::filter(filter_result& data, bool seed_with_previous) const noexcept {
	std::optional<psl::array_view<entity_t>> source;

	for(auto filter : data.group->on_remove) {
		auto cInfo = get_component_container(filter);
		if(!cInfo) {
			data.entities = {};
			return;
		}
		if(!source || cInfo->removed_entities().size() < source.value().size()) {
			source = cInfo->removed_entities();
		}
	}
	for(auto filter : data.group->on_break) {
		auto cInfo = get_component_container(filter);
		if(!cInfo) {
			data.entities = {};
			return;
		}
		if(!source || cInfo->entities(true).size() < source.value().size()) {
			source = cInfo->entities(true);
		}
	}
	for(auto filter : data.group->on_add) {
		auto cInfo = get_component_container(filter);
		if(!cInfo) {
			data.entities = {};
			return;
		}
		if(seed_with_previous) {
			if(!source || cInfo->entities().size() < source.value().size()) {
				source = cInfo->entities();
			}
		} else {
			if(!source || cInfo->added_entities().size() < source.value().size()) {
				source = cInfo->added_entities();
			}
		}
	}
	for(auto filter : data.group->on_combine) {
		auto cInfo = get_component_container(filter);
		if(!cInfo) {
			data.entities = {};
			return;
		}
		if(seed_with_previous) {
			if(!source || cInfo->entities().size() < source.value().size()) {
				source = cInfo->entities();
			}
		} else {
			if(!source || cInfo->entities().size() < source.value().size()) {
				source = cInfo->entities();
			}
		}
	}

	for(auto filter : data.group->filters) {
		auto cInfo = get_component_container(filter);
		if(!cInfo) {
			data.entities = {};
			return;
		}
		if(!source || cInfo->entities().size() < source.value().size()) {
			source = cInfo->entities();
		}
	}

	if(source) {
		psl::array<entity_t> result {source.value()};
		auto begin = std::begin(result);
		auto end   = std::end(result);

		for(auto filter : data.group->on_remove) {
			end = on_remove_op(filter, begin, end);
		}
		if(data.group->on_break.size() > 0) {
			end = on_break_op(data.group->on_break, begin, end);
		}
		for(auto filter : data.group->on_add) {
			if(seed_with_previous)
				end = filter_op(filter, begin, end);
			else
				end = on_add_op(filter, begin, end);
		}
		if(seed_with_previous) {
			for(auto filter : data.group->on_combine) {
				end = filter_op(filter, begin, end);
			}
		} else {
			if(data.group->on_combine.size() > 0)
				end = on_combine_op(data.group->on_combine, begin, end);
		}

		for(auto filter : data.group->filters) {
			end = filter_op(filter, begin, end);
		}
		for(auto filter : data.group->except) {
			end = on_except_op(filter, begin, end);
		}

		// todo support order_by and on_condition

		data.entities = {begin, end};
		return;
	}
}

void state_t::filter(filter_result& data, psl::array_view<entity_t> source) const noexcept {
	if(source.size() == 0) {
		if(data.group->clear_every_frame()) {
			data.entities.clear();
		}
	} else {
		psl::array<entity_t> result {source};
		auto begin = std::begin(result);
		auto end   = std::end(result);

		for(auto filter : data.group->on_remove) {
			end = on_remove_op(filter, begin, end);
		}
		if(data.group->on_break.size() > 0) {
			end = on_break_op(data.group->on_break, begin, end);
		}
		for(auto filter : data.group->on_add) {
			end = on_add_op(filter, begin, end);
		}
		if(data.group->on_combine.size() > 0)
			end = on_combine_op(data.group->on_combine, begin, end);


		for(auto filter : data.group->filters) {
			end = filter_op(filter, begin, end);
		}
		for(auto filter : data.group->except) {
			end = on_except_op(filter, begin, end);
		}

		invoke<entity_t::size_type>([](auto... args) { std::sort(args...); }, begin, end);
		if(data.group->clear_every_frame()) {
			result.erase(end, std::end(result));
			data.entities = std::move(result);

			// do normal operations here, we cannot save perf
			for(auto& transformation : data.transformations) {
				transformation.entities = data.entities;
				transformation.entities.erase(transformation.group->transform(std::begin(transformation.entities),
																			  std::end(transformation.entities),
																			  *this),
											  std::end(transformation.entities));
			}
		} else {
			invoke<entity_t::size_type>([](auto... args) { std::sort(args...); }, end, std::end(result));

			// todo support order_by and on_condition
			// if(false && data.transformations.size() > 0)
			//{
			//	for (auto& transformation : data.transformations)
			//	{
			//		continue;
			//		psl::array<entity_t> ordered_indices(transformation.entities.size());
			//		std::iota(std::begin(ordered_indices), std::end(ordered_indices), 0);

			//		auto zip = psl::zip(transformation.entities, transformation.indices, ordered_indices);

			//		// unwind existing entities
			//		// todo instead of sort, implement this as a sweeping swap
			//		psl::sorting::quick(std::begin(zip), std::end(zip), [](const auto& lhs, const auto& rhs)
			//			{
			//				return lhs.get<1>() < rhs.get<1>();
			//			});

			//		std::tuple<psl::array<entity_t>, psl::array<entity_t>, psl::array<entity_t>> diff_set{};

			//		// apply the normal merging operations, keeping track of index changes
			//		std::set_difference(std::begin(zip), std::end(zip), end, std::end(result),
			// special_inserter(diff_set),
			//			[](const auto& lhs, const auto& rhs)
			//			{
			//				if constexpr (std::is_same_v<decltype(lhs), const entity_t&>)
			//					return rhs.get<0>() < lhs;
			//				else
			//					return lhs.get<0>() < rhs; });

			//		transformation.entities = std::move(std::get<0>(diff_set));
			//		transformation.entities.insert(std::end(transformation.entities), begin, end);

			//		transformation.indices.resize(transformation.entities.size());
			//		std::iota(std::begin(transformation.indices), std::end(transformation.indices), 0);

			//		ordered_indices = std::move(std::get<2>(diff_set));
			//		auto size = ordered_indices.size();
			//		ordered_indices.resize(ordered_indices.size() + std::distance(begin, end));
			//		std::fill(std::next(std::begin(ordered_indices), size), std::next(std::begin(ordered_indices),
			// std::distance(begin, end)), std::numeric_limits<entity_t::size_type>::max());

			//		zip = psl::zip(transformation.entities, transformation.indices, ordered_indices);
			//		// unwind existing entities
			//		// todo instead of sort, implement this as a sweeping swap
			//		psl::sorting::quick(std::begin(zip), std::end(zip), [](const auto& lhs, const auto& rhs)
			//			{
			//				return lhs.get<2>() < rhs.get<2>();
			//			});


			//		// apply order_by and on_condition storing the index changes
			//	}
			//}
			// else
			{
				psl::array_view new_source {begin, end};
				psl::array<entity_t> diff_set {};
				std::set_difference((entity_t::size_type*)(data.entities.data()),
									(entity_t::size_type*)(data.entities.data()) + data.entities.size(),
									(entity_t::size_type*)(source.data()),
									(entity_t::size_type*)(source.data()) + source.size(),
									std::back_inserter(diff_set));
				data.entities = std::move(diff_set);

				auto size = std::size(data.entities);
				data.entities.insert(std::end(data.entities), begin, end);

				std::inplace_merge((entity_t::size_type*)(data.entities.data()),
								   (entity_t::size_type*)(data.entities.data()) + size,
								   (entity_t::size_type*)(data.entities.data()) + data.entities.size());
			}
		}
	}
	psl_assert(std::unique(std::begin(data.entities), std::end(data.entities)) == std::end(data.entities),
			   "some entities were not unique");
	psl_assert(std::all_of(std::begin(data.group->on_combine),
						   std::end(data.group->on_combine),
						   [this, &data](auto filter) {
							   auto cInfo = get_component_container(filter);
							   return std::all_of(std::begin(data.entities),
												  std::end(data.entities),
												  [filter, &cInfo](entity_t e) { return cInfo->has_storage_for(e); });
						   }),
			   "some components failed to have storage for the entities");
}

size_t state_t::prepare_data(psl::array_view<entity_t> entities, void* cache, component_key_t id) const noexcept {
	if(entities.size() == 0)
		return 0;
	const auto& cInfo = get_component_container(id);
	psl_assert(cInfo != nullptr, "component info was null for the key {}", id);
	psl_assert(
	  std::all_of(std::begin(entities), std::end(entities), [&cInfo](auto e) { return cInfo->has_storage_for(e); }),
	  "some components failed to have storage for the entities");
	psl_assert((std::uintptr_t)(cache) + (cInfo->component_size() * entities.size()) <=
				 (std::uintptr_t)(m_Cache.data()) + m_Cache.size(),
			   "Cache ran out of memory");
	return cInfo->copy_to(entities, cache);
}

size_t state_t::prepare_bindings(psl::array_view<entity_t> entities,
								 void* cache,
								 details::dependency_pack& dep_pack) const noexcept {
	size_t offset_start = (std::uintptr_t)cache;
	psl_assert((std::uintptr_t)(cache) + (sizeof(entity_t) * entities.size()) <=
				 (std::uintptr_t)(m_Cache.data()) + m_Cache.size(),
			   "Cache ran out of memory");
	std::memcpy(cache, entities.data(), sizeof(entity_t) * entities.size());
	dep_pack.m_Entities = psl::array_view<entity_t>(
	  (entity_t*)cache, (entity_t*)((std::uintptr_t)cache + (sizeof(entity_t) * entities.size())));

	cache = (void*)((std::uintptr_t)cache + (sizeof(entity_t) * entities.size()));

	if(dep_pack.is_direct_access()) {
		// this functional handles filling in the cache with the data for the given component
		// it offsets the `cache` every invocation with the amount the previous invocation added
		auto write_fn = [entities, &cache, this](auto& binding) {
			std::uintptr_t data_begin = (std::uintptr_t)cache;
			const auto& cInfo		  = get_component_container(binding.first);
			if(cInfo->component_size() > 0) {
				auto offset		= align(data_begin, cInfo->alignment());
				auto write_size = prepare_data(entities, (void*)data_begin, binding.first);
				cache			= (void*)((std::uintptr_t)cache + write_size + offset);
				binding.second	= psl::array_view<std::uintptr_t>((std::uintptr_t*)data_begin, (std::uintptr_t*)cache);
			}
		};

		std::for_each(std::begin(dep_pack.m_RBindings), std::end(dep_pack.m_RBindings), write_fn);
		std::for_each(std::begin(dep_pack.m_RWBindings), std::end(dep_pack.m_RWBindings), write_fn);
	} else {
		// this functional handles filling in the indirect offsets to the data so that we can reconstruct
		// an indirect_array_t
		auto view_fn = [entities, &cache, this](auto& binding) {
			std::uintptr_t data_begin = (std::uintptr_t)cache;
			const auto& cInfo		  = get_component_container(binding.first);
			if(cInfo->component_size() > 0) {
				auto offset = align(data_begin, alignof(entity_t));
				cache		= cInfo->write_memory_location_offsets_for(entities, (entity_t::size_type*)data_begin);
				binding.second.indices =
				  psl::array_view<entity_t::size_type>((entity_t::size_type*)data_begin, (entity_t::size_type*)cache);
				binding.second.data = cInfo->data();
			}
		};
		std::for_each(std::begin(dep_pack.m_IndirectReadBindings), std::end(dep_pack.m_IndirectReadBindings), view_fn);
		std::for_each(
		  std::begin(dep_pack.m_IndirectReadWriteBindings), std::end(dep_pack.m_IndirectReadWriteBindings), view_fn);
	}
	return (std::uintptr_t)cache - offset_start;
}

size_t state_t::set(psl::array_view<entity_t> entities, const details::component_key_t& key, void* data) noexcept {
	if(entities.size() == 0)
		return 0;
	const auto& cInfo = get_component_container(key);
	psl_assert(cInfo != nullptr, "component info for key {} was not found", key);
	return cInfo->copy_from(entities, data);
}


void state_t::execute_command_buffer(info_t& info) {
	auto& buffer = info.command_buffer;

	psl::sparse_array<entity_t::size_type> remapped_entities;
	if(buffer.m_Entities.size() > 0) {
		psl::array<entity_t> added_entities;
		std::set_difference((entity_t::size_type*)(buffer.m_Entities.data()),
							(entity_t::size_type*)(buffer.m_Entities.data()) + buffer.m_Entities.size(),
							(entity_t::size_type*)(buffer.m_DestroyedEntities.data()),
							(entity_t::size_type*)(buffer.m_DestroyedEntities.data()) +
							  buffer.m_DestroyedEntities.size(),
							std::back_inserter(added_entities));


		for(auto e : added_entities) {
			remapped_entities[static_cast<entity_t::size_type>(e)] = static_cast<entity_t::size_type>(create());
		}
	}
	for(auto& component_src : buffer.m_Components) {
		if(component_src->entities(true).size() == 0)
			continue;
		auto component_dst = get_component_container(component_src->id());

		component_src->remap(remapped_entities, [first = buffer.m_First](entity_t e) -> bool {
			return static_cast<entity_t::size_type>(e) >= first;
		});
		if(component_dst == nullptr) {
			auto entities = component_src->entities(true);
			for(auto e : entities) {
				m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(e));
			}
			m_Components[component_src->id()] = std::move(component_src);
		} else {
			component_dst->merge(*component_src);
			for(auto e : component_src->entities(true))
				m_ModifiedEntities.try_insert(static_cast<entity_t::size_type>(e));
		}
	}
	auto destroyed_entities = buffer.m_DestroyedEntities;
	auto mid =
	  std::partition(std::begin(destroyed_entities), std::end(destroyed_entities), [first = buffer.m_First](auto e) {
		  return static_cast<entity_t::size_type>(e) >= first;
	  });
	if(mid != std::end(destroyed_entities))
		destroy(
		  psl::array_view<entity_t> {&*mid, static_cast<size_t>(std::distance(mid, std::end(destroyed_entities)))});
}


size_t state_t::size(psl::array_view<details::component_key_t> keys) const noexcept {
	for(auto& key : keys) {
		auto cInfo = get_component_container(key);
		return cInfo ? cInfo->size() : 0;
	}
	return 0;
}

void state_t::clear(bool release_memory) noexcept {
	if(release_memory) {
		m_Components = decltype(m_Components) {};
	} else {
		for(auto& [key, storage] : m_Components) {
			storage->clear();
		}
	}

	m_Tick	   = 0;
	m_Entities = 0;
	m_Orphans.clear();
	m_ToBeOrphans.clear();
	m_SystemInformations.clear();
	m_NewSystemInformations.clear();
	m_Filters.clear();
	m_LockState = 0;
	m_ModifiedEntities.clear();
	m_ToRevoke.clear();
	++m_ComponentGeneration;
}
