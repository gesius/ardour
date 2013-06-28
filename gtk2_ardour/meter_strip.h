/*
    Copyright (C) 2013 Paul Davis
    Author: Robin Gareus

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

#ifndef __ardour_meter_strip__
#define __ardour_meter_strip__

#include <vector>

#include <cmath>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"
#include "route_ui.h"

#include "level_meter.h"

namespace ARDOUR {
	class Route;
	class Session;
}
namespace Gtk {
	class Window;
	class Style;
}

class Meterbridge;

class MeterStrip : public Gtk::VBox, public RouteUI
{
  public:
	MeterStrip (Meterbridge&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~MeterStrip ();

	void fast_update ();
	void display_metrics (bool);
	boost::shared_ptr<ARDOUR::Route> route() { return _route; }

	static PBD::Signal1<void,MeterStrip*> CatchDeletion;

  protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;
	void self_delete ();

	gint meter_metrics_expose (GdkEventExpose *);
	gint meter_ticks_expose (GdkEventExpose *, Gtk::DrawingArea *);
	gint meter_ticks1_expose (GdkEventExpose *);
	gint meter_ticks2_expose (GdkEventExpose *);

	typedef std::map<std::string,cairo_pattern_t*> MetricPatterns;
	static  MetricPatterns metric_patterns;

	typedef std::map<std::string,cairo_pattern_t*> TickPatterns;
	static  TickPatterns ticks_patterns;

	static  cairo_pattern_t* render_metrics (Gtk::Widget &, std::vector<ARDOUR::DataType>);
	static  cairo_pattern_t* render_ticks (Gtk::Widget &, std::vector<ARDOUR::DataType>);

	void on_theme_changed ();
	bool style_changed;

	void on_size_allocate (Gtk::Allocation&);
	void on_size_request (Gtk::Requisition*);

	/* route UI */
	void update_rec_display ();
	std::string state_id() const;
	void set_button_names ();

  private:
	Gtk::HBox meterbox;
	Gtk::Label label;
	Gtk::DrawingArea meter_metric_area;
	Gtk::DrawingArea meter_ticks1_area;
	Gtk::DrawingArea meter_ticks2_area;
	std::vector<ARDOUR::DataType> _types;

	LevelMeter   *level_meter;
	void meter_changed ();

	PBD::ScopedConnection _config_connection;
	void strip_property_changed (const PBD::PropertyChange&);
	void meter_configuration_changed (ARDOUR::ChanCount);

};

#endif /* __ardour_mixer_strip__ */
