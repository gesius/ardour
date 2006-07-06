/*
    Copyright (C) 2004 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef __ardour_gtk_selectable_h__
#define __ardour_gtk_selectable_h__

#include <sigc++/signal.h>

class Selectable : public virtual sigc::trackable
{
  public:
	Selectable() {
		_selected = false;
	}

	virtual ~Selectable() {}

	virtual void set_selected (bool yn) {
		if (yn != _selected) {
			_selected = yn;
			Selected (_selected); /* EMIT_SIGNAL */
		}
	}

	bool get_selected() const {
		return _selected;
	}

	/** Emitted when the selected status of this Selectable changes */
	sigc::signal<void, bool> Selected ;

  protected:
	bool _selected;
};

#endif /* __ardour_gtk_selectable_h__ */
