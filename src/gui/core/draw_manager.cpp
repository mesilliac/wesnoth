/*
	Copyright (C) 2007 - 2022
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#include "gui/core/draw_manager.hpp"

#include "gui/core/top_level_drawable.hpp"
#include "sdl/rect.hpp"

#include <SDL2/SDL_rect.h>
#include <vector>
#include <map>
#include <iostream>

namespace {
std::vector<gui2::top_level_drawable*> top_level_drawables_;
std::map<gui2::top_level_drawable*, std::vector<SDL_Rect>> animations_;
std::vector<SDL_Rect> invalidated_regions_;
} // namespace

namespace gui2::draw_manager {

void invalidate_region(const SDL_Rect& region)
{
	std::cerr << "invalidating region " << region << std::endl;
	invalidated_regions_.push_back(region);
	// For now we store all the invalidated regions separately.
	// Several avenues for optimization exist, some rather straightforward.
	// This can be looked into if it becomes common to invalidate multiple
	// regions per frame. This is unlikely for the moment, as map animation
	// is still handled internally by the "display" class.
}

bool draw()
{
	//std::cerr << ".";
	// For now just send all regions to all TLDs in the correct order.
	bool drawn = false;
	while (!invalidated_regions_.empty()) {
		std::cerr << "+";
		SDL_Rect r = invalidated_regions_.back();
		invalidated_regions_.pop_back();
		for (auto tld : top_level_drawables_) {
			std::cerr << "*";
			drawn |= tld->expose(r);
		}
	}
	// Also expose animations, as necessary.
	for (auto [tld, regions] : animations_) {
		// very basic for now
		std::cerr << "@";
		for (auto r : regions) {
			drawn |= tld->expose(r);
		}
	}
	return drawn;
}

void register_drawable(gui2::top_level_drawable* tld)
{
	std::cerr << "registering TLD " << static_cast<void*>(tld) << std::endl;
	top_level_drawables_.push_back(tld);
}

void unregister_drawable(gui2::top_level_drawable* tld)
{
	std::cerr << "deregistering TLD " << static_cast<void*>(tld) << std::endl;
	for (auto it = top_level_drawables_.begin();
		it != top_level_drawables_.end();
		++it)
	{
		if (*it == tld) {
			top_level_drawables_.erase(it);
			return;
		}
	}
	animations_.erase(tld);
}

void register_static_animation(gui2::top_level_drawable* tld, const SDL_Rect& r)
{
	animations_[tld].push_back(r);
}

} // namespace gui2::draw_manager
