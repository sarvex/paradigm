﻿#include "psl/memory/range.h"
#include "psl/assertions.h"

using namespace memory;

range_t::range_t(std::uintptr_t begin, std::uintptr_t end) : begin(begin), end(end) { assert(begin <= end); };

bool range_t::operator==(const range_t& other) const { return (other.begin == begin && other.end == end); }


bool range_t::operator!=(const range_t& other) const { return (other.begin != begin || other.end != end); }

bool range_t::operator<(const range_t& other) const { return (begin < other.begin); }

bool range_t::operator>(const range_t& other) const { return (begin > other.begin); }
bool range_t::operator<=(const range_t& other) const { return (begin <= other.begin); }

bool range_t::operator>=(const range_t& other) const { return (begin >= other.begin); }

bool range_t::contains(const range_t& other) const { return other.begin >= begin && other.end <= end; }

bool range_t::is_contained_by(const range_t& other) const { return begin >= other.begin && end <= other.end; }

bool range_t::overlaps(const range_t& other) const
{
	return (other.begin >= begin && other.begin <= end) || (other.end >= begin && other.end <= end);
}

bool range_t::touches(const range_t& other) const { return other.begin == end || other.end == begin; }

size_t range_t::size() const noexcept { return end - begin; };
