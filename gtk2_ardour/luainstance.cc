/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "gtkmm2ext/gui_thread.h"

#include "ardour/audioengine.h"
#include "ardour/diskstream.h"
#include "ardour/plugin_manager.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "region_selection.h"
#include "luainstance.h"
#include "luasignal.h"
#include "script_selector.h"

#include "i18n.h"

namespace LuaSignal {

#define STATIC(name,c,p) else if (!strcmp(type, #name)) {return name;}
#define SESSION(name,c,p) else if (!strcmp(type, #name)) {return name;}
#define ENGINE(name,c,p) else if (!strcmp(type, #name)) {return name;}

LuaSignal
str2luasignal (const std::string &str) {
	const char* type = str.c_str();
	if (0) { }
#	include "luasignal_syms.h"
	else {
		PBD::fatal << string_compose (_("programming error: %1: %2"), "Impossible LuaSignal type", str) << endmsg;
		abort(); /*NOTREACHED*/
	}
}
#undef STATIC
#undef SESSION
#undef ENGINE

#define STATIC(name,c,p) N_(#name),
#define SESSION(name,c,p) N_(#name),
#define ENGINE(name,c,p) N_(#name),
const char *luasignalstr[] = {
#	include "luasignal_syms.h"
	0
};

#undef STATIC
#undef SESSION
#undef ENGINE
}; // namespace

////////////////////////////////////////////////////////////////////////////////

#define xstr(s) stringify(s)
#define stringify(s) #s

using namespace ARDOUR;

void
LuaInstance::register_hooks (lua_State* L)
{

#define ENGINE(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
#define STATIC(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
#define SESSION(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("LuaSignal")
#		include "luasignal_syms.h"
		.endNamespace ();
#undef ENGINE
#undef SESSION
#undef STATIC

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("LuaSignal")
		.beginStdBitSet <LuaSignal::LAST_SIGNAL> ("Set")
		.endClass()
		.endNamespace ();
}

void
LuaInstance::register_classes (lua_State* L)
{
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::session (L);

	register_hooks (L);

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <RegionSelection> ("RegionSelection")
		.addFunction ("clear_all", &RegionSelection::clear_all)
		.addFunction ("start", &RegionSelection::start)
		.addFunction ("end_frame", &RegionSelection::end_frame)
		.addFunction ("n_midi_regions", &RegionSelection::n_midi_regions)
		.endClass ()

		.beginClass <PublicEditor> ("Editor")
		.addFunction ("undo", &PublicEditor::undo)
		.addFunction ("redo", &PublicEditor::redo)
		.addFunction ("play_selection", &PublicEditor::play_selection)
		.addFunction ("play_with_preroll", &PublicEditor::play_with_preroll)
		.addFunction ("maybe_locate_with_edit_preroll", &PublicEditor::maybe_locate_with_edit_preroll)
		.addFunction ("add_location_from_playhead_cursor", &PublicEditor::add_location_from_playhead_cursor)
		.addFunction ("goto_nth_marker", &PublicEditor::goto_nth_marker)
		.addFunction ("set_show_measures", &PublicEditor::set_show_measures)
		.addFunction ("mouse_add_new_marker", &PublicEditor::mouse_add_new_marker)
		.addFunction ("split_regions_at", &PublicEditor::split_regions_at)
		.addFunction ("maximise_editing_space", &PublicEditor::maximise_editing_space)
		.addFunction ("restore_editing_space", &PublicEditor::restore_editing_space)
		.addFunction ("get_regions_from_selection_and_mouse", &PublicEditor::get_regions_from_selection_and_mouse)
		.addFunction ("show_measures", &PublicEditor::show_measures)
		.addFunction ("set_zoom_focus", &PublicEditor::set_zoom_focus)
		.addFunction ("do_import", &PublicEditor::do_import)
		.addFunction ("do_embed", &PublicEditor::do_embed)
		.endClass ()
		.endNamespace ();

#undef ZOOMFOCUS
#undef SNAPTYPE
#undef SNAPMODE
#undef MOUSEMODE
#undef DISPLAYCONTROL
#undef IMPORTMODE
#undef IMPORTPOSITION
#undef IMPORTDISPOSITION

#define ZOOMFOCUS(NAME) .addConst (stringify(NAME), (Editing::ZoomFocus)Editing::NAME)
#define SNAPTYPE(NAME) .addConst (stringify(NAME), (Editing::SnapType)Editing::NAME)
#define SNAPMODE(NAME) .addConst (stringify(NAME), (Editing::SnapMode)Editing::NAME)
#define MOUSEMODE(NAME) .addConst (stringify(NAME), (Editing::MouseMode)Editing::NAME)
#define DISPLAYCONTROL(NAME) .addConst (stringify(NAME), (Editing::DisplayControl)Editing::NAME)
#define IMPORTMODE(NAME) .addConst (stringify(NAME), (Editing::ImportMode)Editing::NAME)
#define IMPORTPOSITION(NAME) .addConst (stringify(NAME), (Editing::ImportPosition)Editing::NAME)
#define IMPORTDISPOSITION(NAME) .addConst (stringify(NAME), (Editing::ImportDisposition)Editing::NAME)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Editing")
#		include "editing_syms.h"
		.endNamespace ();
}

