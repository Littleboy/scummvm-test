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

// Currently, only GOB and SCI32 games play IMDs and VMDs, so skip compiling if GOB and SCI32 is disabled.
#if !(defined(ENABLE_GOB) || defined(ENABLE_SCI32) || defined(DYNAMIC_MODULES))

// Do not compile the CoktelDecoder code

#else

#ifndef GRAPHICS_VIDEO_COKTELDECODER_H
#define GRAPHICS_VIDEO_COKTELDECODER_H

#include "common/list.h"
#include "common/rect.h"

#include "graphics/video/video_decoder.h"

#include "sound/mixer.h"

namespace Graphics {

class CoktelDecoder : public FixedRateVideoDecoder {
public:
	struct State {
		/** Set accordingly to what was done. */
		uint32 flags;
		/** The id of the spoken words. */
		uint16 speechId;

		State();
	};

	CoktelDecoder(Audio::Mixer &mixer,
			Audio::Mixer::SoundType soundType = Audio::Mixer::kPlainSoundType);
	~CoktelDecoder();

	virtual bool seek(int32 frame, int whence = SEEK_SET, bool restart = false) = 0;

	/** Draw directly onto the specified video memory. */
	void setSurfaceMemory(void *mem, uint16 width, uint16 height, uint8 bpp);
	/** Reset the video memory. */
	void setSurfaceMemory();

	/** Draw the video starting at this position within the video memory. */
	void setXY(uint16 x, uint16 y);

	/** Override the video's frame rate. */
	void setFrameRate(Common::Rational frameRate);

	/** Return a list of rectangles that changed in the last frame. */
	const Common::List<Common::Rect> &getDirtyRects() const;

	// VideoDecoder interface

	void close();

	uint16 getWidth()  const;
	uint16 getHeight() const;

	uint32 getFrameCount() const;

	byte *getPalette();
	bool  hasDirtyPalette() const;

protected:
	Audio::Mixer *_mixer;
	Audio::Mixer::SoundType _soundType;

	uint16 _width;
	uint16 _height;

	uint16 _x;
	uint16 _y;

	uint32 _frameCount;

	byte _palette[768];
	bool _paletteDirty;

	bool    _ownSurface;
	Surface _surface;

	Common::List<Common::Rect> _dirtyRects;

	Common::Rational _frameRate;

	bool hasSurface();
	void createSurface();
	void freeSurface();

	// FixedRateVideoDecoder interface
	Common::Rational getFrameRate() const;
};

class PreIMDDecoder : public CoktelDecoder {
public:
	PreIMDDecoder(uint16 width, uint16 height, Audio::Mixer &mixer,
			Audio::Mixer::SoundType soundType = Audio::Mixer::kPlainSoundType);
	~PreIMDDecoder();

	bool seek(int32 frame, int whence = SEEK_SET, bool restart = false);

	// VideoDecoder interface

	bool load(Common::SeekableReadStream &stream);
	void close();

	bool isVideoLoaded() const;

	Surface *decodeNextFrame();

	PixelFormat getPixelFormat() const;

private:
	Common::SeekableReadStream *_stream;

	byte  *_videoBuffer;
	uint32 _videoBufferSize;

	void processFrame();
	void renderFrame();
};

} // End of namespace Graphics

#endif // GRAPHICS_VIDEO_COKTELDECODER_H

#endif // Engine and dynamic plugins guard
