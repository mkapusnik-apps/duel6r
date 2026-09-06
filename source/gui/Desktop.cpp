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

#include <SDL2/SDL.h>
#include "Desktop.h"
#include "../Video.h"

namespace Duel6 {
    namespace Gui {
        namespace {
            Color bcgColor(192, 192, 192);
        }

        Desktop::Desktop(Renderer &renderer)
                : renderer(renderer), scale(1.0f) {}

        Desktop::~Desktop() {
        }

        void Desktop::addControl(Control *control) {
            controls.push_back(std::unique_ptr<Control>(control));
        }

        void Desktop::screenSize(Int32 scrWidth, Int32 scrHeight, Int32 canvasWidth, Int32 canvasHeight,
                                 Int32 trX, Int32 trY, Float32 scale) {
            screenWidth = scrWidth;
            screenHeight = scrHeight;
            this->canvasWidth = canvasWidth;
            this->canvasHeight = canvasHeight;
            this->trX = trX;
            this->trY = trY;
            this->scale = scale;
        }

        void Desktop::update(Float32 elapsedTime) {
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->update(elapsedTime);
                }
            }
        }

        void Desktop::draw(const Font &font) const {
            renderer.setViewMatrix(Matrix::translate(Float32(trX), Float32(trY), 0) *
                                   Matrix::scale(scale, scale, 1.0f));
            renderer.quadXY(Vector(-2, -2), Vector(canvasWidth + 4, canvasHeight + 4), Color::BLACK);
            renderer.quadXY(Vector(0, 0), Vector(canvasWidth, canvasHeight), bcgColor);

            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->draw(renderer, font);
                }
            }

            renderer.setViewMatrix(Matrix::IDENTITY);
        }

        void Desktop::keyEvent(const KeyPressEvent &event) {
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->keyEvent(event);
                }
            }
        }

        void Desktop::textInputEvent(const TextInputEvent &event) {
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->textInputEvent(event);
                }
            }
        }

        void Desktop::mouseButtonEvent(const MouseButtonEvent &event) {
            MouseButtonEvent translatedEvent = event.inverseTransform(scale, trX, trY);
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->mouseButtonEvent(translatedEvent);
                }
            }
        }

        void Desktop::mouseMotionEvent(const MouseMotionEvent &event) {
            MouseMotionEvent translatedEvent = event.inverseTransform(scale, trX, trY);
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->mouseMotionEvent(translatedEvent);
                }
            }
        }

        void Desktop::mouseWheelEvent(const MouseWheelEvent &event) {
            MouseWheelEvent translatedEvent = event.inverseTransform(scale, trX, trY);
            for (auto &control : controls) {
                if (control->isVisible()) {
                    control->mouseWheelEvent(translatedEvent);
                }
            }
        }
    }
}
