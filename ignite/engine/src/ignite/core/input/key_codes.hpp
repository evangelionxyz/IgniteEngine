/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

using KeyCode = uint32_t;
using KeyModCode = uint16_t;

#define IGNITE_KEY_EXTENDED_MASK          (1u << 29)
#define IGNITE_KEY_SCANCODE_MASK          (1u << 30)
#define IGNITE_KEY_SCANCODE_TO_KEYCODE(X) (X | IGNITE_KEY_SCANCODE_MASK)
#define IGNITE_KEY_UNKNOWN                0x00000000u /**< 0 */
#define IGNITE_KEY_RETURN                 0x0000000du /**< '\r' */
#define IGNITE_KEY_ESCAPE                 0x0000001bu /**< '\x1B' */
#define IGNITE_KEY_BACKSPACE              0x00000008u /**< '\b' */
#define IGNITE_KEY_TAB                    0x00000009u /**< '\t' */
#define IGNITE_KEY_SPACE                  0x00000020u /**< ' ' */
#define IGNITE_KEY_EXCLAIM                0x00000021u /**< '!' */
#define IGNITE_KEY_DBLAPOSTROPHE          0x00000022u /**< '"' */
#define IGNITE_KEY_HASH                   0x00000023u /**< '#' */
#define IGNITE_KEY_DOLLAR                 0x00000024u /**< '$' */
#define IGNITE_KEY_PERCENT                0x00000025u /**< '%' */
#define IGNITE_KEY_AMPERSAND              0x00000026u /**< '&' */
#define IGNITE_KEY_APOSTROPHE             0x00000027u /**< '\'' */
#define IGNITE_KEY_LEFTPAREN              0x00000028u /**< '(' */
#define IGNITE_KEY_RIGHTPAREN             0x00000029u /**< ')' */
#define IGNITE_KEY_ASTERISK               0x0000002au /**< '*' */
#define IGNITE_KEY_PLUS                   0x0000002bu /**< '+' */
#define IGNITE_KEY_COMMA                  0x0000002cu /**< ',' */
#define IGNITE_KEY_MINUS                  0x0000002du /**< '-' */
#define IGNITE_KEY_PERIOD                 0x0000002eu /**< '.' */
#define IGNITE_KEY_SLASH                  0x0000002fu /**< '/' */
#define IGNITE_KEY_0                      0x00000030u /**< '0' */
#define IGNITE_KEY_1                      0x00000031u /**< '1' */
#define IGNITE_KEY_2                      0x00000032u /**< '2' */
#define IGNITE_KEY_3                      0x00000033u /**< '3' */
#define IGNITE_KEY_4                      0x00000034u /**< '4' */
#define IGNITE_KEY_5                      0x00000035u /**< '5' */
#define IGNITE_KEY_6                      0x00000036u /**< '6' */
#define IGNITE_KEY_7                      0x00000037u /**< '7' */
#define IGNITE_KEY_8                      0x00000038u /**< '8' */
#define IGNITE_KEY_9                      0x00000039u /**< '9' */
#define IGNITE_KEY_COLON                  0x0000003au /**< ':' */
#define IGNITE_KEY_SEMICOLON              0x0000003bu /**< ';' */
#define IGNITE_KEY_LESS                   0x0000003cu /**< '<' */
#define IGNITE_KEY_EQUALS                 0x0000003du /**< '=' */
#define IGNITE_KEY_GREATER                0x0000003eu /**< '>' */
#define IGNITE_KEY_QUESTION               0x0000003fu /**< '?' */
#define IGNITE_KEY_AT                     0x00000040u /**< '@' */
#define IGNITE_KEY_LEFTBRACKET            0x0000005bu /**< '[' */
#define IGNITE_KEY_BACKSLASH              0x0000005cu /**< '\\' */
#define IGNITE_KEY_RIGHTBRACKET           0x0000005du /**< ']' */
#define IGNITE_KEY_CARET                  0x0000005eu /**< '^' */
#define IGNITE_KEY_UNDERSCORE             0x0000005fu /**< '_' */
#define IGNITE_KEY_GRAVE                  0x00000060u /**< '`' */
#define IGNITE_KEY_A                      0x00000061u /**< 'a' */
#define IGNITE_KEY_B                      0x00000062u /**< 'b' */
#define IGNITE_KEY_C                      0x00000063u /**< 'c' */
#define IGNITE_KEY_D                      0x00000064u /**< 'd' */
#define IGNITE_KEY_E                      0x00000065u /**< 'e' */
#define IGNITE_KEY_F                      0x00000066u /**< 'f' */
#define IGNITE_KEY_G                      0x00000067u /**< 'g' */
#define IGNITE_KEY_H                      0x00000068u /**< 'h' */
#define IGNITE_KEY_I                      0x00000069u /**< 'i' */
#define IGNITE_KEY_J                      0x0000006au /**< 'j' */
#define IGNITE_KEY_K                      0x0000006bu /**< 'k' */
#define IGNITE_KEY_L                      0x0000006cu /**< 'l' */
#define IGNITE_KEY_M                      0x0000006du /**< 'm' */
#define IGNITE_KEY_N                      0x0000006eu /**< 'n' */
#define IGNITE_KEY_O                      0x0000006fu /**< 'o' */
#define IGNITE_KEY_P                      0x00000070u /**< 'p' */
#define IGNITE_KEY_Q                      0x00000071u /**< 'q' */
#define IGNITE_KEY_R                      0x00000072u /**< 'r' */
#define IGNITE_KEY_S                      0x00000073u /**< 's' */
#define IGNITE_KEY_T                      0x00000074u /**< 't' */
#define IGNITE_KEY_U                      0x00000075u /**< 'u' */
#define IGNITE_KEY_V                      0x00000076u /**< 'v' */
#define IGNITE_KEY_W                      0x00000077u /**< 'w' */
#define IGNITE_KEY_X                      0x00000078u /**< 'x' */
#define IGNITE_KEY_Y                      0x00000079u /**< 'y' */
#define IGNITE_KEY_Z                      0x0000007au /**< 'z' */
#define IGNITE_KEY_LEFTBRACE              0x0000007bu /**< '{' */
#define IGNITE_KEY_PIPE                   0x0000007cu /**< '|' */
#define IGNITE_KEY_RIGHTBRACE             0x0000007du /**< '}' */
#define IGNITE_KEY_TILDE                  0x0000007eu /**< '~' */
#define IGNITE_KEY_DELETE                 0x0000007fu /**< '\x7F' */
#define IGNITE_KEY_PLUSMINUS              0x000000b1u /**< '\xB1' */
#define IGNITE_KEY_CAPSLOCK               0x40000039u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CAPSLOCK) */
#define IGNITE_KEY_F1                     0x4000003au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1) */
#define IGNITE_KEY_F2                     0x4000003bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F2) */
#define IGNITE_KEY_F3                     0x4000003cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F3) */
#define IGNITE_KEY_F4                     0x4000003du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F4) */
#define IGNITE_KEY_F5                     0x4000003eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F5) */
#define IGNITE_KEY_F6                     0x4000003fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F6) */
#define IGNITE_KEY_F7                     0x40000040u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F7) */
#define IGNITE_KEY_F8                     0x40000041u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F8) */
#define IGNITE_KEY_F9                     0x40000042u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F9) */
#define IGNITE_KEY_F10                    0x40000043u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F10) */
#define IGNITE_KEY_F11                    0x40000044u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F11) */
#define IGNITE_KEY_F12                    0x40000045u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F12) */
#define IGNITE_KEY_PRINTSCREEN            0x40000046u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PRINTSCREEN) */
#define IGNITE_KEY_SCROLLLOCK             0x40000047u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SCROLLLOCK) */
#define IGNITE_KEY_PAUSE                  0x40000048u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAUSE) */
#define IGNITE_KEY_INSERT                 0x40000049u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INSERT) */
#define IGNITE_KEY_HOME                   0x4000004au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_HOME) */
#define IGNITE_KEY_PAGEUP                 0x4000004bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEUP) */
#define IGNITE_KEY_END                    0x4000004du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_END) */
#define IGNITE_KEY_PAGEDOWN               0x4000004eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEDOWN) */
#define IGNITE_KEY_RIGHT                  0x4000004fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RIGHT) */
#define IGNITE_KEY_LEFT                   0x40000050u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LEFT) */
#define IGNITE_KEY_DOWN                   0x40000051u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DOWN) */
#define IGNITE_KEY_UP                     0x40000052u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UP) */
#define IGNITE_KEY_NUMLOCKCLEAR           0x40000053u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_NUMLOCKCLEAR) */
#define IGNITE_KEY_KP_DIVIDE              0x40000054u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_DIVIDE) */
#define IGNITE_KEY_KP_MULTIPLY            0x40000055u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MULTIPLY) */
#define IGNITE_KEY_KP_MINUS               0x40000056u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MINUS) */
#define IGNITE_KEY_KP_PLUS                0x40000057u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PLUS) */
#define IGNITE_KEY_KP_ENTER               0x40000058u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_ENTER) */
#define IGNITE_KEY_KP_1                   0x40000059u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_1) */
#define IGNITE_KEY_KP_2                   0x4000005au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_2) */
#define IGNITE_KEY_KP_3                   0x4000005bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_3) */
#define IGNITE_KEY_KP_4                   0x4000005cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_4) */
#define IGNITE_KEY_KP_5                   0x4000005du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_5) */
#define IGNITE_KEY_KP_6                   0x4000005eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_6) */
#define IGNITE_KEY_KP_7                   0x4000005fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_7) */
#define IGNITE_KEY_KP_8                   0x40000060u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_8) */
#define IGNITE_KEY_KP_9                   0x40000061u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_9) */
#define IGNITE_KEY_KP_0                   0x40000062u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_0) */
#define IGNITE_KEY_KP_PERIOD              0x40000063u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PERIOD) */
#define IGNITE_KEY_APPLICATION            0x40000065u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_APPLICATION) */
#define IGNITE_KEY_POWER                  0x40000066u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_POWER) */
#define IGNITE_KEY_KP_EQUALS              0x40000067u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_EQUALS) */
#define IGNITE_KEY_F13                    0x40000068u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F13) */
#define IGNITE_KEY_F14                    0x40000069u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F14) */
#define IGNITE_KEY_F15                    0x4000006au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F15) */
#define IGNITE_KEY_F16                    0x4000006bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F16) */
#define IGNITE_KEY_F17                    0x4000006cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F17) */
#define IGNITE_KEY_F18                    0x4000006du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F18) */
#define IGNITE_KEY_F19                    0x4000006eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F19) */
#define IGNITE_KEY_F20                    0x4000006fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F20) */
#define IGNITE_KEY_F21                    0x40000070u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F21) */
#define IGNITE_KEY_F22                    0x40000071u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F22) */
#define IGNITE_KEY_F23                    0x40000072u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F23) */
#define IGNITE_KEY_F24                    0x40000073u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F24) */
#define IGNITE_KEY_EXECUTE                0x40000074u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_EXECUTE) */
#define IGNITE_KEY_HELP                   0x40000075u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_HELP) */
#define IGNITE_KEY_MENU                   0x40000076u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MENU) */
#define IGNITE_KEY_SELECT                 0x40000077u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SELECT) */
#define IGNITE_KEY_STOP                   0x40000078u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_STOP) */
#define IGNITE_KEY_AGAIN                  0x40000079u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AGAIN) */
#define IGNITE_KEY_UNDO                   0x4000007au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UNDO) */
#define IGNITE_KEY_CUT                    0x4000007bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CUT) */
#define IGNITE_KEY_COPY                   0x4000007cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_COPY) */
#define IGNITE_KEY_PASTE                  0x4000007du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PASTE) */
#define IGNITE_KEY_FIND                   0x4000007eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_FIND) */
#define IGNITE_KEY_MUTE                   0x4000007fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MUTE) */
#define IGNITE_KEY_VOLUMEUP               0x40000080u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_VOLUMEUP) */
#define IGNITE_KEY_VOLUMEDOWN             0x40000081u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_VOLUMEDOWN) */
#define IGNITE_KEY_KP_COMMA               0x40000085u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_COMMA) */
#define IGNITE_KEY_KP_EQUALSAS400         0x40000086u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_EQUALSAS400) */
#define IGNITE_KEY_ALTERASE               0x40000099u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_ALTERASE) */
#define IGNITE_KEY_SYSREQ                 0x4000009au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SYSREQ) */
#define IGNITE_KEY_CANCEL                 0x4000009bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CANCEL) */
#define IGNITE_KEY_CLEAR                  0x4000009cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CLEAR) */
#define IGNITE_KEY_PRIOR                  0x4000009du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PRIOR) */
#define IGNITE_KEY_RETURN2                0x4000009eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RETURN2) */
#define IGNITE_KEY_SEPARATOR              0x4000009fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SEPARATOR) */
#define IGNITE_KEY_OUT                    0x400000a0u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_OUT) */
#define IGNITE_KEY_OPER                   0x400000a1u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_OPER) */
#define IGNITE_KEY_CLEARAGAIN             0x400000a2u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CLEARAGAIN) */
#define IGNITE_KEY_CRSEL                  0x400000a3u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CRSEL) */
#define IGNITE_KEY_EXSEL                  0x400000a4u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_EXSEL) */
#define IGNITE_KEY_KP_00                  0x400000b0u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_00) */
#define IGNITE_KEY_KP_000                 0x400000b1u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_000) */
#define IGNITE_KEY_THOUSANDSSEPARATOR     0x400000b2u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_THOUSANDSSEPARATOR) */
#define IGNITE_KEY_DECIMALSEPARATOR       0x400000b3u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DECIMALSEPARATOR) */
#define IGNITE_KEY_CURRENCYUNIT           0x400000b4u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CURRENCYUNIT) */
#define IGNITE_KEY_CURRENCYSUBUNIT        0x400000b5u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CURRENCYSUBUNIT) */
#define IGNITE_KEY_KP_LEFTPAREN           0x400000b6u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_LEFTPAREN) */
#define IGNITE_KEY_KP_RIGHTPAREN          0x400000b7u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_RIGHTPAREN) */
#define IGNITE_KEY_KP_LEFTBRACE           0x400000b8u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_LEFTBRACE) */
#define IGNITE_KEY_KP_RIGHTBRACE          0x400000b9u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_RIGHTBRACE) */
#define IGNITE_KEY_KP_TAB                 0x400000bau /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_TAB) */
#define IGNITE_KEY_KP_BACKSPACE           0x400000bbu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_BACKSPACE) */
#define IGNITE_KEY_KP_A                   0x400000bcu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_A) */
#define IGNITE_KEY_KP_B                   0x400000bdu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_B) */
#define IGNITE_KEY_KP_C                   0x400000beu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_C) */
#define IGNITE_KEY_KP_D                   0x400000bfu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_D) */
#define IGNITE_KEY_KP_E                   0x400000c0u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_E) */
#define IGNITE_KEY_KP_F                   0x400000c1u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_F) */
#define IGNITE_KEY_KP_XOR                 0x400000c2u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_XOR) */
#define IGNITE_KEY_KP_POWER               0x400000c3u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_POWER) */
#define IGNITE_KEY_KP_PERCENT             0x400000c4u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PERCENT) */
#define IGNITE_KEY_KP_LESS                0x400000c5u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_LESS) */
#define IGNITE_KEY_KP_GREATER             0x400000c6u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_GREATER) */
#define IGNITE_KEY_KP_AMPERSAND           0x400000c7u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_AMPERSAND) */
#define IGNITE_KEY_KP_DBLAMPERSAND        0x400000c8u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_DBLAMPERSAND) */
#define IGNITE_KEY_KP_VERTICALBAR         0x400000c9u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_VERTICALBAR) */
#define IGNITE_KEY_KP_DBLVERTICALBAR      0x400000cau /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_DBLVERTICALBAR) */
#define IGNITE_KEY_KP_COLON               0x400000cbu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_COLON) */
#define IGNITE_KEY_KP_HASH                0x400000ccu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_HASH) */
#define IGNITE_KEY_KP_SPACE               0x400000cdu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_SPACE) */
#define IGNITE_KEY_KP_AT                  0x400000ceu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_AT) */
#define IGNITE_KEY_KP_EXCLAM              0x400000cfu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_EXCLAM) */
#define IGNITE_KEY_KP_MEMSTORE            0x400000d0u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMSTORE) */
#define IGNITE_KEY_KP_MEMRECALL           0x400000d1u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMRECALL) */
#define IGNITE_KEY_KP_MEMCLEAR            0x400000d2u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMCLEAR) */
#define IGNITE_KEY_KP_MEMADD              0x400000d3u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMADD) */
#define IGNITE_KEY_KP_MEMSUBTRACT         0x400000d4u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMSUBTRACT) */
#define IGNITE_KEY_KP_MEMMULTIPLY         0x400000d5u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMMULTIPLY) */
#define IGNITE_KEY_KP_MEMDIVIDE           0x400000d6u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MEMDIVIDE) */
#define IGNITE_KEY_KP_PLUSMINUS           0x400000d7u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PLUSMINUS) */
#define IGNITE_KEY_KP_CLEAR               0x400000d8u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_CLEAR) */
#define IGNITE_KEY_KP_CLEARENTRY          0x400000d9u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_CLEARENTRY) */
#define IGNITE_KEY_KP_BINARY              0x400000dau /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_BINARY) */
#define IGNITE_KEY_KP_OCTAL               0x400000dbu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_OCTAL) */
#define IGNITE_KEY_KP_DECIMAL             0x400000dcu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_DECIMAL) */
#define IGNITE_KEY_KP_HEXADECIMAL         0x400000ddu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_HEXADECIMAL) */
#define IGNITE_KEY_LCTRL                  0x400000e0u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LCTRL) */
#define IGNITE_KEY_LSHIFT                 0x400000e1u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LSHIFT) */
#define IGNITE_KEY_LALT                   0x400000e2u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LALT) */
#define IGNITE_KEY_LGUI                   0x400000e3u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LGUI) */
#define IGNITE_KEY_RCTRL                  0x400000e4u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RCTRL) */
#define IGNITE_KEY_RSHIFT                 0x400000e5u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RSHIFT) */
#define IGNITE_KEY_RALT                   0x400000e6u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RALT) */
#define IGNITE_KEY_RGUI                   0x400000e7u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RGUI) */
#define IGNITE_KEY_MODE                   0x40000101u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MODE) */
#define IGNITE_KEY_SLEEP                  0x40000102u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SLEEP) */
#define IGNITE_KEY_WAKE                   0x40000103u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_WAKE) */
#define IGNITE_KEY_CHANNEL_INCREMENT      0x40000104u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CHANNEL_INCREMENT) */
#define IGNITE_KEY_CHANNEL_DECREMENT      0x40000105u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CHANNEL_DECREMENT) */
#define IGNITE_KEY_MEDIA_PLAY             0x40000106u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_PLAY) */
#define IGNITE_KEY_MEDIA_PAUSE            0x40000107u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_PAUSE) */
#define IGNITE_KEY_MEDIA_RECORD           0x40000108u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_RECORD) */
#define IGNITE_KEY_MEDIA_FAST_FORWARD     0x40000109u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_FAST_FORWARD) */
#define IGNITE_KEY_MEDIA_REWIND           0x4000010au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_REWIND) */
#define IGNITE_KEY_MEDIA_NEXT_TRACK       0x4000010bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_NEXT_TRACK) */
#define IGNITE_KEY_MEDIA_PREVIOUS_TRACK   0x4000010cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_PREVIOUS_TRACK) */
#define IGNITE_KEY_MEDIA_STOP             0x4000010du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_STOP) */
#define IGNITE_KEY_MEDIA_EJECT            0x4000010eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_EJECT) */
#define IGNITE_KEY_MEDIA_PLAY_PAUSE       0x4000010fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_PLAY_PAUSE) */
#define IGNITE_KEY_MEDIA_SELECT           0x40000110u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_MEDIA_SELECT) */
#define IGNITE_KEY_AC_NEW                 0x40000111u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_NEW) */
#define IGNITE_KEY_AC_OPEN                0x40000112u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_OPEN) */
#define IGNITE_KEY_AC_CLOSE               0x40000113u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_CLOSE) */
#define IGNITE_KEY_AC_EXIT                0x40000114u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_EXIT) */
#define IGNITE_KEY_AC_SAVE                0x40000115u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_SAVE) */
#define IGNITE_KEY_AC_PRINT               0x40000116u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_PRINT) */
#define IGNITE_KEY_AC_PROPERTIES          0x40000117u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_PROPERTIES) */
#define IGNITE_KEY_AC_SEARCH              0x40000118u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_SEARCH) */
#define IGNITE_KEY_AC_HOME                0x40000119u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_HOME) */
#define IGNITE_KEY_AC_BACK                0x4000011au /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_BACK) */
#define IGNITE_KEY_AC_FORWARD             0x4000011bu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_FORWARD) */
#define IGNITE_KEY_AC_STOP                0x4000011cu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_STOP) */
#define IGNITE_KEY_AC_REFRESH             0x4000011du /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_REFRESH) */
#define IGNITE_KEY_AC_BOOKMARKS           0x4000011eu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_AC_BOOKMARKS) */
#define IGNITE_KEY_SOFTLEFT               0x4000011fu /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SOFTLEFT) */
#define IGNITE_KEY_SOFTRIGHT              0x40000120u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SOFTRIGHT) */
#define IGNITE_KEY_CALL                   0x40000121u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CALL) */
#define IGNITE_KEY_ENDCALL                0x40000122u /**< SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_ENDCALL) */
#define IGNITE_KEY_LEFT_TAB               0x20000001u /**< Extended key Left Tab */
#define IGNITE_KEY_LEVEL5_SHIFT           0x20000002u /**< Extended key Level 5 Shift */
#define IGNITE_KEY_MULTI_KEY_COMPOSE      0x20000003u /**< Extended key Multi-key Compose */
#define IGNITE_KEY_LMETA                  0x20000004u /**< Extended key Left Meta */
#define IGNITE_KEY_RMETA                  0x20000005u /**< Extended key Right Meta */
#define IGNITE_KEY_LHYPER                 0x20000006u /**< Extended key Left Hyper */
#define IGNITE_KEY_RHYPER                 0x20000007u /**< Extended key Right Hyper */

