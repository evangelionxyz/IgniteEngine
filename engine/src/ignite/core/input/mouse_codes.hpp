/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once
#include <cstdint>

#define IGNITE_BUTTON_LEFT     1
#define IGNITE_BUTTON_MIDDLE   2
#define IGNITE_BUTTON_RIGHT    3
#define IGNITE_BUTTON_X1       4
#define IGNITE_BUTTON_X2       5
#define IGNITE_BUTTON_MASK(X)  (1u << ((X)-1))
#define IGNITE_BUTTON_LMASK    IGNITE_BUTTON_MASK(IGNITE_BUTTON_LEFT)
#define IGNITE_BUTTON_MMASK    IGNITE_BUTTON_MASK(IGNITE_BUTTON_MIDDLE)
#define IGNITE_BUTTON_RMASK    IGNITE_BUTTON_MASK(IGNITE_BUTTON_RIGHT)
#define IGNITE_BUTTON_X1MASK   IGNITE_BUTTON_MASK(IGNITE_BUTTON_X1)
#define IGNITE_BUTTON_X2MASK   IGNITE_BUTTON_MASK(IGNITE_BUTTON_X2)

using MouseCode = uint8_t;
namespace ignite::Mouse {
	enum : MouseCode
	{
		ButtonLeft = IGNITE_BUTTON_LEFT,
		ButtonRight = IGNITE_BUTTON_RIGHT,
		ButtonMiddle = IGNITE_BUTTON_MIDDLE
	};
}
