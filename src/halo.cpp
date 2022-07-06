/*
	Copyright (C) 2003 - 2022
	by David White <dave@whitevine.net>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

/**
 * @file
 * Maintain halo-effects for units and items.
 * Examples: white mage, lighthouse.
 */

#include "animated.hpp"
#include "display.hpp"
#include "draw.hpp"
#include "preferences/game.hpp"
#include "gui/core/draw_manager.hpp"
#include "halo.hpp"
#include "log.hpp"
#include "serialization/string_utils.hpp"
#include "sdl/rect.hpp"
#include "sdl/texture.hpp"

#include <iostream>

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)
#define WRN_DP LOG_STREAM(warn, log_display)
#define LOG_DP LOG_STREAM(info, log_display)
#define DBG_DP LOG_STREAM(debug, log_display)

namespace halo
{

class halo_impl
{

class effect
{
public:
	effect(display * screen, int xpos, int ypos, const animated<image::locator>::anim_description& img,
			const map_location& loc, ORIENTATION, bool infinite);

	void set_location(int x, int y);
	rect get_draw_location();

	void invalidate();
	void update();
	bool render();

	bool expired()     const { return !images_.cycles() && images_.animation_finished(); }
	bool need_update() const { return images_.need_update(); }
	bool does_change() const { return !images_.does_not_change(); }
	bool on_location(const std::set<map_location>& locations) const;
	bool location_not_known() const;

	void add_overlay_location(std::set<map_location>& locations);
private:

	const image::locator& current_image() const { return images_.get_current_frame(); }

	animated<image::locator> images_;

	ORIENTATION orientation_;

	int x_, y_, w_, h_;
	texture tex_, buffer_;
	rect rect_, buffer_pos_;

	/** The location of the center of the halo. */
	map_location loc_;

	/** All locations over which the halo lies. */
	std::vector<map_location> overlayed_hexes_;

