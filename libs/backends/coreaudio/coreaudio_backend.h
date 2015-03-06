/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_coreaudio_backend_h__
#define __libbackend_coreaudio_backend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include "ardour/audio_backend.h"
#include "ardour/types.h"

#include "coreaudio_pcmio.h"
#include "coremidi_io.h"

namespace ARDOUR {

class CoreAudioBackend;

class CoreMidiEvent {
	public:
		CoreMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		CoreMidiEvent (const CoreMidiEvent& other);
		~CoreMidiEvent ();
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const unsigned char* const_data () const { return _data; };
		unsigned char* data () { return _data; };
		bool operator< (const CoreMidiEvent &other) const { return timestamp () < other.timestamp (); };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t *_data;
};

typedef std::vector<boost::shared_ptr<CoreMidiEvent> > CoreMidiBuffer;

class CoreBackendPort {
	protected:
		CoreBackendPort (CoreAudioBackend &b, const std::string&, PortFlags);
	public:
		virtual ~CoreBackendPort ();

		const std::string& name () const { return _name; }
		PortFlags flags () const { return _flags; }

		int set_name (const std::string &name) { _name = name; return 0; }

		virtual DataType type () const = 0;

		bool is_input ()     const { return flags () & IsInput; }
		bool is_output ()    const { return flags () & IsOutput; }
		bool is_physical ()  const { return flags () & IsPhysical; }
		bool is_terminal ()  const { return flags () & IsTerminal; }
		bool is_connected () const { return _connections.size () != 0; }
		bool is_connected (const CoreBackendPort *port) const;
		bool is_physically_connected () const;

		const std::vector<CoreBackendPort *>& get_connections () const { return _connections; }

		int connect (CoreBackendPort *port);
		int disconnect (CoreBackendPort *port);
		void disconnect_all ();

		virtual void* get_buffer (pframes_t nframes) = 0;

		const LatencyRange& latency_range (bool for_playback) const
		{
			return for_playback ? _playback_latency_range : _capture_latency_range;
		}

		void set_latency_range (const LatencyRange &latency_range, bool for_playback)
		{
			if (for_playback)
			{
				_playback_latency_range = latency_range;
			}
			else
			{
				_capture_latency_range = latency_range;
			}
		}

	private:
		CoreAudioBackend &_osx_backend;
		std::string _name;
		const PortFlags _flags;
		LatencyRange _capture_latency_range;
		LatencyRange _playback_latency_range;
		std::vector<CoreBackendPort*> _connections;

		void _connect (CoreBackendPort* , bool);
		void _disconnect (CoreBackendPort* , bool);

}; // class CoreBackendPort

class CoreAudioPort : public CoreBackendPort {
	public:
		CoreAudioPort (CoreAudioBackend &b, const std::string&, PortFlags);
		~CoreAudioPort ();

		DataType type () const { return DataType::AUDIO; };

		Sample* buffer () { return _buffer; }
		const Sample* const_buffer () const { return _buffer; }
		void* get_buffer (pframes_t nframes);

	private:
		Sample _buffer[8192];
}; // class CoreAudioPort

class CoreMidiPort : public CoreBackendPort {
	public:
		CoreMidiPort (CoreAudioBackend &b, const std::string&, PortFlags);
		~CoreMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const CoreMidiBuffer * const_buffer () const { return & _buffer[_bufperiod]; }

		void next_period() { if (_n_periods > 1) { get_buffer(0); _bufperiod = (_bufperiod + 1) % _n_periods; } }
		void set_n_periods(int n) { if (n > 0 && n < 3) { _n_periods = n; } }

	private:
		CoreMidiBuffer _buffer[2];
		int _n_periods;
		int _bufperiod;
}; // class CoreMidiPort

class CoreAudioBackend : public AudioBackend {
	friend class CoreBackendPort;
	public:
		CoreAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~CoreAudioBackend ();

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;