#define IGNITE_KMOD_NONE   0x0000u /**< no modifier is applicable. */
#define IGNITE_KMOD_LSHIFT 0x0001u /**< the left Shift key is down. */
#define IGNITE_KMOD_RSHIFT 0x0002u /**< the right Shift key is down. */
#define IGNITE_KMOD_LEVEL5 0x0004u /**< the Level 5 Shift key is down. */
#define IGNITE_KMOD_LCTRL  0x0040u /**< the left Ctrl (Control) key is down. */
#define IGNITE_KMOD_RCTRL  0x0080u /**< the right Ctrl (Control) key is down. */
#define IGNITE_KMOD_LALT   0x0100u /**< the left Alt key is down. */
#define IGNITE_KMOD_RALT   0x0200u /**< the right Alt key is down. */
#define IGNITE_KMOD_LGUI   0x0400u /**< the left GUI key (often the Windows key) is down. */
#define IGNITE_KMOD_RGUI   0x0800u /**< the right GUI key (often the Windows key) is down. */
#define IGNITE_KMOD_NUM    0x1000u /**< the Num Lock key (may be located on an extended keypad) is down. */
#define IGNITE_KMOD_CAPS   0x2000u /**< the Caps Lock key is down. */
#define IGNITE_KMOD_MODE   0x4000u /**< the !AltGr key is down. */
#define IGNITE_KMOD_SCROLL 0x8000u /**< the Scroll Lock key is down. */
#define IGNITE_KMOD_CTRL   (IGNITE_KMOD_LCTRL | IGNITE_KMOD_RCTRL)   /**< Any Ctrl key is down. */
#define IGNITE_KMOD_SHIFT  (IGNITE_KMOD_LSHIFT | IGNITE_KMOD_RSHIFT) /**< Any Shift key is down. */
#define IGNITE_KMOD_ALT    (IGNITE_KMOD_LALT | IGNITE_KMOD_RALT)     /**< Any Alt key is down. */
#define IGNITE_KMOD_GUI    (IGNITE_KMOD_LGUI | IGNITE_KMOD_RGUI)     /**< Any GUI key is down. */