	display * disp;
};

display* disp;

std::map<int, effect> haloes;
int halo_id;

/**
 * Upon unrendering, an invalidation list is send. All haloes in that area and
 * the other invalidated haloes are stored in this set. Then there'll be
 * tested which haloes overlap and they're also stored in this set.
 */
std::set<int> invalidated_haloes;

/**
 * Upon deleting, a halo isn't deleted but added to this set, upon unrendering
 * the image is unrendered and deleted.
 */
std::set<int> deleted_haloes;

/**
 * Haloes that have an animation or expiration time need to be checked every
 * frame and are stored in this set.
 */
std::set<int> changing_haloes;

public:
/**
 * impl's of exposed functions
 */

explicit halo_impl(display & screen) :
	disp(&screen),
	haloes(),
	halo_id(1),
	invalidated_haloes(),
	deleted_haloes(),
	changing_haloes()
{}


int add(int x, int y, const std::string& image, const map_location& loc,
		ORIENTATION orientation=NORMAL, bool infinite=true);

/** Set the position of an existing haloing effect, according to its handle. */
void set_location(int handle, int x, int y);

/** Remove the halo with the given handle. */
void remove(int handle);

void update();
void render();

}; //end halo_impl

halo_impl::effect::effect(display * screen, int xpos, int ypos, const animated<image::locator>::anim_description& img,
		const map_location& loc, ORIENTATION orientation, bool infinite) :
	images_(img),
	orientation_(orientation),
	x_(0),
	y_(0),
	w_(0),
	h_(0),
	tex_(),
	buffer_(),
	rect_(sdl::empty_rect),
	buffer_pos_(sdl::empty_rect),
	loc_(loc),
	overlayed_hexes_(),
	disp(screen)
{
	assert(disp != nullptr);

	set_location(xpos,ypos);

	images_.start_animation(0,infinite);

}

void halo_impl::effect::set_location(int x, int y)
{
	// TODO: draw_manager - tidy...
	int new_x = x - disp->get_location_x(map_location::ZERO());
	int new_y = y - disp->get_location_y(map_location::ZERO());
	if (new_x != x_ || new_y != y_) {
		x_ = new_x;
		y_ = new_y;
		buffer_.reset();
		overlayed_hexes_.clear();
	}
}

rect halo_impl::effect::get_draw_location()
{
	// TODO: draw_manager - this probably isn't always up to date yet?
	return rect_;
}

/** Update the current location, animation frame, etc. */
void halo_impl::effect::update()
{
	if(disp == nullptr) {
		// Why would this ever be the case?
		WRN_DP << "trying to update halo with null display" << std::endl;
		rect_ = {};
		return;
	}

	if(loc_.x != -1 && loc_.y != -1) {
		// The location of a halo is an x,y value and not a map location.
		// This means when a map is zoomed, the halo's won't move,
		// This glitch is most visible on [item] haloes.
		// This workaround always recalculates the location of the halo
		// (item haloes have a location parameter to hide them under the shroud)
		// and reapplies that location.
		// It might be optimized by storing and comparing the zoom value.
		set_location(
			disp->get_location_x(loc_) + disp->hex_size() / 2,
			disp->get_location_y(loc_) + disp->hex_size() / 2);
	}

	images_.update_last_draw_time();
	tex_ = image::get_texture(current_image());
	if(!tex_) {
		rect_ = {};
		return;
	}
	w_ = int(tex_.w() * disp->get_zoom_factor());
	h_ = int(tex_.h() * disp->get_zoom_factor());

	const int screenx = disp->get_location_x(map_location::ZERO());
	const int screeny = disp->get_location_y(map_location::ZERO());

	const int xpos = x_ + screenx - w_/2;
	const int ypos = y_ + screeny - h_/2;

	rect_ = {xpos, ypos, w_, h_};
}

bool halo_impl::effect::render()
{
	if(disp == nullptr) {
		// Why would this ever be the case?
		WRN_DP << "trying to render halo with null display" << std::endl;
		return false;
	}

	if(loc_.x != -1 && loc_.y != -1 && disp->shrouded(loc_)) {
		// TODO: draw_manager - should this always be so? what if the edge of the halo peeks out of the shroud?
		DBG_DP << "not rendering shrouded halo" << std::endl;
		return false;
	}

	rect clip_rect = disp->map_outside_area();

	// If rendered the first time, need to determine the area affected.
	// If a halo changes size, it is not updated.
	if(location_not_known()) {
		display::rect_of_hexes hexes = disp->hexes_under_rect(rect_);
		display::rect_of_hexes::iterator i = hexes.begin(), end = hexes.end();
		for (;i != end; ++i) {
			// TODO: draw_manager - this can probably be completely removed
			overlayed_hexes_.push_back(*i);
		}
	}

	if(!clip_rect.overlaps(rect_)) {
		DBG_DP << "halo outside clip" << std::endl;
		buffer_.reset();
		return false;
	}

	auto clipper = draw::reduce_clip(clip_rect);

	buffer_pos_ = rect_;
	//buffer_ = disp->video().read_texture(&buffer_pos_);

	DBG_DP << "drawing halo at " << rect_ << std::endl;

	if (orientation_ == NORMAL) {
		draw::blit(tex_, rect_);
	} else {
		draw::flipped(tex_, rect_,
			orientation_ == HREVERSE || orientation_ == HVREVERSE,
			orientation_ == VREVERSE || orientation_ == HVREVERSE);
	}

	return true;
}

void halo_impl::effect::invalidate()
{
	// TODO: draw_manager - remove buffer_ texture
	if (!tex_/* || !buffer_*/) {
		return;
	}

	// Shrouded haloes are never rendered unless shroud has been re-placed; in
	// that case, unrendering causes the hidden terrain (and previous halo
	// frame, when dealing with animated halos) to glitch through shroud. We
	// don't need to unrender them because shroud paints over the underlying
	// area anyway.
	if (loc_.x != -1 && loc_.y != -1 && disp->shrouded(loc_)) {
		DBG_DP << "shrouded or unpositioned halo" << std::endl;
		// TODO: draw_manager - probably should redo this
		return;
	}


	SDL_Rect clip_rect = disp->map_outside_area();
	auto clipper = draw::set_clip(clip_rect);

	// Due to scrolling, the location of the rendered halo
	// might have changed; recalculate
	const int screenx = disp->get_location_x(map_location::ZERO());
	const int screeny = disp->get_location_y(map_location::ZERO());

	const int xpos = x_ + screenx - w_/2;
	const int ypos = y_ + screeny - h_/2;

	buffer_pos_.x += xpos - rect_.x;
	buffer_pos_.y += ypos - rect_.y;

	DBG_DP << "invalidating halo" << buffer_pos_ << std::endl;

	gui2::draw_manager::invalidate_region(buffer_pos_);
	//draw::blit(buffer_, buffer_pos_);
}

bool halo_impl::effect::on_location(const std::set<map_location>& locations) const
{
	for(std::vector<map_location>::const_iterator itor = overlayed_hexes_.begin();
			itor != overlayed_hexes_.end(); ++itor) {
		if(locations.find(*itor) != locations.end()) {
			return true;
		}
	}
	return false;
}

bool halo_impl::effect::location_not_known() const
{
	return overlayed_hexes_.empty();
}

void halo_impl::effect::add_overlay_location(std::set<map_location>& locations)
{
	for(std::vector<map_location>::const_iterator itor = overlayed_hexes_.begin();
			itor != overlayed_hexes_.end(); ++itor) {

		locations.insert(*itor);
	}
}

// End halo_impl::effect impl's

int halo_impl::add(int x, int y, const std::string& image, const map_location& loc,
		ORIENTATION orientation, bool infinite)
{
	const int id = halo_id++;
	animated<image::locator>::anim_description image_vector;
	std::vector<std::string> items = utils::square_parenthetical_split(image, ',');

	for(const std::string& item : items) {
		const std::vector<std::string>& sub_items = utils::split(item, ':');
		std::string str = item;
		int time = 100;

		if(sub_items.size() > 1) {
			str = sub_items.front();
			try {
				time = std::stoi(sub_items.back());
			} catch(const std::invalid_argument&) {
				ERR_DP << "Invalid time value found when constructing halo: " << sub_items.back() << "\n";
			}
		}
		image_vector.push_back(animated<image::locator>::frame_description(time,image::locator(str)));

	}
	haloes.emplace(id, effect(disp, x, y, image_vector, loc, orientation, infinite));
	invalidated_haloes.insert(id);
	if(haloes.find(id)->second.does_change() || !infinite) {
		changing_haloes.insert(id);
	}
	return id;
}

void halo_impl::set_location(int handle, int x, int y)
{
	const std::map<int,effect>::iterator itor = haloes.find(handle);
	if(itor != haloes.end()) {
		itor->second.set_location(x,y);
	}
}

void halo_impl::remove(int handle)
{
	// Silently ignore invalid haloes.
	// This happens when Wesnoth is being terminated as well.
	if(handle == NO_HALO || haloes.find(handle) == haloes.end())  {
		return;
	}

	deleted_haloes.insert(handle);
}

void halo_impl::update()
{
	if(haloes.empty()) {
		return;
	}

	// Mark expired haloes for removal
	for(auto& [id, effect] : haloes) {
		if(effect.expired()) {
			DBG_DP << "expiring halo " << id << std::endl;
			deleted_haloes.insert(id);
		}
	}

	// Invalidate deleted halos
	for(int id : deleted_haloes) {
		DBG_DP << "invalidating deleted halo " << id << std::endl;
		haloes.at(id).invalidate();
	}

	// Invalidate any animated halos which need updating
	for(int id : changing_haloes) {
		auto& halo = haloes.at(id);
		if(halo.need_update()) {
			DBG_DP << "invalidating changed halo " << id << std::endl;
			halo.invalidate();
		}
	}

	// Now actually delete the halos that need deleting
	for(int id : deleted_haloes) {
		DBG_DP << "deleting halo " << id << std::endl;
		changing_haloes.erase(id);
		haloes.erase(id);
	}

	deleted_haloes.clear();
}

void halo_impl::render()
{
	if(haloes.empty()) {
		return;
	}

	// TODO: draw_manager - pass in rect in stead of assuming clip makes sense
	rect clip = draw::get_clip();

	for(auto& [id, effect] : haloes) {
		effect.update();
		if(clip.overlaps(effect.get_draw_location())) {
			DBG_DP << "drawing intersected halo " << id << std::endl;
			effect.render();
		}
	}
}

// end halo_impl implementations

// begin halo::manager

manager::manager(display& screen) : impl_(new halo_impl(screen))
{}

handle manager::add(int x, int y, const std::string& image, const map_location& loc,
		ORIENTATION orientation, bool infinite)
{
	int new_halo = impl_->add(x,y,image, loc, orientation, infinite);
	return handle(new halo_record(new_halo, impl_));
}

/** Set the position of an existing haloing effect, according to its handle. */
void manager::set_location(const handle & h, int x, int y)
{
	impl_->set_location(h->id_,x,y);
}

/** Remove the halo with the given handle. */
void manager::remove(const handle & h)
{
	impl_->remove(h->id_);
	h->id_ = NO_HALO;
}

void manager::update()
{
	impl_->update();
}

void manager::render()
{
	impl_->render();
}

// end halo::manager implementation


/**
 * halo::halo_record implementation
 */

halo_record::halo_record() :
	id_(NO_HALO), //halo::NO_HALO
	my_manager_()
{}

halo_record::halo_record(int id, const std::shared_ptr<halo_impl> & my_manager) :
	id_(id),
	my_manager_(my_manager)
{}

halo_record::~halo_record()
{
	if (!valid()) return;

	std::shared_ptr<halo_impl> man = my_manager_.lock();

	if(man) {
		man->remove(id_);
	}
}

} //end namespace halo
