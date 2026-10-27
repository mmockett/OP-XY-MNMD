// mdref: renders single Machinedrum hits or Monomachine notes from the owner's
// own ROM, through the gearmulator-md-mm emulator, for comparison with this
// repository's machines. The ROM says which instrument it is.
//
//   mdref <rom.bin> <out-dir> < jobs.txt
//
// Each job line is: <name> <machine-id> <seconds> <p1> .. <p8> [options]
// with the eight synthesis parameters 0..127, then any of:
//   ccN=V      set controller N to V on the track's channel (other pages)
//   note=N     the note (Monomachine; default 60)
//   gate=S     seconds before the note-off (Monomachine; default 60% of seconds)
//   table=1    assign from the UW's ROM/RAM machine table (Machinedrum)
// Lines starting with # are skipped; a line "lcd <name>" writes the LCD as
// text to <out-dir>/<name>.txt.
// Machinedrum hits play on track 1 with neutral track settings (no AM, flat
// EQ, open filter, no SRR or distortion, no sends, no LFO), triggered from the
// panel. Monomachine notes play on track 1 with the machine's initial pages.
// Each is written as a mono 44.1 kHz float WAV, <out-dir>/<name>.wav.
//
// This file links the GPL-3 emulator and is GPL-3 like it. Nothing from the
// emulator or the ROM is used by the OP-XY machines.
#include "mdLib/mddevice.h"
#include "mdLib/mdromloader.h"
#include "mdLib/mdpanel.h"
#include "mdLib/mdtypes.h"

#include "baseLib/filesystem.h"
#include "synthLib/midiTypes.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr size_t g_block = 128;

	struct Rig
	{
		std::unique_ptr<md::Device> device;
		std::array<std::vector<float>, 6> out;
		synthLib::TAudioInputs inputs{};
		synthLib::TAudioOutputs outputs{};
		std::vector<float> silence = std::vector<float>(g_block, 0.0f);
		std::vector<synthLib::SMidiEvent> midi, discarded;

		md::MachineModel model;

		explicit Rig(std::vector<uint8_t> _rom, const std::string& _name, const md::MachineModel _model) : model(_model)
		{
			synthLib::DeviceCreateParams params;
			params.romData = std::move(_rom);
			params.romName = _name;
			params.customData = md::deviceCustomData(_model);
			device = std::make_unique<md::Device>(params);
			for(size_t i = 0; i < out.size(); ++i)
			{
				out[i].assign(g_block, 0.0f);
				outputs[i] = out[i].data();
			}
			inputs = {silence.data(), silence.data(), nullptr, nullptr};
		}

		// Runs `frames`, appending the main mix (left + right) / 2 to `record` if given.
		void run(const size_t _frames, std::vector<float>* _record = nullptr)
		{
			for(size_t done = 0; done < _frames; done += g_block)
			{
				const auto n = std::min(g_block, _frames - done);
				discarded.clear();
				device->process(inputs, outputs, n, midi, discarded);
				midi.clear();
				if(_record)
					for(size_t i = 0; i < n; ++i)
						_record->push_back(0.5f * (out[0][i] + out[1][i]));
			}
		}

		void sysex(std::initializer_list<uint8_t> _bytes)
		{
			synthLib::SMidiEvent event(synthLib::MidiEventSource::Host);
			event.sysex.assign(_bytes.begin(), _bytes.end());
			midi.push_back(event);
			run(8192);
		}

		void cc(const uint8_t _controller, const uint8_t _value)
		{
			midi.push_back({synthLib::MidiEventSource::Host, synthLib::M_CONTROLCHANGE, _controller, _value});
			run(g_block);
		}

		void note(const uint8_t _note, const bool _on)
		{
			midi.push_back({synthLib::MidiEventSource::Host,
				static_cast<uint8_t>(_on ? synthLib::M_NOTEON : synthLib::M_NOTEOFF), _note, static_cast<uint8_t>(_on ? 100 : 0)});
		}

		void panel(const md::PanelControl _control, const bool _down)
		{
			const auto packet = md::panelPacket(md::MachineModel::Machinedrum, _control);
			if(packet)
				device->sendPanelEvent(packet->row, _down ? packet->mask : 0);
		}

		void lcd(std::ostream& _out) const
		{
			const auto panel = device->getFrontPanelSnapshot();
			for(uint32_t y = 0; y < md::FrontPanel::g_lcdHeight; ++y)
			{
				for(uint32_t x = 0; x < md::FrontPanel::g_lcdWidth; ++x)
					_out << (panel.getLcdPixel(x, y) ? '#' : '.');
				_out << '\n';
			}
		}
	};

	void writeWav(const std::string& _path, const std::vector<float>& _audio)
	{
		std::ofstream f(_path, std::ios::binary);
		const auto put32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
		const auto put16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
		const auto bytes = static_cast<uint32_t>(_audio.size() * 4);
		f.write("RIFF", 4); put32(36 + bytes); f.write("WAVE", 4);
		f.write("fmt ", 4); put32(16); put16(3); put16(1); put32(md::g_samplerate);
		put32(md::g_samplerate * 4); put16(4); put16(32);
		f.write("data", 4); put32(bytes);
		f.write(reinterpret_cast<const char*>(_audio.data()), bytes);
	}
}