namespace ignite
{
    namespace Key {
        enum : KeyCode
        {
            Space = IGNITE_KEY_SPACE,
            Apostrophe = IGNITE_KEY_APOSTROPHE, /* ' */
            Comma = IGNITE_KEY_COMMA, /* , */
            Minus = IGNITE_KEY_MINUS, /* - */
            Period = IGNITE_KEY_PERIOD, /* . */
            Slash = IGNITE_KEY_SLASH, /* / */

            D0 = IGNITE_KEY_0, /* 0 */
            D1 = IGNITE_KEY_1, /* 1 */
            D2 = IGNITE_KEY_2, /* 2 */
            D3 = IGNITE_KEY_3, /* 3 */
            D4 = IGNITE_KEY_4, /* 4 */
            D5 = IGNITE_KEY_5, /* 5 */
            D6 = IGNITE_KEY_6, /* 6 */
            D7 = IGNITE_KEY_7, /* 7 */
            D8 = IGNITE_KEY_8, /* 8 */
            D9 = IGNITE_KEY_9, /* 9 */

            Semicolon = IGNITE_KEY_SEMICOLON, /* ; */
            Equal = IGNITE_KEY_EQUALS, /* = */

            A = IGNITE_KEY_A,
            B = IGNITE_KEY_B,
            C = IGNITE_KEY_C,
            D = IGNITE_KEY_D,
            E = IGNITE_KEY_E,
            F = IGNITE_KEY_F,
            G = IGNITE_KEY_G,
            H = IGNITE_KEY_H,
            I = IGNITE_KEY_I,
            J = IGNITE_KEY_J,
            K = IGNITE_KEY_K,
            L = IGNITE_KEY_L,
            M = IGNITE_KEY_M,
            N = IGNITE_KEY_N,
            O = IGNITE_KEY_O,
            P = IGNITE_KEY_P,
            Q = IGNITE_KEY_Q,
            R = IGNITE_KEY_R,
            S = IGNITE_KEY_S,
            T = IGNITE_KEY_T,
            U = IGNITE_KEY_U,
            V = IGNITE_KEY_V,
            W = IGNITE_KEY_W,
            X = IGNITE_KEY_X,
            Y = IGNITE_KEY_Y,
            Z = IGNITE_KEY_Z,