#undef xstr
#undef stringify

////////////////////////////////////////////////////////////////////////////////

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace std;

#ifndef NDEBUG
static void _lua_print (std::string s) {
	std::cout << "LuaInstance: " << s << "\n";
}
#endif

LuaInstance* LuaInstance::_instance = 0;

LuaInstance*
LuaInstance::instance ()
{
	if (!_instance) {
		_instance  = new LuaInstance;
	}

	return _instance;
}

LuaInstance::LuaInstance ()
{
#ifndef NDEBUG
	lua.Print.connect (&_lua_print);
#endif
	init ();

	LuaScriptParamList args;
}

LuaInstance::~LuaInstance ()
{
	delete _lua_call_action;
	delete _lua_add_action;
	delete _lua_del_action;
	delete _lua_get_action;

	delete _lua_load;
	delete _lua_save;
	delete _lua_clear;
	_callbacks.clear();
}

void
LuaInstance::init ()
{
	lua.do_command (
			"function ScriptManager ()"
			"  local self = { scripts = {}, instances = {} }"
			""
			"  local remove = function (id)"
			"   self.scripts[id] = nil"
			"   self.instances[id] = nil"
			"  end"
			""
			"  local addinternal = function (i, n, s, f, a)"
			"   assert(type(i) == 'number', 'id must be numeric')"
			"   assert(type(n) == 'string', 'Name must be string')"
			"   assert(type(s) == 'string', 'Script must be string')"
			"   assert(type(f) == 'function', 'Factory is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   self.scripts[i] = { ['n'] = n, ['s'] = s, ['f'] = f, ['a'] = a }"
			"   local env = _ENV;  env.f = nil env.debug = nil os.exit = nil require = nil dofile = nil loadfile = nil package = nil"
			"   self.instances[i] = load (string.dump(f, true), nil, nil, env)(a)"
			"  end"
			""
			"  local call = function (id)"
			"   if type(self.instances[id]) == 'function' then"
			"     local status, err = pcall (self.instances[id])"
			"     if not status then"
			"       print ('action \"'.. id .. '\": ', err)" // error out
			"       remove (id)"
			"     end"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			"  local add = function (i, n, s, b, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   load (b)()" // assigns f
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (i, n, s, load(f), a)"
			"  end"
			""
			"  local get = function (id)"
			"   if type(self.scripts[id]) == 'table' then"
			"    return { ['name'] = self.scripts[id]['n'],"
			"             ['script'] = self.scripts[id]['s'],"
			"             ['args'] = self.scripts[id]['a'] }"
			"   end"
			"   return nil"
			"  end"
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   collectgarbage()"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"     collectgarbage()" // string concatenation allocates a new string
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			""
			"  local save = function ()"
			"   return (serialize('scripts', self.scripts))"
			"  end"
			""
			"  local clear = function ()"
			"   self.scripts = {}"
			"   self.instances = {}"
			"   collectgarbage()"
			"  end"
			""
			"  local restore = function (state)"
			"   clear()"
			"   load (state)()"
			"   for i, s in pairs (scripts) do"
			"    addinternal (i, s['n'], s['s'], load(s['f']), s['a'])"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			" return { call = call, add = add, remove = remove, get = get,"
			"          restore = restore, save = save, clear = clear}"
			" end"
			" "
			" manager = ScriptManager ()"
			" ScriptManager = nil"
			);

	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_mgr = luabridge::getGlobal (L, "manager");
		lua.do_command ("manager = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_add_action = new luabridge::LuaRef(lua_mgr["add"]);
		_lua_del_action = new luabridge::LuaRef(lua_mgr["remove"]);
		_lua_get_action = new luabridge::LuaRef(lua_mgr["get"]);
		_lua_call_action = new luabridge::LuaRef(lua_mgr["call"]);
		_lua_save = new luabridge::LuaRef(lua_mgr["save"]);
		_lua_load = new luabridge::LuaRef(lua_mgr["restore"]);
		_lua_clear = new luabridge::LuaRef(lua_mgr["clear"]);

	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup Lua action interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	register_classes (L);

	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
}

void LuaInstance::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!_session) {
		return;
	}

	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);

	for (LuaCallbackMap::iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		i->second->set_session (s);
	}
}

