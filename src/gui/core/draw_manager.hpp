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

#pragma once

#include "sdl/rect.hpp"

namespace gui2
{
class top_level_drawable;

namespace draw_manager
{

/**
 * Mark a region of the screen as requiring redraw.
 *
 * This should be called any time an item changes in such a way as to
 * require redrawing.
 */
void invalidate_region(const rect& region);

/**
 * Ensure that everything which needs to be drawn is drawn.
 *
 * This includes making sure window sizes and locations are up to date,
 * updating animation frames, and drawing whatever regions of the screen
 * need drawing or redrawing.
 *
 * If vsync is enabled, this function will block until the next vblank.
 * If nothing is drawn, it will still block for an appropriate amount of
 * time to simulate vsync.
 */
void sparkle();

/** Ensure layout is up-to-date for all TLDs. */
void layout();

/** Draw all invalidated regions. Returns false if nothing was drawn. */
bool draw();

/** Register a top-level drawable.
 *
 * Registered drawables will be drawn in the order of registration,
 * so the most recently-registered drawable will be "on top".
 */
void register_drawable(gui2::top_level_drawable* tld);

/** Remove a top-level drawable from the drawing stack. */
void unregister_drawable(gui2::top_level_drawable* tld);

/** Raise a TLD to the top of the drawing stack. */
void raise_drawable(gui2::top_level_drawable* tld);

/** Register an animation. This is a prototyping interface which will change. */
void register_static_animation(top_level_drawable* tld, const SDL_Rect& r);

} // namespace draw_manager
} // namespace gui2
