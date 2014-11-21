/*
 OFX color-spaces transformations support as-well as bit-depth conversions.
 
 Copyright (C) 2014 INRIA
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.
 
 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France
 */

#include "ofxsLut.h"


namespace OFX {
    namespace Color {
        // compile-time endianness checking found on:
        // http://stackoverflow.com/questions/2100331/c-macro-definition-to-determine-big-endian-or-little-endian-machine
        // if(O32_HOST_ORDER == O32_BIG_ENDIAN) will always be optimized by gcc -O2
        enum
        {
            O32_LITTLE_ENDIAN = 0x03020100ul,
            O32_BIG_ENDIAN = 0x00010203ul,
            O32_PDP_ENDIAN = 0x01000302ul
        };
        
        static const union
        {
            uint8_t bytes[4];
            uint32_t value;
        }
        
        o32_host_order = {
            { 0, 1, 2, 3 }
        };
#define O32_HOST_ORDER (o32_host_order.value)
        unsigned short
        LutBase::hipart(const float f)
        {
            union
            {
                float f;
                unsigned short us[2];
            }
            
            tmp;
            
            tmp.us[0] = tmp.us[1] = 0;
            tmp.f = f;
            
            if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
                return tmp.us[0];
            } else if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
                return tmp.us[1];
            } else {
                assert( (O32_HOST_ORDER == O32_LITTLE_ENDIAN) || (O32_HOST_ORDER == O32_BIG_ENDIAN) );
                
                return 0;
            }
        }
        
        float
        LutBase::index_to_float(const unsigned short i)
        {
            union
            {
                float f;
                unsigned short us[2];
            }
            
            tmp;
            
            /* positive and negative zeros, and all gradual underflow, turn into zero: */
            if ( ( i < 0x80) || ( ( i >= 0x8000) && ( i < 0x8080) ) ) {
                return 0;
            }
            /* All NaN's and infinity turn into the largest possible legal float: */
            if ( ( i >= 0x7f80) && ( i < 0x8000) ) {
                return std::numeric_limits<float>::max();
            }
            if (i >= 0xff80) {
                return -std::numeric_limits<float>::max();
            }
            if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
                tmp.us[0] = i;
                tmp.us[1] = 0x8000;
            } else if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
                tmp.us[0] = 0x8000;
                tmp.us[1] = i;
            } else {
                assert( (O32_HOST_ORDER == O32_LITTLE_ENDIAN) || (O32_HOST_ORDER == O32_BIG_ENDIAN) );
            }
            
            return tmp.f;
        }
        
        ///initialize the singleton
        LutManager LutManager::m_instance = LutManager();
        LutManager::LutManager()
        : luts()
        {
        }
        
        
        LutManager::~LutManager()
        {
            ////the luts must all have been released before!
            ////This is because the Lut holds a OFX::MultiThread::Mutex and it can't be deleted
            //// by this singleton because it makes their destruction time uncertain regarding to
            ///the host multi-thread suite.
            for (LutsMap::iterator it = luts.begin(); it != luts.end(); ++it) {
                delete it->second;
            }
        }
  
        
        ///////////////////////
        /////////////////////////////////////////// LINEAR //////////////////////////////////////////////
        ///////////////////////
        
        namespace Linear {
            
            
            void
            to_byte_packed(unsigned char* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                           const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes)
            {
                int srcElements = srcRowBytes / sizeof(float);
                int dstElements = dstRowBytes / sizeof(unsigned char);

                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const float *src_pixels = from + (y * srcElements) + renderWindow.x1 * nComponents;
                    unsigned char *dst_pixels = to + (y * dstElements) + renderWindow.x1 * nComponents;
                    
                    
                    const float* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        for (int k = 0; k < nComponents; ++k) {
                            *dst_pixels++ = floatToInt<256>(*src_pixels++);
                        }
                    }
                }
            }
            
            void
            to_short_packed(unsigned short* to, const float* from,const OfxRectI & renderWindow, int nComponents,
                            const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes)
            {
                int srcElements = srcRowBytes / sizeof(float);
                int dstElements = dstRowBytes / sizeof(unsigned short);
                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const float *src_pixels = from + (y * srcElements) + renderWindow.x1 * nComponents;
                    unsigned short *dst_pixels = to + (y * dstElements) + renderWindow.x1 * nComponents;
                    
                    
                    const float* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        for (int k = 0; k < nComponents; ++k) {
                            *dst_pixels++ = floatToInt<65536>(*src_pixels++);
                        }
                    }
                }
                
            }
            
            
            void
            from_byte_packed(float* to, const unsigned char* from,const OfxRectI & renderWindow, int nComponents,
                             const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes)
            {
                int srcElements = srcRowBytes / sizeof(unsigned char);
                int dstElements = dstRowBytes / sizeof(float);
                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const unsigned char *src_pixels = from + (y * srcElements) + renderWindow.x1 * nComponents;
                    float *dst_pixels = to + (y * dstRowBytes) + renderWindow.x1 * nComponents;
                    
                    
                    const unsigned char* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        for (int k = 0; k < nComponents; ++k) {
                            *dst_pixels++ = intToFloat<256>(*src_pixels++);
                        }
                    }
                }
                
            }
            
            void
            from_short_packed(float* to, const unsigned short* from,const OfxRectI & renderWindow, int nComponents,
                              const OfxRectI & srcBounds,int srcRowBytes,const OfxRectI & dstBounds,int dstRowBytes)
            {
                int srcElements = srcRowBytes / sizeof(unsigned short);
                int dstElements = dstRowBytes / sizeof(float);
                int w = renderWindow.x2 - renderWindow.x1;
                for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
                    
                    const unsigned short *src_pixels = from + (y * srcElements) + renderWindow.x1 * nComponents;
                    float *dst_pixels = to + (y * dstElements) + renderWindow.x1 * nComponents;
                    
                    
                    const unsigned short* src_end = src_pixels + w * nComponents;
                    
                    
                    while (src_pixels != src_end) {
                        for (int k = 0; k < nComponents; ++k) {
                            *dst_pixels++ = intToFloat<65536>(*src_pixels++);
                        }
                    }
                }
                
            }
        }
   
     

        // r,g,b values are from 0 to 1
        // h = [0,360], s = [0,1], v = [0,1]
        //		if s == 0, then h = 0 (undefined)
        void
        rgb_to_hsv( float r,
                   float g,
                   float b,
                   float *h,
                   float *s,
                   float *v )
        {
            float min, max, delta;
            
            min = std::min(std::min(r, g), b);
            max = std::max(std::max(r, g), b);
            *v = max;                       // v
            
            delta = max - min;
            
            if (max != 0.) {
                *s = delta / max;               // s
            } else {
                // r = g = b = 0		// s = 0, v is undefined
                *s = 0.;
                *h = 0.;
                
                return;
            }
            
            if (delta == 0.) {
                *h = 0.;         // gray
            } else if (r == max) {
                *h = (g - b) / delta;               // between yellow & magenta
            } else if (g == max) {
                *h = 2 + (b - r) / delta;           // between cyan & yellow
            } else {
                *h = 4 + (r - g) / delta;           // between magenta & cyan
            }
            *h *= 60;                       // degrees
            if (*h < 0) {
                *h += 360;
            }
        }
    }     // namespace Color
} //namespace OFX

