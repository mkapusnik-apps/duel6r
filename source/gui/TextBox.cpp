/*
* Copyright (c) 2006, Ondrej Danek (www.ondrej-danek.net)
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Ondrej Danek nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <SDL2/SDL_keycode.h>
#include "TextBox.h"
#include "../Video.h"

namespace Duel6 {
    namespace Gui {
        Textbox *Textbox::focusedTextbox = nullptr;

        Textbox::Textbox(Desktop &desk)
                : Control(desk), focused(false), labelLeft(false) {
        }

        Textbox::~Textbox() {
            if (focusedTextbox == this) {
                focusedTextbox = nullptr;
            }
        }

        void Textbox::setPosition(int X, int Y, int W, int M, const std::string &allowed) {
            setLocation(X, Y);
            max = M;
            width = W;

            allowedCharacters = allowed;
            text.clear();
        }

        void Textbox::setLocation(int X, int Y) {
            x = X + 2;
            y = Y - 2;
        }

        void Textbox::setLabel(const std::string &label) {
            this->label = label;
        }

        void Textbox::setLabelLeft(bool labelLeft) {
            this->labelLeft = labelLeft;
        }

        void Textbox::keyEvent(const KeyPressEvent &event) {
            if (focused && !text.empty() && event.getCode() == SDLK_BACKSPACE) {
                text.pop_back();
            }
        }

        void Textbox::textInputEvent(const TextInputEvent &event) {
            if (!focused) {
                return;
            }

            const std::string &newText = event.getText();
            for (auto iter = newText.cbegin(); iter != newText.cend(); ++iter) {
                char letter = *iter;
                if ((int) text.length() < max && allowedCharacters.find(letter) != std::string::npos) {
                    text.push_back(letter);
                }
            }
        }

        void Textbox::mouseButtonEvent(const MouseButtonEvent &event) {
            if (event.getButton() != SysEvent::MouseButton::LEFT || !event.isPressed()) {
                return;
            }

            int w = (width << 3) + 8;
            bool requestedFocus = Control::mouseIn(event, x - 2, y + 2, w + 4, 22);
            if (requestedFocus && focusedTextbox != this) {
                if (focusedTextbox != nullptr) {
                    focusedTextbox->focused = false;
                }
                focusedTextbox = this;
            }

            setFocused(requestedFocus);
        }

        const std::string &Textbox::getText() const {
            return text;
        }

        void Textbox::setText(const std::string &text) {
            this->text = text.substr(0, max);
        }

        bool Textbox::isFocused() const {
            return focused;
        }

        void Textbox::setFocused(bool focused) {
            this->focused = focused;
            if (!focused && focusedTextbox == this) {
                focusedTextbox = nullptr;
            }
        }

        void Textbox::flush() {
            text.clear();
        }

        void Textbox::draw(Renderer &renderer, const Font &font) const {
            int w = (width << 3) + 8;

            drawFrame(renderer, x - 2, y + 2, w + 4, 22, true);
            renderer.quadXY(Vector(x, y - 17), Vector(w, 17), Color::WHITE);
            font.print(x, y - 16, Color(0), text + (focused ? "_" : ""));

            if (!label.empty()) {
                Int32 labelX = labelLeft ? x - Int32(label.length() * 8) - 12 : x + w + 12;
                font.print(labelX, y - 16, Color(0), label);
            }
        }
    }
}
