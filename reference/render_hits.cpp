// Renders single hits of one of this repository's voice modules on the host, for
// comparison with mdref's renders of the originals: the module's own C++, at
// middle C or the job's note, with no envelope or gate after it.
//
//   c++ -std=c++17 -I../open-xy/sdk/include -Ilib -Ivoices -DHEADER='"trx_bd.hpp"' -DVOICE=trx_bd \
//       reference/render_hits.cpp -o local/hits-trx_bd
//   local/hits-trx_bd <out-dir> < jobs.txt
//
// Job lines are as for mdref without the machine ID: <name> <seconds> <p1> .. <p8>,
// the machine's eight slots, then optionally the MIDI note (middle C, 60, if not).
#include HEADER

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	void writeWav(const std::string& path, const std::vector<float>& audio)
	{
		std::ofstream f(path, std::ios::binary);
		const auto put32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
		const auto put16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
		const auto bytes = static_cast<uint32_t>(audio.size() * 4);
		f.write("RIFF", 4); put32(36 + bytes); f.write("WAVE", 4);
		f.write("fmt ", 4); put32(16); put16(3); put16(1); put32(44100); put32(44100 * 4); put16(4); put16(32);
		f.write("data", 4); put32(bytes);
		f.write(reinterpret_cast<const char*>(audio.data()), bytes);
	}
}

int main(int argc, char** argv)
{
	if(argc != 2) { std::cerr << "usage: hits <out-dir> < jobs.txt\n"; return 2; }
	static md::VOICE::State voice;
	static md::Common shared;
	opxy::u32 pitch[opxy::block_frames];
	std::string line;
	while(std::getline(std::cin, line))
	{
		if(line.empty() || line[0] == '#') continue;
		std::istringstream in(line);
		std::string name;
		double seconds = 0;
		double v[8] = {};
		in >> name >> seconds;
		for(auto& x : v) in >> x;
		if(!in) { std::cerr << "bad job line: " << line << '\n'; return 2; }
		double note = 60, given = 0;
		if(in >> given) note = given;
		for(auto& p : pitch)   // middle C is 25479004, 261.63 Hz
			p = note == 60 ? 25479004u : static_cast<opxy::u32>(std::llround(440.0 * std::pow(2.0, (note - 69) / 12) / 44100 * 4294967296.0));
		opxy::i32 parameters[8];
		for(int i = 0; i < 8; ++i) parameters[i] = static_cast<opxy::i32>(v[i] * 16909320.0);   // 0..127 -> Q31
		shared = md::Common();
		md::VOICE::reset(voice, shared, 1);
		md::VOICE::trigger(voice, shared);
		std::vector<float> audio;
		opxy::i32 out[opxy::block_frames];
		const auto blocks = static_cast<unsigned>(seconds * 44100 / opxy::block_frames) + 1;
		for(unsigned b = 0; b < blocks; ++b)
		{
			md::VOICE::render(voice, shared, out, parameters, pitch);
			for(auto s : out) audio.push_back(static_cast<float>(s / 2147483648.0));
		}
		writeWav(std::string(argv[1]) + "/" + name + ".wav", audio);
	}
	return 0;
}