void
LuaInstance::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaInstance::session_going_away);
	(*_lua_clear)();
	for (int i = 0; i < 9; ++i) {
		ActionChanged (i, ""); /* EMIT SIGNAL */
	}
	SessionHandlePtr::session_going_away ();
	_session = 0;

	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);
	lua.do_command ("collectgarbage();");
}

int
LuaInstance::set_state (const XMLNode& node)
{
	LocaleGuard lg (X_("C"));
	XMLNode* child;

	if ((child = find_named_node (node, "ActionScript"))) {
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			if (!(*n)->is_content ()) { continue; }
			gsize size;
			guchar* buf = g_base64_decode ((*n)->content ().c_str (), &size);
			try {
				(*_lua_load)(std::string ((const char*)buf, size));
			} catch (luabridge::LuaException const& e) {
				cerr << "LuaException:" << e.what () << endl;
			}
			for (int i = 0; i < 9; ++i) {
				std::string name;
				if (lua_action_name (i, name)) {
					ActionChanged (i, name); /* EMIT SIGNAL */
				}
			}
			g_free (buf);
		}
	}

	if ((child = find_named_node (node, "ActionHooks"))) {
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			try {
				LuaCallbackPtr p (new LuaCallback (_session, *(*n)));
				_callbacks.insert (std::make_pair(p->id(), p));
				p->drop_callback.connect (_slotcon, MISSING_INVALIDATOR, boost::bind (&LuaInstance::unregister_lua_slot, this, p->id()), gui_context());
				SlotChanged (p->id(), p->name(), p->signals()); /* EMIT SIGNAL */
			} catch (luabridge::LuaException const& e) {
				cerr << "LuaException:" << e.what () << endl;
			}
		}
	}

	return 0;
}

bool
LuaInstance::interactive_add (LuaScriptInfo::ScriptType type, int id)
{
	std::string title;
	std::vector<std::string> reg;

	switch (type) {
		case LuaScriptInfo::EditorAction:
			reg = lua_action_names ();
			title = "Add Lua Action";
			break;
		case LuaScriptInfo::EditorHook:
			reg = lua_slot_names ();
			title = "Add Lua Callback Hook";
			break;
		default:
			return false;
	}

	LuaScriptInfoPtr spi;
	ScriptSelector ss (title, type);
	switch (ss.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			spi = ss.script();
			break;
		default:
			return false;
	}

	std::string script = "";

	try {
		script = Glib::file_get_contents (spi->path);
	} catch (Glib::FileError e) {
		string msg = string_compose (_("Cannot read script '%1': %2"), spi->path, e.what());
		Gtk::MessageDialog am (msg);
		am.run ();
		return false;
	}

	LuaScriptParamList lsp = LuaScripting::script_params (spi, "action_params");

	ScriptParameterDialog spd (_("Set Script Parameters"), spi, reg, lsp);
	switch (spd.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			break;
		default:
			return false;
	}

	switch (type) {
		case LuaScriptInfo::EditorAction:
			return set_lua_action (id, spd.name(), script, lsp);
			break;
		case LuaScriptInfo::EditorHook:
			return register_lua_slot (spd.name(), script, lsp);
			break;
		default:
			break;
	}
	return false;
}

