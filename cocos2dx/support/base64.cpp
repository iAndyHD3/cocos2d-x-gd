/****************************************************************************
Copyright (c) 2010 cocos2d-x.org

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "base64.h"

namespace cocos2d {

unsigned char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
unsigned char alphabetURL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int _base64Decode( unsigned char *input, unsigned int input_len, unsigned char *output, unsigned int *output_len, bool isURL)
{
    static char inalphabet[256], decoder[256];
    int i, bits, c = 0, char_count, errors = 0;
    unsigned int input_idx = 0;
    unsigned int output_idx = 0;

    unsigned char* alpha = isURL ? alphabetURL : alphabet;

    for (i = (sizeof alphabet) - 1; i >= 0 ; i--) {
        inalphabet[alpha[i]] = 1;
        decoder[alpha[i]] = i;
    }

    char_count = 0;
    bits = 0;
    for( input_idx=0; input_idx < input_len ; input_idx++ ) {
        c = input[ input_idx ];
        if (c == '=')
            break;
        if (c > 255 || ! inalphabet[c])
            continue;
        bits += decoder[c];
        char_count++;
        if (char_count == 4) {
            output[ output_idx++ ] = (bits >> 16);
            output[ output_idx++ ] = ((bits >> 8) & 0xff);
            output[ output_idx++ ] = ( bits & 0xff);
            bits = 0;
            char_count = 0;
        } else {
            bits <<= 6;
        }
    }
    
    if( c == '=' ) {
        switch (char_count) {
            case 1:
#if (CC_TARGET_PLATFORM != CC_PLATFORM_BADA)
                std::fprintf(stderr, "base64Decode: encoding incomplete: at least 2 bits missing");
#endif
                errors++;
                break;
            case 2:
                output[ output_idx++ ] = ( bits >> 10 );
                break;
            case 3:
                output[ output_idx++ ] = ( bits >> 16 );
                output[ output_idx++ ] = (( bits >> 8 ) & 0xff);
                break;
            }
    } else if ( input_idx < input_len ) {
        if (char_count) {
#if (CC_TARGET_PLATFORM != CC_PLATFORM_BADA)
            std::fprintf(stderr, "base64 encoding incomplete: at least %d bits truncated",
                    ((4 - char_count) * 6));
#endif
            errors++;
        }
    }
    
    *output_len = output_idx;
    return errors;
}

int base64Decode(unsigned char *in, unsigned int inLength, unsigned char **out, bool isURL)
{
    unsigned int outLength = 0;
    
    //should be enough to store 6-bit buffers in 8-bit buffers
    *out = new unsigned char[(size_t)(inLength * 3.0f / 4.0f + 1)];
    if( *out ) {
        int ret = _base64Decode(in, inLength, *out, &outLength, isURL);
        
        if (ret > 0 )
        {
            printf("Base64Utils: error decoding");
            delete [] *out;
            *out = NULL;            
            outLength = 0;
        }
    }
    return outLength;
}

int _base64Encode(unsigned char *in, unsigned int inlen, char *out, unsigned int *outlen, bool isURL)
{
    unsigned char* alpha = isURL ? alphabetURL : alphabet;
    unsigned int i, j = 0;
    int result = 0;
    int char_count = 0;

    for (i = 0; i < inlen; i++) {
        result = (result << 8) | in[i];
        char_count++;
        if (char_count == 3) {
            out[j++] = alpha[(result >> 18) & 0x3F];
            out[j++] = alpha[(result >> 12) & 0x3F];
            out[j++] = alpha[(result >> 6) & 0x3F];
            out[j++] = alpha[result & 0x3F];
            result = 0;
            char_count = 0;
        }
    }

    if (char_count == 1) {
        result <<= 8;
        out[j++] = alpha[(result >> 18) & 0x3F];
        out[j++] = alpha[(result >> 12) & 0x3F];
        out[j++] = '=';
        out[j++] = '=';
    } else if (char_count == 2) {
        out[j++] = alpha[(result >> 18) & 0x3F];
        out[j++] = alpha[(result >> 12) & 0x3F];
        out[j++] = alpha[(result >> 6) & 0x3F];
        out[j++] = '=';
    }

    out[j] = 0;
    *outlen = j;
    return 0;
}

int base64Encode(unsigned char *in, unsigned int inLength, char **out, bool isURL)
{
    unsigned int outLength = 0;

    *out = (char*)malloc((size_t)(inLength * 4.0f / 3.0f + 4));
    if (*out) {
        int ret = _base64Encode(in, inLength, *out, &outLength, isURL);

        if (ret > 0) {
            printf("Base64Utils: error encoding");
            free(*out);
            *out = NULL;
            outLength = 0;
        }
    }
    return outLength;
}

}//namespace   cocos2d 
