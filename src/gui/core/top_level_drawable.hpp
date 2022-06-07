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

struct SDL_Rect;

namespace gui2
{

/**
 * A top-level drawable item (TLD), such as a window.
 *
 * For now, TLDs keep track of where they are on the screen on their own.
 * They must draw themselves when requested via expose().
 */
class top_level_drawable
{
protected:
	top_level_drawable();
	virtual ~top_level_drawable();
public:
	/**
	 * Draw the portion of the drawable intersecting @p region to the screen.
	 *
	 * @param region    The region to expose, in absolute draw-space
	 *                  coordinates.
	 * @returns         True if anything was drawn, false otherwise.
	 */
	virtual bool expose(const SDL_Rect& region) = 0;
};

} // namespace gui2
