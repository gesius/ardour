ardour {
	["type"]    = "dsp",
	name        = "Simple Synth",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[An Example Synth for prototyping.]]
}

function dsp_ioconfig ()
	return
	{
		{ audio_in = 0, audio_out = 1},
	}
end

function dsp_midi_input ()
	return true
end


local note_table = {}
local active_notes = {}
local phases = {}
local env = .01;

function dsp_init (rate)
	for n = 1,128 do
		note_table [n] = (440 / 32) * 2^((n - 10.0) / 12.0) / rate
	end
	env = 100 / rate
end

function dsp_run (ins, outs, n_samples)
	-- initialize output buffer
	assert (#outs == 1)
	local a = {}
	for s = 1, n_samples do a[s] = 0 end


	-- very basic synth, simple sine, basic envelope
	local function synth (s_start, s_end)
		for n,v in pairs (active_notes) do
			local vel = v["vel"] or 0
			local tgt = v["tvel"];
			for s = s_start,s_end do
				local phase = phases[n] or 0
				vel = vel + env * (tgt - vel)
				a[s] = a[s] + math.sin(6.283185307 * phase) * vel / 167
				phase = phase + note_table[n]
				if (phase > 1.0) then
					phases[n] = phase - 2.0
				else
					phases[n] = phase
				end
			end
			if vel < 1 and tgt == 0 then
				active_notes[n] = nil
			else
				active_notes[n]["vel"] = vel;
			end
		end
	end

	local tme = 1
	-- parse midi messages
	assert (type(mididata) == "table") -- global table of midi events (for now)
	for _,b in pairs (mididata) do 
		local t = b["time"] -- t = [ 1 .. n_samples ]

		-- synth sound until event
		synth(tme, t)
		tme = t + 1

		local d = b["data"] -- get midi-event
		-- we ignore the midi channel
		if (#d == 3 and bit32.band (d[1], 240) == 144) then -- note on
			local n = 1 + d[2];
			active_notes[n] = active_notes[n] or {}
			active_notes[n]["tvel"] = d[3]
		end
		if (#d == 3 and bit32.band (d[1], 240) == 128) then -- note off
			local n = 1 + d[2];
			active_notes[n] = active_notes[n] or {}
			active_notes[n]["tvel"] = 0
		end
		if (#d == 3 and bit32.band (d[1], 240) == 176) then -- CC
			if (d[2] == 120 or d[2] == 123) then -- panic
				active_notes = {}
			end
		end
	end

	-- synth rest of cycle
	synth(tme, n_samples)

	-- copy
	outs[1]:set_table(a, n_samples)
end