		int set_device_name (const std::string&);
		int set_sample_rate (float);
		int set_buffer_size (uint32_t);
		int set_interleaved (bool yn);
		int set_input_channels (uint32_t);
		int set_output_channels (uint32_t);
		int set_systemic_input_latency (uint32_t);
		int set_systemic_output_latency (uint32_t);
		int set_systemic_midi_input_latency (std::string const, uint32_t);
		int set_systemic_midi_output_latency (std::string const, uint32_t);

		int reset_device () { return 0; };

		/* Retrieving parameters */
		std::string  device_name () const;
		float        sample_rate () const;
		uint32_t     buffer_size () const;
		bool         interleaved () const;
		uint32_t     input_channels () const;
		uint32_t     output_channels () const;
		uint32_t     systemic_input_latency () const;
		uint32_t     systemic_output_latency () const;
		uint32_t     systemic_midi_input_latency (std::string const) const;
		uint32_t     systemic_midi_output_latency (std::string const) const;

		bool can_set_systemic_midi_latencies () const { return false; /* XXX */}

		/* External control app */
		std::string control_app_name () const { return std::string ("Apple"); }
		void launch_control_app ();

		/* MIDI */
		std::vector<std::string> enumerate_midi_options () const;
		int set_midi_option (const std::string&);
		std::string midi_option () const;

		std::vector<DeviceStatus> enumerate_midi_devices () const;
		int set_midi_device_enabled (std::string const, bool);
		bool midi_device_enabled (std::string const) const;

		// really private, but needing static access:
		int process_callback();
		void error_callback();
		void xrun_callback();
		void hw_changed_callback();

	protected:
		/* State Control */
		int _start (bool for_latency_measurement);
	public:
		int stop ();
		int freewheel (bool);
		float dsp_load () const;
		size_t raw_buffer_size (DataType t);

		/* Process time */
		framepos_t sample_time ();
		framepos_t sample_time_at_cycle_start ();
		pframes_t samples_since_cycle_start ();

		int create_process_thread (boost::function<void()> func);
		int join_process_threads ();
		bool in_process_thread ();
		uint32_t process_thread_count ();

		void update_latencies ();

		/* PORTENGINE API */

		void* private_handle () const;
		const std::string& my_name () const;
		bool available () const;
		uint32_t port_name_size () const;

		int         set_port_name (PortHandle, const std::string&);
		std::string get_port_name (PortHandle) const;
		PortHandle  get_port_by_name (const std::string&) const;

		int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

		DataType port_data_type (PortHandle) const;

		PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		void unregister_port (PortHandle);

		int  connect (const std::string& src, const std::string& dst);
		int  disconnect (const std::string& src, const std::string& dst);
		int  connect (PortHandle, const std::string&);
		int  disconnect (PortHandle, const std::string&);
		int  disconnect_all (PortHandle);

		bool connected (PortHandle, bool process_callback_safe);
		bool connected_to (PortHandle, const std::string&, bool process_callback_safe);
		bool physically_connected (PortHandle, bool process_callback_safe);
		int  get_connections (PortHandle, std::vector<std::string>&, bool process_callback_safe);

		/* MIDI */
		int midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
		int midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
		uint32_t get_midi_event_count (void* port_buffer);
		void     midi_clear (void* port_buffer);

		/* Monitoring */

		bool can_monitor_input () const;
		int  request_input_monitoring (PortHandle, bool);
		int  ensure_input_monitoring (PortHandle, bool);
		bool monitoring_input (PortHandle);

		/* Latency management */

		void         set_latency_range (PortHandle, bool for_playback, LatencyRange);
		LatencyRange get_latency_range (PortHandle, bool for_playback);

		/* Discovering physical ports */

		bool      port_is_physical (PortHandle) const;
		void      get_physical_outputs (DataType type, std::vector<std::string>&);
		void      get_physical_inputs (DataType type, std::vector<std::string>&);
		ChanCount n_physical_outputs () const;
		ChanCount n_physical_inputs () const;

		/* Getting access to the data buffer for a port */

		void* get_buffer (PortHandle, pframes_t);