            LeftBracket = IGNITE_KEY_LEFTBRACKET,  /* [ */
            Backslash = IGNITE_KEY_BACKSLASH,  /* \ */
            RightBracket = IGNITE_KEY_RIGHTBRACKET,  /* ] */
            GraveAccent = IGNITE_KEY_GRAVE,  /* ` */

            /* Function keys */
            Escape = IGNITE_KEY_ESCAPE,
            Enter = IGNITE_KEY_RETURN,
            Tab = IGNITE_KEY_TAB,
            Backspace = IGNITE_KEY_BACKSPACE,
            Insert = IGNITE_KEY_INSERT,
            Delete = IGNITE_KEY_DELETE,
            Right = IGNITE_KEY_RIGHT,
            Left = IGNITE_KEY_LEFT,
            Down = IGNITE_KEY_DOWN,
            Up = IGNITE_KEY_UP,
            PageUp = IGNITE_KEY_PAGEUP,
            PageDown = IGNITE_KEY_PAGEDOWN,
            Home = IGNITE_KEY_HOME,
            End = IGNITE_KEY_END,
            CapsLock = IGNITE_KEY_CAPSLOCK,
            ScrollLock = IGNITE_KEY_SCROLLLOCK,
            NumLock = IGNITE_KEY_NUMLOCKCLEAR,
            PrintScreen = IGNITE_KEY_PRINTSCREEN,
            Pause = IGNITE_KEY_PAUSE,
            F1 = IGNITE_KEY_F1,
            F2 = IGNITE_KEY_F2,
            F3 = IGNITE_KEY_F3,
            F4 = IGNITE_KEY_F4,
            F5 = IGNITE_KEY_F5,
            F6 = IGNITE_KEY_F6,
            F7 = IGNITE_KEY_F7,
            F8 = IGNITE_KEY_F8,
            F9 = IGNITE_KEY_F9,
            F10 = IGNITE_KEY_F10,
            F11 = IGNITE_KEY_F11,
            F12 = IGNITE_KEY_F12,
            F13 = IGNITE_KEY_F13,
            F14 = IGNITE_KEY_F14,
            F15 = IGNITE_KEY_F15,
            F16 = IGNITE_KEY_F16,
            F17 = IGNITE_KEY_F17,
            F18 = IGNITE_KEY_F18,
            F19 = IGNITE_KEY_F19,
            F20 = IGNITE_KEY_F20,
            F21 = IGNITE_KEY_F21,
            F22 = IGNITE_KEY_F22,
            F23 = IGNITE_KEY_F23,
            F24 = IGNITE_KEY_F24,