XMLNode&
LuaInstance::get_action_state ()
{
	LocaleGuard lg (X_("C"));
	std::string saved;
	{
		luabridge::LuaRef savedstate ((*_lua_save)());
		saved = savedstate.cast<std::string>();
	}
	lua.collect_garbage ();

	gchar* b64 = g_base64_encode ((const guchar*)saved.c_str (), saved.size ());
	std::string b64s (b64);
	g_free (b64);

	XMLNode* script_node = new XMLNode (X_("ActionScript"));
	script_node->add_property (X_("lua"), LUA_VERSION);
	script_node->add_content (b64s);

	return *script_node;
}

XMLNode&
LuaInstance::get_hook_state ()
{
	XMLNode* script_node = new XMLNode (X_("ActionHooks"));
	for (LuaCallbackMap::const_iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		script_node->add_child_nocopy (i->second->get_state ());
	}
	return *script_node;
}

void
LuaInstance::call_action (const int id)
{
	try {
		(*_lua_call_action)(id + 1);
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
	}
}

bool
LuaInstance::set_lua_action (
		const int id,
		const std::string& name,
		const std::string& script,
		const LuaScriptParamList& args)
{
	try {
		lua_State* L = lua.getState();
		// get bytcode of factory-function in a sandbox
		// (don't allow scripts to interfere)
		const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
		luabridge::LuaRef tbl_arg (luabridge::newTable(L));
		for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
			if ((*i)->optional && !(*i)->is_set) { continue; }
			tbl_arg[(*i)->name] = (*i)->value;
		}
		(*_lua_add_action)(id + 1, name, script, bytecode, tbl_arg);
		ActionChanged (id, name); /* EMIT SIGNAL */
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		return false;
	}
	return true;
}

bool
LuaInstance::remove_lua_action (const int id)
{
	try {
		(*_lua_del_action)(id + 1);
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		return false;
	}
	ActionChanged (id, ""); /* EMIT SIGNAL */
	return true;
}

bool
LuaInstance::lua_action_name (const int id, std::string& rv)
{
	try {
		luabridge::LuaRef ref ((*_lua_get_action)(id + 1));
		if (ref.isNil()) {
			return false;
		}
		if (ref["name"].isString()) {
			rv = ref["name"].cast<std::string>();
			return true;
		}
		return true;
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		return false;
	}
	return false;
}

std::vector<std::string>
LuaInstance::lua_action_names ()
{
	std::vector<std::string> rv;
	for (int i = 0; i < 9; ++i) {
		std::string name;
		if (lua_action_name (i, name)) {
			rv.push_back (name);
		}
	}
	return rv;
}

bool
LuaInstance::lua_action (const int id, std::string& name, std::string& script, LuaScriptParamList& args)
{
	try {
		luabridge::LuaRef ref ((*_lua_get_action)(id + 1));
		if (ref.isNil()) {
			return false;
		}
		if (!ref["name"].isString()) {
			return false;
		}
		if (!ref["script"].isString()) {
			return false;
		}
		if (!ref["args"].isTable()) {
			return false;
		}
		name = ref["name"].cast<std::string>();
		script = ref["script"].cast<std::string>();

		args.clear();
		LuaScriptInfoPtr lsi = LuaScripting::script_info (script);
		if (!lsi) {
			return false;
		}
		args = LuaScripting::script_params (lsi, "action_params");
		for (luabridge::Iterator i (static_cast<luabridge::LuaRef>(ref["args"])); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			std::string name = i.key ().cast<std::string> ();
			std::string value = i.value ().cast<std::string> ();
			for (LuaScriptParamList::const_iterator ii = args.begin(); ii != args.end(); ++ii) {
				if ((*ii)->name == name) {
					(*ii)->value = value;
					break;
				}
			}
		}
		return true;
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		return false;
	}
	return false;
}

bool
LuaInstance::register_lua_slot (const std::string& name, const std::string& script, const ARDOUR::LuaScriptParamList& args)
{
	/* parse script, get ActionHook(s) from script */
	ActionHook ah;
	try {
		LuaState l;
#ifndef NDEBUG
		l.Print.connect (&_lua_print);
#endif
		lua_State* L = l.getState();
		register_hooks (L);
		l.do_command ("function ardour () end");
		l.do_command (script);
		luabridge::LuaRef signals = luabridge::getGlobal (L, "signals");
		if (signals.isFunction()) {
			ah = signals();
		}
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
	}

	if (ah.none ()) {
		cerr << "Script registered no hooks." << endl;
		return false;
	}

	/* register script w/args, get entry-point / ID */

	try {
		LuaCallbackPtr p (new LuaCallback (_session, name, script, ah, args));
		_callbacks.insert (std::make_pair(p->id(), p));
		p->drop_callback.connect (_slotcon, MISSING_INVALIDATOR, boost::bind (&LuaInstance::unregister_lua_slot, this, p->id()), gui_context());
		SlotChanged (p->id(), p->name(), p->signals()); /* EMIT SIGNAL */
		return true;
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
	}
	return false;
}