		void* freewheel_thread ();
		void post_process ();
		void coremidi_rediscover ();

	private:
		std::string _instance_name;
		CoreAudioPCM *_pcmio;
		CoreMidiIo *_midiio;

		bool  _run; /* keep going or stop, ardour thread */
		bool  _active_ca; /* is running, process thread */
		bool  _active_fw; /* is running, process thread */
		bool  _preinit;
		bool  _freewheeling;
		bool  _freewheel;
		bool  _freewheel_ack;
		bool  _reinit_thread_callback;
		bool  _measure_latency;
		pthread_mutex_t _process_callback_mutex;

		static std::vector<std::string> _midi_options;
		static std::vector<AudioBackend::DeviceStatus> _audio_device_status;
		static std::vector<AudioBackend::DeviceStatus> _midi_device_status;

		mutable std::string _audio_device;
		std::string _midi_driver_option;

		/* audio settings */
		float  _samplerate;
		size_t _samples_per_period;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _systemic_audio_input_latency;
		uint32_t _systemic_audio_output_latency;

		/* coreaudio specific  */
		uint32_t name_to_id(std::string) const;

		/* midi settings */
		struct CoreMidiDeviceInfo {
			bool     enabled;
			uint32_t systemic_input_latency;
			uint32_t systemic_output_latency;
			CoreMidiDeviceInfo()
				: enabled (true)
				, systemic_input_latency (0)
				, systemic_output_latency (0)
			{}
		};

		mutable std::map<std::string, struct CoreMidiDeviceInfo *> _midi_devices;
		struct CoreMidiDeviceInfo * midi_device_info(std::string const) const;

		/* processing */
		float  _dsp_load;
		uint64_t _processed_samples;

		pthread_t _main_thread;
		pthread_t _freeewheel_thread;

		/* process threads */
		static void* coreaudio_process_thread (void *);
		std::vector<pthread_t> _threads;

		struct ThreadData {
			CoreAudioBackend* engine;
			boost::function<void ()> f;
			size_t stacksize;

			ThreadData (CoreAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
				: engine (e) , f (fp) , stacksize (stacksz) {}
		};

		/* port engine */
		PortHandle add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		int register_system_audio_ports ();
		int register_system_midi_ports ();
		void unregister_ports (bool system_only = false);

		std::vector<CoreBackendPort *> _ports;
		std::vector<CoreBackendPort *> _system_inputs;
		std::vector<CoreBackendPort *> _system_outputs;
		std::vector<CoreBackendPort *> _system_midi_in;
		std::vector<CoreBackendPort *> _system_midi_out;

		struct PortConnectData {
			std::string a;
			std::string b;
			bool c;

			PortConnectData (const std::string& a, const std::string& b, bool c)
				: a (a) , b (b) , c (c) {}
		};

		std::vector<PortConnectData *> _port_connection_queue;
		pthread_mutex_t _port_callback_mutex;
		bool _port_change_flag;

		void port_connect_callback (const std::string& a, const std::string& b, bool conn) {
			pthread_mutex_lock (&_port_callback_mutex);
			_port_connection_queue.push_back(new PortConnectData(a, b, conn));
			pthread_mutex_unlock (&_port_callback_mutex);
		}

		void port_connect_add_remove_callback () {
			pthread_mutex_lock (&_port_callback_mutex);
			_port_change_flag = true;
			pthread_mutex_unlock (&_port_callback_mutex);
		}

		bool valid_port (PortHandle port) const {
			return std::find (_ports.begin (), _ports.end (), (CoreBackendPort*)port) != _ports.end ();
		}

		CoreBackendPort * find_port (const std::string& port_name) const {
			for (std::vector<CoreBackendPort*>::const_iterator it = _ports.begin (); it != _ports.end (); ++it) {
				if ((*it)->name () == port_name) {
					return *it;
				}
			}
			return NULL;
		}

}; // class CoreAudioBackend

} // namespace

#endif /* __libbackend_coreaudio_backend_h__ */