            /* Keypad */
            KP0 = IGNITE_KEY_KP_0,
            KP1 = IGNITE_KEY_KP_1,
            KP2 = IGNITE_KEY_KP_2,
            KP3 = IGNITE_KEY_KP_3,
            KP4 = IGNITE_KEY_KP_4,
            KP5 = IGNITE_KEY_KP_5,
            KP6 = IGNITE_KEY_KP_6,
            KP7 = IGNITE_KEY_KP_7,
            KP8 = IGNITE_KEY_KP_8,
            KP9 = IGNITE_KEY_KP_9,
            KPDecimal = IGNITE_KEY_KP_DECIMAL,
            KPDivide = IGNITE_KEY_KP_DIVIDE,
            KPMultiply = IGNITE_KEY_KP_MULTIPLY,
            KPSubtract = IGNITE_KEY_KP_MEMSUBTRACT,
            KPAdd = IGNITE_KEY_KP_MEMADD,
            KPEnter = IGNITE_KEY_KP_ENTER,
            KPEqual = IGNITE_KEY_KP_EQUALS,
            Menu = IGNITE_KEY_MENU,
        };
        }

    namespace KeyMod {
    enum : KeyModCode {
        None = IGNITE_KMOD_NONE,
        Shift = IGNITE_KMOD_SHIFT,
        Control = IGNITE_KMOD_CTRL,
        LeftShift = IGNITE_KMOD_LSHIFT,
        LeftControl = IGNITE_KMOD_LCTRL,
        LeftAlt = IGNITE_KMOD_LALT,
        LeftSuper = IGNITE_KMOD_LGUI,
        RightShift = IGNITE_KMOD_RSHIFT,
        RightControl = IGNITE_KMOD_RCTRL,
        RightAlt = IGNITE_KMOD_RALT,
        RightSuper = IGNITE_KMOD_RGUI,
        Super = IGNITE_KMOD_GUI,
    };
    }
}