bool
LuaInstance::unregister_lua_slot (const PBD::ID& id)
{
	LuaCallbackMap::const_iterator i = _callbacks.find (id);
	if (i != _callbacks.end()) {
		SlotChanged (id, "", ActionHook()); /* EMIT SIGNAL */
		_callbacks.erase (i);
		return true;
	}
	return false;
}

std::vector<PBD::ID>
LuaInstance::lua_slots () const
{
	std::vector<PBD::ID> rv;
	for (LuaCallbackMap::const_iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		rv.push_back (i->first);
	}
	return rv;
}

bool
LuaInstance::lua_slot_name (const PBD::ID& id, std::string& name) const
{
	LuaCallbackMap::const_iterator i = _callbacks.find (id);
	if (i != _callbacks.end()) {
		name = i->second->name();
		return true;
	}
	return false;
}

std::vector<std::string>
LuaInstance::lua_slot_names () const
{
	std::vector<std::string> rv;
	std::vector<PBD::ID> ids = lua_slots();
	for (std::vector<PBD::ID>::const_iterator i = ids.begin(); i != ids.end(); ++i) {
		std::string name;
		if (lua_slot_name (*i, name)) {
			rv.push_back (name);
		}
	}
	return rv;
}

bool
LuaInstance::lua_slot (const PBD::ID& id, std::string& name, std::string& script, ActionHook& ah, ARDOUR::LuaScriptParamList& args)
{
	LuaCallbackMap::const_iterator i = _callbacks.find (id);
	if (i == _callbacks.end()) {
		return false; // error
	}
	return i->second->lua_slot (name, script, ah, args);
}

///////////////////////////////////////////////////////////////////////////////

LuaCallback::LuaCallback (Session *s,
		const std::string& name,
		const std::string& script,
		const ActionHook& ah,
		const ARDOUR::LuaScriptParamList& args)
	: SessionHandlePtr (s)
	, _id ("0")
	, _name (name)
	, _signals (ah)
{
	// TODO: allow to reference object (e.g region)
	init ();

	lua_State* L = lua.getState();
	luabridge::LuaRef tbl_arg (luabridge::newTable(L));
	for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
		if ((*i)->optional && !(*i)->is_set) { continue; }
		tbl_arg[(*i)->name] = (*i)->value;
	}

	try {
	const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
	(*_lua_add)(name, script, bytecode, tbl_arg);
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		throw failed_constructor ();
	}

	_id.reset ();
	set_session (s);
}

LuaCallback::LuaCallback (Session *s, XMLNode & node)
	: SessionHandlePtr (s)
{
	XMLNode* child = NULL;
	if (node.name() != X_("LuaCallback")
			|| !node.property ("signals")
			|| !node.property ("id")
			|| !node.property ("name")) {
		throw failed_constructor ();
	}

	for (XMLNodeList::const_iterator n = node.children ().begin (); n != node.children ().end (); ++n) {
		if (!(*n)->is_content ()) { continue; }
		child = *n;
	}

	if (!child) {
		throw failed_constructor ();
	}

	init ();

	_id = PBD::ID (node.property ("id")->value ());
	_name = node.property ("name")->value ();
	_signals = ActionHook (node.property ("signals")->value ());

	gsize size;
	guchar* buf = g_base64_decode (child->content ().c_str (), &size);
	try {
		(*_lua_load)(std::string ((const char*)buf, size));
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
	}
	g_free (buf);

	set_session (s);
}

LuaCallback::~LuaCallback ()
{
	delete _lua_add;
	delete _lua_get;
	delete _lua_call;
	delete _lua_load;
	delete _lua_save;
}

