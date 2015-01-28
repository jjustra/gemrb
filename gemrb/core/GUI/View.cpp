/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2015 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "View.h"

#include "GUI/ScrollBar.h"
#include "Interface.h"
#include "Video.h"

namespace GemRB {

View::View(const Region& frame)
	: frame(frame)
{

	dirty = true;
}

View::~View()
{
	std::list<View*>::iterator it;
	for (it = subViews.begin(); it != subViews.end(); ++it) {
		delete *it;
	}
}

void View::Draw()
{

}

void View::SetFrame(const Region& r)
{
	frame = r;
}

void View::SetFrameOrigin(const Point& p)
{
	frame.x = p.x;
	frame.y = p.y;
}

void View::SetFrameSize(const Size& s)
{
	frame.w = s.w;
	frame.h = s.h;
}

}