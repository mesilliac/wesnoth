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

#include "gui/core/top_level_drawable.hpp"

#include "gui/core/draw_manager.hpp"

namespace gui2
{

top_level_drawable::top_level_drawable()
{
	draw_manager::register_drawable(this);
}

top_level_drawable::~top_level_drawable()
{
	draw_manager::unregister_drawable(this);
}

} // namespace gui2
