/*
    Copyright (C) 2002-2009 Paul Davis 

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

*/

#include <iostream>
#include "ardour/bundle.h"
#include "ardour/types.h"
#include "port_matrix_body.h"
#include "port_matrix.h"

PortMatrixBody::PortMatrixBody (PortMatrix* p)
	: _matrix (p),
	  _column_labels (p, this),
	  _row_labels (p, this),
	  _grid (p, this),
	  _xoffset (0),
	  _yoffset (0),
	  _pointer_inside (false)
{
	modify_bg (Gtk::STATE_NORMAL, Gdk::Color ("#00000"));
	add_events (Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::POINTER_MOTION_MASK);
}


bool
PortMatrixBody::on_expose_event (GdkEventExpose* event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	bool intersects;
	Gdk::Rectangle r = exposure;
	r.intersect (_column_labels.parent_rectangle(), intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_column_labels.get_pixmap (get_window()->gobj()),
			_column_labels.parent_to_component_x (r.get_x()),
			_column_labels.parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (_row_labels.parent_rectangle(), intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_row_labels.get_pixmap (get_window()->gobj()),
			_row_labels.parent_to_component_x (r.get_x()),
			_row_labels.parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	r = exposure;
	r.intersect (_grid.parent_rectangle(), intersects);

	if (intersects) {
		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_grid.get_pixmap (get_window()->gobj()),
			_grid.parent_to_component_x (r.get_x()),
			_grid.parent_to_component_y (r.get_y()),
			r.get_x(),
			r.get_y(),
			r.get_width(),
			r.get_height()
			);
	}

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());
	_grid.draw_extra (cr);
	_row_labels.draw_extra (cr);
	_column_labels.draw_extra (cr);
	cairo_destroy (cr);

	return true;
}

void
PortMatrixBody::on_size_request (Gtk::Requisition *req)
{
	std::pair<int, int> const col = _column_labels.dimensions ();
	std::pair<int, int> const row = _row_labels.dimensions ();
	std::pair<int, int> const grid = _grid.dimensions ();

	/* don't ask for the maximum size of our contents, otherwise GTK won't
	   let the containing window shrink below this size */

	req->width = std::min (512, std::max (col.first, grid.first + row.first));
	req->height = std::min (512, col.second + grid.second);
}

void
PortMatrixBody::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_alloc_width = alloc.get_width ();
	_alloc_height = alloc.get_height ();

	compute_rectangles ();
	_matrix->setup_scrollbars ();
}

void
PortMatrixBody::compute_rectangles ()
{
	/* full sizes of components */
	std::pair<uint32_t, uint32_t> const col = _column_labels.dimensions ();
	std::pair<uint32_t, uint32_t> const row = _row_labels.dimensions ();
	std::pair<uint32_t, uint32_t> const grid = _grid.dimensions ();

	Gdk::Rectangle col_rect;
	Gdk::Rectangle row_rect;
	Gdk::Rectangle grid_rect;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		/* build from top left */

		col_rect.set_x (0);
		col_rect.set_y (0);
		grid_rect.set_x (0);

		if (_alloc_width > col.first) {
			col_rect.set_width (col.first);
		} else {
			col_rect.set_width (_alloc_width);
		}

		/* move down to y division */
		
		uint32_t y = 0;
		if (_alloc_height > col.second) {
			y = col.second;
		} else {
			y = _alloc_height;
		}

		col_rect.set_height (y);
		row_rect.set_y (y);
		row_rect.set_height (_alloc_height - y);
		grid_rect.set_y (y);
		grid_rect.set_height (_alloc_height - y);

		/* move right to x division */

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		grid_rect.set_width (x);
		row_rect.set_x (x);
		row_rect.set_width (_alloc_width - x);
			

	} else if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

		/* build from bottom right */

		/* move left to x division */

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		grid_rect.set_x (_alloc_width - x);
		grid_rect.set_width (x);
		col_rect.set_width (col.first - grid.first + x);
		col_rect.set_x (_alloc_width - col_rect.get_width());

		row_rect.set_width (std::min (_alloc_width - x, row.first));
		row_rect.set_x (_alloc_width - x - row_rect.get_width());

		/* move up to the y division */
		
		uint32_t y = 0;
		if (_alloc_height > col.second) {
			y = col.second;
		} else {
			y = _alloc_height;
		}

		col_rect.set_y (_alloc_height - y);
		col_rect.set_height (y);

		grid_rect.set_height (std::min (grid.second, _alloc_height - y));
		grid_rect.set_y (_alloc_height - y - grid_rect.get_height());

		row_rect.set_height (grid_rect.get_height());
		row_rect.set_y (grid_rect.get_y());
	}

	_row_labels.set_parent_rectangle (row_rect);
	_column_labels.set_parent_rectangle (col_rect);
	_grid.set_parent_rectangle (grid_rect);
}

