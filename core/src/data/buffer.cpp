﻿#include "data/buffer.h"
#include "resource/resource.hpp"
#include "logging.h"
using namespace psl;
using namespace core::data;

buffer::buffer(core::resource::cache& cache, const core::resource::metadata& metaData, psl::meta::file* metaFile,
			   core::gfx::memory_usage usage, core::gfx::memory_property memoryPropertyFlags,
			   memory::region&& memory_region) noexcept
	: m_Region(std::move(memory_region)), m_Usage(usage), m_MemoryPropertyFlags(memoryPropertyFlags)
{}

buffer::~buffer() {}


std::optional<memory::segment> buffer::allocate(size_t size)
{
	if(auto segm = m_Region.allocate(size); segm)
	{
		m_Segments.push_back(segm.value());
		return segm;
	}
	return std::optional<memory::segment>{};
}
bool buffer::deallocate(memory::segment& segment)
{
	const memory::range range{segment.range()};
	auto find_it = std::find_if(std::begin(m_Segments), std::end(m_Segments),
								[&range](const memory::segment& entry) { return entry.range() == range; });
	if(find_it == std::end(m_Segments))
	{
		core::data::log->critical("could not erase the range {0} - {1}", range.begin, range.end);
		return false;
	}

	if(m_Region.deallocate(segment))
	{
		std::rotate(find_it, std::next(find_it), std::end(m_Segments));
		m_Segments.erase(std::prev(std::end(m_Segments)), std::end(m_Segments));
		return true;
	}
	
	core::data::log->warn("tried to erase a range {0} - {1} that was not present in the buffer", range.begin,
							  range.end);
	return false;
}

size_t buffer::size() const
{
	return m_Region.size();
	// return std::accumulate(std::begin(m_Segments), std::end(m_Segments), (size_t)0u,
	//					   [](size_t sum, const memory::segment& segment) { return sum + segment.range().size(); });
}
core::gfx::memory_usage buffer::usage() const { return m_Usage.value; }
core::gfx::memory_property buffer::memoryPropertyFlags() const { return m_MemoryPropertyFlags.value; }
const memory::region& buffer::region() const { return m_Region; }
const std::vector<memory::segment>& buffer::segments() const { return m_Segments; };

bool buffer::transient() const noexcept { return m_Transient.value; }
void buffer::transient(bool value) noexcept { m_Transient.value = value; }
core::gfx::memory_write_frequency buffer::write_frequency() const noexcept { return m_WriteFrequency.value; }
void buffer::write_frequency(core::gfx::memory_write_frequency value) noexcept { m_WriteFrequency.value = value; }