XMLNode&
LuaCallback::get_state (void)
{
	std::string saved;
	{
		luabridge::LuaRef savedstate ((*_lua_save)());
		saved = savedstate.cast<std::string>();
	}
	lua.collect_garbage ();

	gchar* b64 = g_base64_encode ((const guchar*)saved.c_str (), saved.size ());
	std::string b64s (b64);
	g_free (b64);

	XMLNode* script_node = new XMLNode (X_("LuaCallback"));
	script_node->add_property (X_("lua"), LUA_VERSION);
	script_node->add_property (X_("id"), _id.to_s ());
	script_node->add_property (X_("name"), _name);
	script_node->add_property (X_("signals"), _signals.to_string ());
	script_node->add_content (b64s);
	return *script_node;
}

void
LuaCallback::init (void)
{
#ifndef NDEBUG
	lua.Print.connect (&_lua_print);
#endif

	lua.do_command (
			"function ScriptManager ()"
			"  local self = { script = {}, instance = {} }"
			""
			"  local addinternal = function (n, s, f, a)"
			"   assert(type(n) == 'string', 'Name must be string')"
			"   assert(type(s) == 'string', 'Script must be string')"
			"   assert(type(f) == 'function', 'Factory is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   self.script = { ['n'] = n, ['s'] = s, ['f'] = f, ['a'] = a }"
			"   local env = _ENV;  env.f = nil env.debug = nil os.exit = nil require = nil dofile = nil loadfile = nil package = nil"
			"   self.instance = load (string.dump(f, true), nil, nil, env)(a)"
			"  end"
			""
			"  local call = function (...)"
			"   if type(self.instance) == 'function' then"
			"     local status, err = pcall (self.instance, ...)"
			"     if not status then"
			"       print ('callback \"'.. self.script['n'] .. '\": ', err)" // error out
			"       self.script = nil"
			"       self.instance = nil"
			"       return false"
			"     end"
			"   end"
			"   collectgarbage()"
			"   return true"
			"  end"
			""
			"  local add = function (n, s, b, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   load (b)()" // assigns f
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (n, s, load(f), a)"
			"  end"
			""
			"  local get = function ()"
			"   if type(self.instance) == 'function' and type(self.script['n']) == 'string' then"
			"    return { ['name'] = self.script['n'],"
			"             ['script'] = self.script['s'],"
			"             ['args'] = self.script['a'] }"
			"   end"
			"   return nil"
			"  end"
			""
			// code dup
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   collectgarbage()"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"     collectgarbage()" // string concatenation allocates a new string
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			// end code dup
			""
			"  local save = function ()"
			"   return (serialize('s', self.script))"
			"  end"
			""
			"  local restore = function (state)"
			"   self.script = {}"
			"   load (state)()"
			"   addinternal (s['n'], s['s'], load(s['f']), s['a'])"
			"  end"
			""
			" return { call = call, add = add, get = get,"
			"          restore = restore, save = save}"
			" end"
			" "
			" manager = ScriptManager ()"
			" ScriptManager = nil"
			);

	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_mgr = luabridge::getGlobal (L, "manager");
		lua.do_command ("manager = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_add = new luabridge::LuaRef(lua_mgr["add"]);
		_lua_get = new luabridge::LuaRef(lua_mgr["get"]);
		_lua_call = new luabridge::LuaRef(lua_mgr["call"]);
		_lua_save = new luabridge::LuaRef(lua_mgr["save"]);
		_lua_load = new luabridge::LuaRef(lua_mgr["restore"]);

	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup Lua callback interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	LuaInstance::register_classes (L);

	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
}

bool
LuaCallback::lua_slot (std::string& name, std::string& script, ActionHook& ah, ARDOUR::LuaScriptParamList& args)
{
	// TODO consolidate w/ LuaInstance::lua_action()
	try {
		luabridge::LuaRef ref = (*_lua_get)();
		if (ref.isNil()) {
			return false;
		}
		if (!ref["name"].isString()) {
			return false;
		}
		if (!ref["script"].isString()) {
			return false;
		}
		if (!ref["args"].isTable()) {
			return false;
		}

		ah = _signals;
		name = ref["name"].cast<std::string> ();
		script = ref["script"].cast<std::string> ();

		args.clear();
		LuaScriptInfoPtr lsi = LuaScripting::script_info (script);
		if (!lsi) {
			return false;
		}
		args = LuaScripting::script_params (lsi, "action_params");
		for (luabridge::Iterator i (static_cast<luabridge::LuaRef>(ref["args"])); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			std::string name = i.key ().cast<std::string> ();
			std::string value = i.value ().cast<std::string> ();
			for (LuaScriptParamList::const_iterator ii = args.begin(); ii != args.end(); ++ii) {
				if ((*ii)->name == name) {
					(*ii)->value = value;
					break;
				}
			}
		}
		return true;
	} catch (luabridge::LuaException const& e) {
		cerr << "LuaException:" << e.what () << endl;
		return false;
	}
	return false;
}

void
LuaCallback::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		lua_State* L = lua.getState();
		LuaBindings::set_session (L, _session);
	}

	reconnect();
}

