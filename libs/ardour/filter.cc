/*
    Copyright (C) 2004-2007 Paul Davis

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

#include <time.h>
#include <cerrno>

#include "pbd/basename.h"

#include "ardour/analyser.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/filter.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

int
Filter::make_new_sources (boost::shared_ptr<Region> region, SourceList& nsrcs, string suffix)
{
	vector<string> names = region->master_source_names();
	assert (region->n_channels() <= names.size());

	for (uint32_t i = 0; i < region->n_channels(); ++i) {

		string name = PBD::basename_nosuffix (names[i]);

		/* remove any existing version of suffix by assuming it starts
		   with some kind of "special" character.
		*/

		if (!suffix.empty()) {
			string::size_type pos = name.find (suffix[0]);
			if (pos != string::npos && pos > 2) {
				name = name.substr (0, pos - 1);
			}
		}

		string path = session.new_audio_source_path (name, region->n_channels(), i, false, false);

		if (path.empty()) {
			error << string_compose (_("filter: error creating name for new file based on %1"), region->name())
			      << endmsg;
			return -1;
		}

		try {
			nsrcs.push_back (boost::dynamic_pointer_cast<Source> (
				SourceFactory::createWritable (region->data_type(), session,
							       path, false, session.frame_rate())));
		}

		catch (failed_constructor& err) {
			error << string_compose (_("filter: error creating new file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
Filter::finish (boost::shared_ptr<Region> region, SourceList& nsrcs, string region_name)
{
	/* update headers on new sources */

	time_t xnow;
	struct tm* now;

	time (&xnow);
	now = localtime (&xnow);

	/* this is ugly. */
	for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*si);
		if (afs) {
			afs->done_with_peakfile_writes ();
			afs->update_header (region->position(), *now, xnow);
			afs->mark_immutable ();
		}

		boost::shared_ptr<SMFSource> smfs = boost::dynamic_pointer_cast<SMFSource>(*si);
		if (smfs) {
			smfs->set_timeline_position (region->position());
			smfs->flush ();
		}

		/* now that there is data there, requeue the file for analysis */

		Analyser::queue_source_for_analysis (*si, false);
	}

	/* create a new region */

	if (region_name.empty()) {
		region_name = RegionFactory::new_region_name (region->name());
	}
	results.clear ();

	PropertyList plist;

	plist.add (Properties::start, 0);
	plist.add (Properties::length, region->length());
	plist.add (Properties::name, region_name);
	plist.add (Properties::whole_file, true);
	plist.add (Properties::position, region->position());

	boost::shared_ptr<Region> r = RegionFactory::create (nsrcs, plist);

	boost::shared_ptr<AudioRegion> audio_region = boost::dynamic_pointer_cast<AudioRegion> (region);
	boost::shared_ptr<AudioRegion> audio_r = boost::dynamic_pointer_cast<AudioRegion> (r);
	if (audio_region && audio_r) {
		audio_r->set_scale_amplitude (audio_region->scale_amplitude());
		audio_r->set_fade_in_active (audio_region->fade_in_active ());
		audio_r->set_fade_in (audio_region->fade_in ());
		audio_r->set_fade_out_active (audio_region->fade_out_active ());
		audio_r->set_fade_out (audio_region->fade_out ());
		*(audio_r->envelope()) = *(audio_region->envelope ());
	}
	results.push_back (r);

	return 0;
}


