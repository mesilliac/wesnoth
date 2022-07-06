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

#include "exceptions.hpp"
#include "log.hpp"
#include "gui/core/top_level_drawable.hpp"
#include "preferences/general.hpp"
#include "sdl/rect.hpp"
#include "video.hpp"

#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_timer.h>

#include <vector>
#include <map>
#include <iostream>

namespace {
std::vector<gui2::top_level_drawable*> top_level_drawables_;
std::map<gui2::top_level_drawable*, std::vector<SDL_Rect>> animations_;
std::vector<rect> invalidated_regions_;
bool drawing_ = false;
uint32_t last_sparkle_ = 0;
} // namespace

namespace gui2::draw_manager {

void render();

void invalidate_region(const rect& region)
{
	if (drawing_) {
		ERR_GUI_D << "Attempted to invalidate region " << region
			<< " during draw" << std::endl;
		return;
	}

	// On-add region optimization
	rect progressive_cover = region;
	int64_t cumulative_area = 0;
	for (auto& r : invalidated_regions_) {
		if (r.contains(region)) {
			// An existing invalidated region already contains it,
			// no need to do anything in this case.
			//std::cerr << "no need to invalidate " << region << std::endl;
			//std::cerr << '.';
			return;
		}
		if (region.contains(r)) {
			// This region contains a previously invalidated region,
			// might as well superceded it with this.
			//std::cerr << "superceding previous invalidation " << r
			//	<< " with " << region << std::endl;
			std::cerr << '\'';
			r = region;
			return;
		}
		// maybe merge with another rect
		rect m = r.minimal_cover(region);
		if (m.area() <= r.area() + region.area()) {
			// This won't always be the best,
			// but it also won't ever be the worst.
			//std::cerr << "merging " << region << " with " << r
			//	<< " to invalidate " << m << std::endl;
			std::cerr << ':';
			r = m;
			return;
		}
		// maybe merge *all* the rects */
		progressive_cover.expand_to_cover(r);
		cumulative_area += r.area();
		if (progressive_cover.area() <= cumulative_area) {
			//std::cerr << "conglomerating invalidations to "
			//	<< progressive_cover << std::endl;
			std::cerr << '%';
			// replace the first one, so we can easily prune later
			invalidated_regions_[0] = progressive_cover;
			return;
		}
	}

	// No optimization was found, so add a new invalidation
	//std::cerr << "invalidating region " << region << std::endl;
	std::cerr << '.';
	invalidated_regions_.push_back(region);
}

void sparkle()
{
	if (drawing_) {
		ERR_GUI_D << "Draw recursion detected" << std::endl;
		return;
	}

	draw_manager::layout();

	draw_manager::render();

	if (draw_manager::draw()) {
		CVideo::get_singleton().render_screen();
	} else if (preferences::vsync()) { // TODO: draw_manager - does anyone ever really want this not to rate limit?
		int rr = CVideo::get_singleton().current_refresh_rate();
		if (rr <= 0) {
			// make something up
			rr = 50;
		}
		int vsync_delay = (1000 / rr) - 1;
		int time_to_wait = last_sparkle_ + vsync_delay - SDL_GetTicks();
		if (time_to_wait > 0) {
			//std::cerr << "sparkle waiting for " << time_to_wait << "ms"
			//	<< " out of " << vsync_delay << "ms" << std::endl;
			SDL_Delay(std::min(time_to_wait, 1000));
		}
	}
	last_sparkle_ = SDL_GetTicks();
}

// TODO: draw_manager - rename to include animation, or split animation out
void layout()
{
	for (auto tld : top_level_drawables_) {
		tld->layout();
	}
}

// TODO: draw_manager - do animations get invalidated here or in layout?
void render()
{
	for (auto tld : top_level_drawables_) {
		tld->render();
	}
}

bool draw()
{
	// TODO: draw_manager - some things were skipping draw when video is faked. Should this skip all in this case?
	drawing_ = true;
	//std::cerr << ".";

	// For now just send all regions to all TLDs in the correct order.
	bool drawn = false;
next:
	while (!invalidated_regions_.empty()) {
		rect r = invalidated_regions_.back();
		invalidated_regions_.pop_back();
		// check if this will be superceded by or should be merged with another
		for (auto& other : invalidated_regions_) {
			// r will never contain other, due to construction
			if (other.contains(r)) {
				std::cerr << "-";
				goto next;
			}
			rect m = other.minimal_cover(r);
			if (m.area() <= r.area() + other.area()) {
				other = m;
				std::cerr << "=";
				goto next;
			}
		}
		std::cerr << "+";
		for (auto tld : top_level_drawables_) {
			rect i = r.intersect(tld->screen_location());
			if (i.empty()) {
				std::cerr << "x";
				continue;
			}
			std::cerr << "*";
			drawn |= tld->expose(i);
		}
	}
	// TODO: draw_manager - replace or overhaul this
	// Also expose animations, as necessary.
	for (auto& [tld, regions] : animations_) {
		// very basic for now
		//std::cerr << "@";
		for (auto& r : regions) {
			drawn |= tld->expose(r);
		}
	}
	drawing_ = false;
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
	// Erase tld from top_level_drawables
	auto& vec = top_level_drawables_;
	vec.erase(std::remove(vec.begin(), vec.end(), tld), vec.end());

	// Remove any linked animations
	animations_.erase(tld);
}

void register_static_animation(gui2::top_level_drawable* tld, const SDL_Rect& r)
{
	animations_[tld].push_back(r);
}

} // namespace gui2::draw_manager