int main(const int argc, const char* const* argv)
{
	if(argc != 3)
	{
		std::cerr << "usage: mdref <rom.bin> <out-dir> < jobs.txt\n";
		return 2;
	}
	std::vector<uint8_t> rom;
	if(!baseLib::filesystem::readFile(rom, argv[1]))
	{
		std::cerr << "mdref: cannot read " << argv[1] << '\n';
		return 1;
	}
	const std::string dir = argv[2];
	const bool mono = md::RomLoader::isRomForModel(rom, md::MachineModel::Monomachine);
	if(!mono && !md::RomLoader::isRomForModel(rom, md::MachineModel::Machinedrum))
	{
		std::cerr << "mdref: not a supported Machinedrum (OS 1.63) or Monomachine (OS 1.32b) image\n";
		return 1;
	}
	Rig rig(std::move(rom), argv[1], mono ? md::MachineModel::Monomachine : md::MachineModel::Machinedrum);
	if(!rig.device->isValid())
	{
		std::cerr << "mdref: the ROM did not make a valid instrument\n";
		return 1;
	}
	// Boot, including the UW's first-run flash initialisation.
	rig.run(md::g_samplerate * 20);
	std::cerr << "mdref: booted\n";

	std::string line;
	while(std::getline(std::cin, line))
	{
		if(line.empty() || line[0] == '#')
			continue;
		std::istringstream in(line);
		std::string name;
		in >> name;
		if(name == "lcd")
		{
			std::string file;
			in >> file;
			std::ofstream out(dir + "/" + file + ".txt");
			rig.lcd(out);
			continue;
		}
		unsigned machine = 0;
		double seconds = 0;
		std::array<unsigned, 8> p{};
		in >> machine >> seconds;
		for(auto& v : p)
			in >> v;
		if(!in)
		{
			std::cerr << "mdref: bad job line: " << line << '\n';
			return 2;
		}
		std::vector<std::pair<uint8_t, uint8_t>> extra;
		unsigned key = 60;
		double gate = seconds * 0.6;
		unsigned table = 0;
		std::string option;
		while(in >> option)
		{
			const auto eq = option.find('=');
			const auto field = option.substr(0, eq);
			const auto value = eq == std::string::npos ? std::string() : option.substr(eq + 1);
			if(field.rfind("cc", 0) == 0)
				extra.emplace_back(static_cast<uint8_t>(std::stoi(field.substr(2)) & 0x7f), static_cast<uint8_t>(std::stoi(value) & 0x7f));
			else if(field == "note")
				key = static_cast<unsigned>(std::stoi(value));
			else if(field == "gate")
				gate = std::stod(value);
			else if(field == "table")
				table = static_cast<unsigned>(std::stoi(value));
			else
			{
				std::cerr << "mdref: unknown option " << option << '\n';
				return 2;
			}
		}

		std::vector<float> audio;
		if(!mono)
		{
			// OS 1.63 Appendix C: assign machine <id> to track 1 from the standard
			// table (table=1 picks the UW's ROM/RAM table), then set every parameter
			// on track 1 (CC 16..39, channel 1).
			rig.sysex({0xf0, 0x00, 0x20, 0x3c, 0x02, 0x00, 0x5b, 0x00, static_cast<uint8_t>(machine),
				static_cast<uint8_t>(table ? 1 : 0), 0xf7});
			rig.run(md::g_samplerate / 2);   // the firmware rebuilds the track before it takes controllers
			for(uint8_t i = 0; i < 8; ++i)
				rig.cc(16 + i, static_cast<uint8_t>(p[i] & 0x7f));
			// AMD AMF EQF EQG FLTF FLTW FLTQ SRR, then DIST VOL PAN DEL REV LFOS LFOD LFOM.
			constexpr std::array<uint8_t, 16> neutral{0, 0, 64, 64, 0, 127, 0, 0, 0, 100, 64, 0, 0, 0, 0, 0};
			for(uint8_t i = 0; i < neutral.size(); ++i)
				rig.cc(24 + i, neutral[i]);
			rig.cc(8, 100);    // track level
			rig.cc(12, 0);     // unmuted
			for(const auto& [controller, value] : extra)
				rig.cc(controller, value);
			rig.run(md::g_samplerate / 4);

			rig.panel(md::PanelControl::Trigger1, true);
			rig.run(g_block * 8, &audio);
			rig.panel(md::PanelControl::Trigger1, false);
			rig.run(static_cast<size_t>(seconds * md::g_samplerate), &audio);
		}
		else
		{
			// SFX-6/60 Appendix C: assign machine <id> to track 1 and initialise all
			// its pages, then the synthesis page on CC 48..55, channel 1.
			rig.sysex({0xf0, 0x00, 0x20, 0x3c, 0x03, 0x00, 0x5b, 0x00, static_cast<uint8_t>(machine), 0x01, 0xf7});
			rig.run(md::g_samplerate);
			for(uint8_t i = 0; i < 8; ++i)
				rig.cc(48 + i, static_cast<uint8_t>(p[i] & 0x7f));
			rig.cc(7, 100);    // track level
			for(const auto& [controller, value] : extra)
				rig.cc(controller, value);
			rig.run(md::g_samplerate / 4);

			rig.note(static_cast<uint8_t>(key), true);
			const auto held = static_cast<size_t>(gate * md::g_samplerate);
			rig.run(held, &audio);
			rig.note(static_cast<uint8_t>(key), false);
			const auto total = static_cast<size_t>(seconds * md::g_samplerate);
			rig.run(total > held ? total - held : 0, &audio);
		}
		writeWav(dir + "/" + name + ".wav", audio);
		std::cerr << name << '\n';
		rig.run(md::g_samplerate);   // let any tail finish before the next job
	}
	return 0;
}
