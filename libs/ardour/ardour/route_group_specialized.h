#ifndef __ardour_route_group_specialized_h__
#define __ardour_route_group_specialized_h__

#include <ardour/route_group.h>
#include <ardour/audio_track.h>

namespace ARDOUR {

template<class T> void 
RouteGroup::apply (void (Track::*func)(T, void *), T val, void *src) 
{
	for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		Track *at;
		if ((at = dynamic_cast<Track*>(*i)) != 0) {
			(at->*func)(val, this);
		}
	}
}
 
} /* namespace ARDOUR */

#endif /* __ardour_route_group_specialized_h__ */