void
PortMatrixBody::setup ()
{
	/* Discard any old connections to bundles */
	
	for (std::list<sigc::connection>::iterator i = _bundle_connections.begin(); i != _bundle_connections.end(); ++i) {
		i->disconnect ();
	}
	_bundle_connections.clear ();

	/* Connect to bundles so that we find out when their names change */
	
	ARDOUR::BundleList r = _matrix->rows()->bundles ();
	for (ARDOUR::BundleList::iterator i = r.begin(); i != r.end(); ++i) {
		
		_bundle_connections.push_back (
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_row_labels))
			);
		
	}

	ARDOUR::BundleList c = _matrix->columns()->bundles ();
	for (ARDOUR::BundleList::iterator i = c.begin(); i != c.end(); ++i) {
		_bundle_connections.push_back (
			(*i)->NameChanged.connect (sigc::mem_fun (*this, &PortMatrixBody::rebuild_and_draw_column_labels))
			);
	}
	
	_column_labels.setup ();
	_row_labels.setup ();
	_grid.setup ();

	set_mouseover (
		PortMatrixNode (
			ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0),
			ARDOUR::BundleChannel (boost::shared_ptr<ARDOUR::Bundle> (), 0)
			)
		);

	compute_rectangles ();
}

uint32_t
PortMatrixBody::full_scroll_width ()
{
	return _grid.dimensions().first;

}

uint32_t
PortMatrixBody::alloc_scroll_width ()
{
	return _grid.parent_rectangle().get_width();
}

uint32_t
PortMatrixBody::full_scroll_height ()
{
	return _grid.dimensions().second;
}

uint32_t
PortMatrixBody::alloc_scroll_height ()
{
	return _grid.parent_rectangle().get_height();
}

void
PortMatrixBody::set_xoffset (uint32_t xo)
{
	_xoffset = xo;
	queue_draw ();
}

void
PortMatrixBody::set_yoffset (uint32_t yo)
{
	_yoffset = yo;
	queue_draw ();
}

bool
PortMatrixBody::on_button_press_event (GdkEventButton* ev)
{
	if (Gdk::Region (_grid.parent_rectangle()).point_in (ev->x, ev->y)) {

		_grid.button_press (
			_grid.parent_to_component_x (ev->x),
			_grid.parent_to_component_y (ev->y),
			ev->button
			);

	} else if (Gdk::Region (_row_labels.parent_rectangle()).point_in (ev->x, ev->y)) {

		_row_labels.button_press (
			_row_labels.parent_to_component_x (ev->x),
			_row_labels.parent_to_component_y (ev->y),
			ev->button, ev->time
			);
	
	} else if (Gdk::Region (_column_labels.parent_rectangle()).point_in (ev->x, ev->y)) {

		_column_labels.button_press (
			_column_labels.parent_to_component_x (ev->x),
			_column_labels.parent_to_component_y (ev->y),
			ev->button, ev->time
			);
	}

	return true;
}

void
PortMatrixBody::rebuild_and_draw_grid ()
{
	_grid.require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_column_labels ()
{
	_column_labels.require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_row_labels ()
{
	_row_labels.require_rebuild ();
	queue_draw ();
}

bool
PortMatrixBody::on_enter_notify_event (GdkEventCrossing* ev)
{
	if (ev->type == GDK_ENTER_NOTIFY) {
		_pointer_inside = true;
	} else if (ev->type == GDK_LEAVE_NOTIFY) {
		_pointer_inside = false;
	}

	return true;
}

bool
PortMatrixBody::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_pointer_inside && Gdk::Region (_grid.parent_rectangle()).point_in (ev->x, ev->y)) {
		_grid.mouseover_event (
			_grid.parent_to_component_x (ev->x),
			_grid.parent_to_component_y (ev->y)
			);
	}

	return true;
}

void
PortMatrixBody::set_mouseover (PortMatrixNode const & n)
{
	if (n == _mouseover) {
		return;
	}

	PortMatrixNode old = _mouseover;
	_mouseover = n;
	
	_grid.mouseover_changed (old);
	_row_labels.mouseover_changed (old);
	_column_labels.mouseover_changed (old);
}
