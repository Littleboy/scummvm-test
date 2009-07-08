/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */
#include "common/scummsys.h"
#include "common/endian.h"
#include "common/stream.h"
#include "common/util.h"
#include "common/debug.h"

#include "sound/mods/maxtrax.h"

namespace Audio {

MaxTrax::MaxTrax(int rate, bool stereo)
	: Paula(stereo, rate, rate/50), _playerCtx(), _voiceCtx(), _patch(), _channelCtx(), _scores(), _numScores(), _microtonal() {
	_playerCtx.maxScoreNum = 128;
	_playerCtx.vBlankFreq = 50;
	_playerCtx.frameUnit = (uint16)((1000 * (1<<8)) /  _playerCtx.vBlankFreq);
	_playerCtx.scoreIndex = -1;
	// glob_CurrentScore = _scoreptr;
	_playerCtx.volume = 0x64;

	_playerCtx.tempoTime = 0;

	uint32 uinqueId = 0;
	byte flags = 0;

	uint32 colorClock = kPalSystemClock / 2;

	// init extraChannel
	// extraChannel. chan_Number = 16, chan_Flags = chan_VoicesActive = 0
}

MaxTrax::~MaxTrax() {
	stopMusic();
	freePatches();
	freeScores();
}

void MaxTrax::interrupt() {
	// a5 - maxtraxm a4 . globaldata

	// test for changes in shared struct and make changes
	// specifically all used channels get marked altered

	_playerCtx.ticks += _playerCtx.tickUnit;
	const int32 millis = _playerCtx.ticks >> 8; // d4
	for (int i = 0; i < ARRAYSIZE(_voiceCtx); ++i) {
		VoiceContext &voice = _voiceCtx[i]; // a3
		if (!voice.channel || voice.stopEventCommand >= 0x80)
			continue;
		const int channelNo = voice.stopEventParameter;
		voice.stopEventTime -= (channelNo < kNumChannels) ? _playerCtx.tickUnit : _playerCtx.frameUnit;
		if (voice.stopEventTime <= 0) {
			noteOff(_channelCtx[channelNo], voice.stopEventCommand);
			voice.stopEventCommand = 0xFF;
		}
	}

	if (_playerCtx.musicPlaying) {
		Event *curEvent = _playerCtx.nextEvent;
		int32 eventTime = _playerCtx.nextEventTime;
		for (; eventTime <= millis; eventTime += (++curEvent)->startTime) {
			const byte cmd = curEvent->command;
			const byte data = curEvent->parameter;
			const uint16 stopTime = curEvent->stopTime;
			ChannelContext &channel = _channelCtx[data & 0x0F];

			outPutEvent(*curEvent);

			if (cmd < 0x80) {
				_playerCtx.addedNote = false;
				const uint16 vol = (data & 0xF0) >> 1;

				const int8 voiceIndex = noteOn(channel, cmd, vol, kPriorityScore);
				if (voiceIndex >= 0) {
					VoiceContext &voice = _voiceCtx[voiceIndex];
					voice.stopEventCommand = cmd;
					voice.stopEventParameter = data & 0x0F;
					voice.stopEventTime = (eventTime + stopTime - millis) << 8;
				}
			} else {
				switch (cmd) {
				case 0x80:	// TEMPO
					if ((_playerCtx.tickUnit >> 8) > stopTime) {
						setTempo(data << 4);
						_playerCtx.tempoTime = 0;
					} else {
						_playerCtx.tempoStart = _playerCtx.tempo;
						_playerCtx.tempoDelta = (data << 4) - _playerCtx.tempoStart;
						_playerCtx.tempoTime  = (stopTime << 8);
						_playerCtx.tempoTicks = 0;
					}
					break;
/*				case 0xA0:	// SPECIAL
					break;
				case 0xB0:	// CONTROL
					// TODO: controlChange((byte)stopTime, (byte)(stopTime >> 8))
					break;
*/				case 0xC0:	// PROGRAM
					channel.patch = &_patch[stopTime & (kNumPatches - 1)];
					break;
				case 0xE0:	// BEND
					channel.pitchBend = ((stopTime & 0x7F00) >> 1) | (stopTime & 0x7f);
					// channel.pitchReal = ((int32)(channel.pitchBendRange << 8) * (channel.pitchBend - (64 << 7))) / (64 << 7);
					channel.pitchReal = ((channel.pitchBendRange * channel.pitchBend) >> 5) - (channel.pitchBendRange << 8);
					channel.flags |= ChannelContext::kFlagAltered;
					break;
				case 0xFF:	// END
					if (_playerCtx.musicLoop) {
						// event -1 as it gets increased at the end of the loop
						curEvent = _scores[_playerCtx.scoreIndex].events - 1;
						_playerCtx.ticks = 0;
						eventTime = 0;
					} else
						_playerCtx.musicPlaying = false;
					break;
				default:
					debug("Unhandled Command");
					outPutEvent(*curEvent);
				}
			}
		}
		_playerCtx.nextEvent = curEvent;
		_playerCtx.nextEventTime = eventTime;

		// tempoEffect
		if (_playerCtx.tempoTime) {
			_playerCtx.tempoTicks += _playerCtx.tickUnit;
			uint16 newTempo;
			if (_playerCtx.tempoTicks < _playerCtx.tempoTime) {
				const uint16 delta = (_playerCtx.tempoTicks * _playerCtx.tempoDelta) / _playerCtx.tempoTime;
				newTempo = delta;
			} else {
				_playerCtx.tempoTime = 0;
				newTempo = _playerCtx.tempoDelta;
			}
			setTempo(_playerCtx.tempoStart + newTempo);
		}

		// envelopes

	}

}

int32 MaxTrax::omgItsAntiLog(uint32 val) {
	// some really weird exponential function, and some also very nonstandard "standard format" floats
	// format is 16? bit exponent, 16 bit mantissa. and we need to scale with log(2)
	const float v = ldexp((float)((val & 0xFFFF) + 0x10000) * (float)(0.69314718055994530942 / 65536), val >> 16);
	return (uint32)exp(v);
}

void MaxTrax::stopMusic() {
}

bool MaxTrax::doSong(int songIndex, int advance) {
	if (songIndex < 0 || songIndex >= _numScores)
		return false;
	Paula::pausePlay(true);
	_playerCtx.musicPlaying = false;
	_playerCtx.musicLoop = false;

	setTempo(_playerCtx.tempoInitial);
	_playerCtx.nextEvent = _scores[songIndex].events;
	_playerCtx.scoreIndex = songIndex;

	_playerCtx.musicPlaying = true;
	Paula::startPaula();
	return true;
}

void MaxTrax::killVoice(byte num) {
	VoiceContext &voice = _voiceCtx[num];
	--(voice.channel->voicesActive);
	voice.channel = 0;
	voice.status = VoiceContext::kStatusFree;
	voice.flags = 0;
	voice.priority = 0;
	voice.uinqueId = 0;

	// "stop" voice, set period to 1, vol to 0
	Paula::disableChannel(0);
	Paula::setChannelPeriod(num, 1);
	Paula::setChannelVolume(num, 0);
}

int MaxTrax::calcNote(VoiceContext &voice) {
	ChannelContext &channel = *voice.channel;
	voice.lastPeriod = 0;

	int16 bend = 0;
	if ((voice.flags & VoiceContext::kFlagPortamento) != 0) {
		// microtonal crap

		bend = (int16)((int8)(voice.endNote - voice.baseNote) * voice.portaTicks) / channel.portamento;
	}
	// modulation
	if (channel.modulation && (channel.flags & ChannelContext::kFlagModVolume) == 0) {
		// TODO get sine
		int32 sinevalue = 0;
		// TODO
	}

	int32 tone = bend + channel.pitchReal;
	// more it-never-worked microtonal code
	tone += voice.baseNote << 8;

	Patch &patch = *voice.patch;
	tone += ((int16)patch.tune << 8) / 24;

	tone -= 45 << 8; // MIDI note 45

	tone = (((tone * 4) / 3) << 4);
	enum { K_VALUE = 0x9fd77, PREF_PERIOD = 0x8fd77, PERIOD_LIMIT = 0x6f73d };

	tone = K_VALUE - tone;
	int octave = 0;

	if ((voice.flags & VoiceContext::kFlagRecalc) == 0) {
		voice.periodOffset = 0;
		const int maxOctave = patch.sampleOctaves - 1;
		while (tone > PREF_PERIOD && octave < maxOctave) {
			tone -= 1 << 4;
			voice.periodOffset += 1 <<4;
			octave++;
		}
		tone -= voice.periodOffset;
	}
	if (tone < PERIOD_LIMIT)
		voice.lastPeriod = (uint16)omgItsAntiLog((float)tone);

	return octave;
}

int8 MaxTrax::noteOn(ChannelContext &channel, const byte note, uint16 volume, uint16 pri) {
	if (channel.microtonal >= 0)
		_microtonal[note % 127] = channel.microtonal;
	if (!volume)
		return -1;

	Patch &patch = *channel.patch;
	if (!patch.samplePtr)
		return -1;
	int8 voiceNum = -1;
	if ((channel.flags & ChannelContext::kFlagMono) != 0  && channel.voicesActive) {
		for (voiceNum = ARRAYSIZE(_voiceCtx) - 1; voiceNum >= 0 && _voiceCtx[voiceNum].channel != &channel; --voiceNum)
			;
		if (voiceNum < 0)
			return -1;

		VoiceContext &voice = _voiceCtx[voiceNum];
		if (voice.status >= VoiceContext::kStatusSustain && (channel.flags & ChannelContext::kFlagPortamento) != 0) {
			// reset previous porta
			if ((voice.flags & VoiceContext::kFlagPortamento) != 0)
				voice.baseNote = voice.endNote;
			voice.portaTicks = 0;
			voice.flags |= VoiceContext::kFlagPortamento;
			voice.endNote = channel.lastNote = note;
			voice.noteVolume = (_playerCtx.handleVolume) ? volume + 1 : 128;
			_playerCtx.addedNote = true;
			_playerCtx.lastVoice = voiceNum;
			return voiceNum;
		}
	} else {
		// TODO:
		// pickvoice based on channel.isRightChannel
		// return if no channel found
		voiceNum = (channel.flags & ChannelContext::kFlagRightChannel) != 0 ? 0 : 1;
	}
	assert(voiceNum >= 0 && voiceNum < ARRAYSIZE(_voiceCtx));

	VoiceContext &voice = _voiceCtx[voiceNum];
	voice.flags = 0;
	if (voice.channel) {
		killVoice(voiceNum);
		voice.flags |= VoiceContext::kFlagStolen;
	}
	voice.channel = &channel;
	voice.patch = &patch;
	voice.baseNote = note;

	int useOctave = calcNote(voice);
	voice.priority = (byte)pri;
	voice.status = VoiceContext::kStatusStart;

	voice.noteVolume = (_playerCtx.handleVolume) ? volume + 1 : 128;

	// ifeq HAS_FULLCHANVOL macro
	if (channel.volume < 128)
		voice.noteVolume = (voice.noteVolume * channel.volume) >> 7;

	voice.baseVolume = 0;
	voice.lastTicks = 0;

	uint16 period = voice.lastPeriod;
	if (!period)
		period = 1000;

	// get samplestart for the given octave
	const int8 *samplePtr = patch.samplePtr + (patch.sampleTotalLen << useOctave) - patch.sampleTotalLen;
	if (patch.sampleAttackLen) {
		Paula::setChannelSampleStart(voiceNum, samplePtr);
		Paula::setChannelSampleLen(voiceNum, patch.sampleAttackLen << useOctave);
		Paula::setChannelPeriod(voiceNum, period);
		Paula::setChannelVolume(voiceNum, 0x64);

		Paula::enableChannel(voiceNum);
		// wait  for dma-clear
	}

	if (patch.sampleTotalLen > patch.sampleAttackLen) {
		Paula::setChannelSampleStart(voiceNum, samplePtr + patch.sampleAttackLen);
		Paula::setChannelSampleLen(voiceNum, (patch.sampleTotalLen - patch.sampleAttackLen) << useOctave);
		if (!patch.sampleAttackLen) {
			// need to enable channel
			Paula::setChannelPeriod(voiceNum, period);
			Paula::setChannelVolume(voiceNum, 0x64);

			Paula::enableChannel(voiceNum);
		}
		// another pointless wait for DMA-Clear???
	}

	channel.voicesActive++;
	if (&channel < &_channelCtx[kNumChannels]) {
		const byte flagsSet = ChannelContext::kFlagMono | ChannelContext::kFlagPortamento;
		if ((channel.flags & flagsSet) == flagsSet && channel.lastNote < 0x80 && channel.lastNote != voice.baseNote) {
			voice.portaTicks = 0;
			voice.endNote = voice.baseNote;
			voice.baseNote = channel.lastNote;
			voice.flags |= VoiceContext::kFlagPortamento;
		}
		if ((channel.flags & ChannelContext::kFlagPortamento) != 0)
			channel.lastNote = note;
	}
	_playerCtx.addedNote = true;
	_playerCtx.lastVoice = voiceNum;
	return voiceNum;
}

void MaxTrax::noteOff(ChannelContext &channel, const byte note) {
	VoiceContext &voice = _voiceCtx[_playerCtx.lastVoice];
	if (channel.voicesActive && voice.channel == &channel && voice.status != VoiceContext::kStatusRelease) {
		const byte refNote = ((voice.flags & VoiceContext::kFlagPortamento) != 0) ? voice.endNote : voice.baseNote;
		if (refNote == note) {
			if ((channel.flags & ChannelContext::kFlagDamper) != 0)
				voice.flags |= VoiceContext::kFlagDamper;
			else
				voice.status = VoiceContext::kStatusRelease;
		}
	}
}

void MaxTrax::freeScores() {
	if (_scores) {
		for (int i = 0; i < _numScores; ++i)
			delete _scores[i].events;
		delete _scores;
		_scores = 0;
	}
	_numScores = 0;
	memset(_microtonal, 0, sizeof(_microtonal));
}

void MaxTrax::freePatches() {
	for (int i = 0; i < ARRAYSIZE(_patch); ++i) {
		delete[] _patch[i].samplePtr;
		delete[] _patch[i].attackPtr;
	}
	memset(_patch, 0, sizeof(_patch));
}

bool MaxTrax::load(Common::SeekableReadStream &musicData, bool loadScores, bool loadSamples) {
	bool res = false;
	stopMusic();
	if (loadSamples)
		freePatches();
	if (loadScores)
		freeScores();
	// 0x0000: 4 Bytes Header "MXTX"
	// 0x0004: uint16 tempo
	// 0x0006: uint16 flags. bit0 = lowpassfilter, bit1 = attackvolume, bit15 = microtonal	
	if (musicData.readUint32BE() != 0x4D585458) {
		warning("Maxtrax: File is not a Maxtrax Module");
		return false;
	}
	_playerCtx.tempoInitial = musicData.readUint16BE();
	const uint16 flags = musicData.readUint16BE();
	_playerCtx.filterOn = (flags & 1) != 0;
	_playerCtx.handleVolume = (flags & 2) != 0;
	debug("Header: MXTX %02X %02X", _playerCtx.tempo, flags);

	if (loadScores && flags & (1 << 15)) {
		debug("Song has microtonal");
		for (int i = 0; i < ARRAYSIZE(_microtonal); ++i)
			_microtonal[i] = musicData.readUint16BE();
	}

	int scoresLoaded = 0;
	// uint16 number of Scores
	const uint16 scoresInFile = musicData.readUint16BE();

	if (loadScores) {
		const uint16 tempScores = MIN(scoresInFile, _playerCtx.maxScoreNum);
		debug("#Scores: %d, loading # of scores: %d", scoresInFile, tempScores);
		Score *curScore =_scores = new Score[tempScores];
		
		for (int i = tempScores; i > 0; --i, ++curScore) {
			const uint32 numEvents = musicData.readUint32BE();
			Event *curEvent = curScore->events = new Event[numEvents];
			for (int j = numEvents; j > 0; --j, ++curEvent) {
				curEvent->command = musicData.readByte();
				curEvent->parameter = musicData.readByte();
				curEvent->startTime = musicData.readUint16BE();
				curEvent->stopTime = musicData.readUint16BE();
			}
			curScore->numEvents = numEvents;
		}
		_numScores = scoresLoaded = tempScores;
	}

	if (false && !loadSamples)
		return true;

	// skip over remaining scores in file
	for (int i = scoresInFile - scoresLoaded; i > 0; --i)
		musicData.skip(musicData.readUint32BE() * 6);

	for (int i = 0; i < _numScores; ++i)
		outPutScore(_scores[i], i);

	debug("samples start at filepos %08X", musicData.pos());
	// uint16 number of Samples
	const uint16 wavesInFile = musicData.readUint16BE();
	if (loadSamples) {
		for (int i = wavesInFile; i > 0; --i) {
			// load disksample structure
			const uint16 number = musicData.readUint16BE();
			assert(number < ARRAYSIZE(_patch));
			// pointer to samples needed?
			Patch &curPatch = _patch[number];

			curPatch.tune = musicData.readUint16BE();
			curPatch.volume = musicData.readUint16BE();
			curPatch.sampleOctaves = musicData.readUint16BE();
			curPatch.sampleAttackLen = musicData.readUint32BE();
			const uint32 sustainLen = musicData.readUint32BE();
			curPatch.sampleTotalLen = curPatch.sampleAttackLen + sustainLen;
			// each octave the number of samples doubles.
			const uint32 totalSamples = curPatch.sampleTotalLen * ((1 << curPatch.sampleOctaves) - 1);
			curPatch.attackLen = musicData.readUint16BE();
			curPatch.releaseLen = musicData.readUint16BE();
			const uint32 totalEnvs = curPatch.attackLen + curPatch.releaseLen;

			debug("wave nr %d at %08X - %d octaves", number, musicData.pos(), curPatch.sampleOctaves);
			// Allocate space for both attack and release Segment.
			Envelope *envPtr = new Envelope[totalEnvs];
			// Attack Segment
			curPatch.attackPtr = envPtr;
			// Release Segment
			// curPatch.releasePtr = envPtr + curPatch.attackLen;

			// Read Attack and Release Segments
			for (int j = totalEnvs; j > 0; --j, ++envPtr) {
				envPtr->duration = musicData.readUint16BE();
				envPtr->volume = musicData.readUint16BE();
			}

			// read Samples
			curPatch.samplePtr = new int8[totalSamples];
			musicData.read(curPatch.samplePtr, totalSamples);
		}
	} else if (wavesInFile > 0){
		uint32 skipLen = 3 * 2;
		for (int i = wavesInFile; i > 0; --i) {
			musicData.skip(skipLen);
			const uint16 octaves = musicData.readUint16BE();
			const uint32 attackLen = musicData.readUint32BE();
			const uint32 sustainLen = musicData.readUint32BE();
			const uint16 attackCount = musicData.readUint16BE();
			const uint16 releaseCount = musicData.readUint16BE();
			debug("wave nr %d at %08X", 0, musicData.pos());
			
			skipLen = attackCount * 4 + releaseCount * 4 
				+ (attackLen + sustainLen) * ((1 << octaves) - 1)
				+ 3 * 2;
		}
		musicData.skip(skipLen - 3 * 2);
	}
	debug("endpos %08X", musicData.pos());
	return res;
}

}	// End of namespace Audio