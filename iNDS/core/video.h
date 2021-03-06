/*
	Copyright (C) 2009-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "videofilter.h"
#include "libkern/OSAtomic.h"
#include "gfx3d.h"

class VideoInfo
{
public:

	int width;
	int height;

	int rotation;
	int rotation_userset;
	int screengap;
	int layout;
	int layout_old;
	int	swap;

	int currentfilter;

	pthread_mutex_t bufferMutex;
	int currentBuffer;

	CACHE_ALIGN u32 buffer[2][16*256*192*2];
	CACHE_ALIGN u32 filteredbuffer[16*256*192*2];

	enum {
		NONE,
		HQ2X,
		_2XSAI,
		SUPER2XSAI,
		SUPEREAGLE,
		SCANLINE,
		BILINEAR,
		NEAREST2X,
		HQ2XS,
		LQ2X,
		LQ2XS,
        EPX,
        NEARESTPLUS1POINT5,
        NEAREST1POINT5,
        EPXPLUS,
        EPX1POINT5,
        EPXPLUS1POINT5,
        HQ4X,
        BRZ2x,
        BRZ3x,
        BRZ4x,
    BRZ5x,

		NUM_FILTERS,
	};

	VideoInfo() {
		pthread_mutex_init(&bufferMutex, NULL);
	}

	void reset() {
		width = 256;
		height = 384;
	}

	void setfilter(int filter) {

		if(filter < 0 || filter >= NUM_FILTERS)
			filter = NONE;

		currentfilter = filter;

		switch(filter) {

			case NONE:
				width = 256;
				height = 384;
				break;
			case EPX1POINT5:
			case EPXPLUS1POINT5:
			case NEAREST1POINT5:
			case NEARESTPLUS1POINT5:
				width = 256*3/2;
				height = 384*3/2;
				break;
            case BRZ3x:
                width = 256*3;
                height = 384*3;
                break;
            case HQ4X:
            case BRZ4x:
				width = 256*4;
				height = 384*4;
                break;
            case BRZ5x:
                width = 256*5;
                height = 384*5;
                break;
			default:
				width = 256*2;
				height = 384*2;
				break;
		}
	}

	SSurface src;
	SSurface dst;

	void copyBuffer(u8* srcBuffer)
	{
		pthread_mutex_lock(&bufferMutex);
		//convert pixel format to 32bpp for compositing
		//why do we do this over and over? well, we are compositing to
		//filteredbuffer32bpp, and it needs to get refreshed each frame..
		const int size = this->size();
		u16* src = (u16*)srcBuffer;
		u32* dest = buffer[currentBuffer];
		for(int i=0;i<size;++i)
			*dest++ = 0xFF000000 | RGB15TO32_NOALPHA(src[i]);
		pthread_mutex_unlock(&bufferMutex);
	}

	u8* swapBuffer()
	{
		pthread_mutex_lock(&bufferMutex);
		u8* returnBuffer = (u8*)buffer[currentBuffer];
		currentBuffer = currentBuffer == 0 ? 1 : 0;
		pthread_mutex_unlock(&bufferMutex);
		return returnBuffer;
	}

	u16* finalBuffer() const
	{
		return (u16*)filteredbuffer;
	}

	void filter() {
		src.Height = 384;
		src.Width = 256;
		src.Pitch = 512;
		src.Surface = swapBuffer();
		currentBuffer = currentBuffer == 0 ? 1 : 0;

		dst.Height = height;
		dst.Width = width;
		dst.Pitch = width*2;
		dst.Surface = (u8*)filteredbuffer;

		switch(currentfilter)
		{
			case NONE:
				memcpy(dst.Surface, src.Surface, dst.Width * dst.Height * sizeof(uint32_t));
				break;
			case LQ2X:
				RenderLQ2X(src, dst);
				break;
			case LQ2XS:
				RenderLQ2XS(src, dst);
				break;
			case HQ2X:
				RenderHQ2X(src, dst);
				break;
			case HQ4X:
				RenderHQ4X(src, dst);
				break;
			case HQ2XS:
				RenderHQ2XS(src, dst);
				break;
			case _2XSAI:
				Render2xSaI (src, dst);
				break;
			case SUPER2XSAI:
				RenderSuper2xSaI (src, dst);
				break;
			case SUPEREAGLE:
				RenderSuperEagle (src, dst);
				break;
			case SCANLINE:
				RenderScanline(src, dst);
				break;
			case BILINEAR:
				RenderBilinear(src, dst);
				break;
			case NEAREST2X:
				RenderNearest2X(src,dst);
				break;
			case EPX:
				RenderEPX(src,dst);
				break;
			case EPXPLUS:
				RenderEPXPlus(src,dst);
				break;
			case EPX1POINT5:
				RenderEPX_1Point5x(src,dst);
				break;
			case EPXPLUS1POINT5:
				RenderEPXPlus_1Point5x(src,dst);
				break;
			case NEAREST1POINT5:
				RenderNearest_1Point5x(src,dst);
				break;
            case NEARESTPLUS1POINT5:
                RenderNearestPlus_1Point5x(src,dst);
                break;
            case BRZ2x:
                Render2xBRZ(src,dst);
                break;
            case BRZ3x:
                Render3xBRZ(src,dst);
                break;
            case BRZ4x:
                Render4xBRZ(src,dst);
                break;
            case BRZ5x:
                Render5xBRZ(src,dst);
                break;
		}
	}

	int size() {
		return width*height;
	}

	int dividebyratio(int x) {
		return x * 256 / width;
	}

	int rotatedwidth() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height;
			case 180:
				return width;
			case 270:
				return height;
			default:
				return 0;
		}
	}

	int rotatedheight() {
		switch(rotation) {
			case 0:
				return height;
			case 90:
				return width;
			case 180:
				return height;
			case 270:
				return width;
			default:
				return 0;
		}
	}

	int rotatedwidthgap() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height + ((layout == 0) ? scaledscreengap() : 0);
			case 180:
				return width;
			case 270:
				return height + ((layout == 0) ? scaledscreengap() : 0);
			default:
				return 0;
		}
	}

	int rotatedheightgap() {
		switch(rotation) {
			case 0:
				return height + ((layout == 0) ? scaledscreengap() : 0);
			case 90:
				return width;
			case 180:
				return height + ((layout == 0) ? scaledscreengap() : 0);
			case 270:
				return width;
			default:
				return 0;
		}
	}

	int scaledscreengap() {
		return screengap * height / 384;
	}
};