void
LuaCallback::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaCallback::session_going_away);
	lua.do_command ("collectgarbage();");

	SessionHandlePtr::session_going_away ();
	_session = 0;

	drop_callback (); /* EMIT SIGNAL */
}

void
LuaCallback::reconnect ()
{
	_connections.drop_connections ();
	if ((*_lua_get) ().isNil ()) {
		drop_callback (); /* EMIT SIGNAL */
		return;
	}

	// TODO pass object which emits the signal (e.g region)
	//
	// save/load bound objects will be tricky.
	// Best idea so far is to save/lookup the PBD::ID
	// (either use boost::any indirection or templates for bindable
	// object types or a switch statement..)
	//
	// _session->route_by_id ()
	// _session->track_by_diskstream_id ()
	// _session->source_by_id ()
	// _session->controllable_by_id ()
	// _session->processor_by_id ()
	// RegionFactory::region_by_id ()
	//
	// TODO loop over objects (if any)

	reconnect_object ((void*)0);
}

template <class T> void
LuaCallback::reconnect_object (T obj)
{
	for (uint32_t i = 0; i < LuaSignal::LAST_SIGNAL; ++i) {
		if (_signals[i]) {
#define ENGINE(n,c,p) else if (i == LuaSignal::n) { connect_ ## p (LuaSignal::n, AudioEngine::instance(), &(AudioEngine::instance()->c)); }
#define SESSION(n,c,p) else if (i == LuaSignal::n) { if (_session) { connect_ ## p (LuaSignal::n, _session, &(_session->c)); } }
#define STATIC(n,c,p) else if (i == LuaSignal::n) { connect_ ## p (LuaSignal::n, obj, c); }
			if (0) {}
#			include "luasignal_syms.h"
			else {
				PBD::fatal << string_compose (_("programming error: %1: %2"), "Impossible LuaSignal type", i) << endmsg;
				abort(); /*NOTREACHED*/
			}
#undef ENGINE
#undef SESSION
#undef STATIC
		}
	}
}

template <typename T, typename S> void
LuaCallback::connect_0 (enum LuaSignal::LuaSignal ls, T ref, S *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_0<T>, this, ls, ref),
			gui_context());
}

template <typename T, typename C1> void
LuaCallback::connect_1 (enum LuaSignal::LuaSignal ls, T ref, PBD::Signal1<void, C1> *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_1<T, C1>, this, ls, ref, _1),
			gui_context());
}

template <typename T, typename C1, typename C2> void
LuaCallback::connect_2 (enum LuaSignal::LuaSignal ls, T ref, PBD::Signal2<void, C1, C2> *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_2<T, C1, C2>, this, ls, ref, _1, _2),
			gui_context());
}

template <typename T> void
LuaCallback::proxy_0 (enum LuaSignal::LuaSignal ls, T ref) {
	if (!(*_lua_call)((int)ls, ref)) { drop_callback (); /* EMIT SIGNAL */}
}

template <typename T, typename C1> void
LuaCallback::proxy_1 (enum LuaSignal::LuaSignal ls, T ref, C1 a1) {
	if (!(*_lua_call)((int)ls, ref, a1)) { drop_callback (); /* EMIT SIGNAL */}
}

template <typename T, typename C1, typename C2> void
LuaCallback::proxy_2 (enum LuaSignal::LuaSignal ls, T ref, C1 a1, C2 a2) {
	if (!(*_lua_call)((int)ls, ref, a1, a2)) { drop_callback (); /* EMIT SIGNAL */}
}
