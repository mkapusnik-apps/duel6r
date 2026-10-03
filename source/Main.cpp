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

#include <stdio.h>
#include <string>
#include <SDL2/SDL.h>
#include "Exception.h"
#include "Application.h"
#ifdef D6_MACOS_LOCAL
#include "platform/MacLocal.h"
#include <memory>
#include <stdexcept>
#endif

static void reportError(const std::string &err) {
    fprintf(stderr, "Error occured: %s\n", err.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err.c_str(), nullptr);
}

int main(int argc, char **argv) {
    try {
#ifdef D6_MACOS_LOCAL
        // SDL's bundle base path is Contents/Resources (also fixed in Info.plist).
        // Resolve it before Application's audio, configuration and resource loads.
        std::unique_ptr<char, decltype(&SDL_free)> resources(SDL_GetBasePath(), SDL_free);
        std::unique_ptr<char, decltype(&SDL_free)> support(
                SDL_GetPrefPath("", "Duel 6 Reloaded"), SDL_free);
        if (!resources || !support) throw std::runtime_error(SDL_GetError());
        Duel6::MacLocal::preparePaths(resources.get(), support.get());
#endif
        Duel6::Application app(argc, argv);
        app.run();
        return 0;
    }
    catch (const Duel6::Exception &e) {
        std::string msg = e.getMessage() + "\nAt: " + e.getFile() + ": " + std::to_string(e.getLine());
        reportError(msg);
    }
    catch (const std::exception &e) {
        reportError(e.what());
    }
    catch (...) {
        reportError("Unexpected error");
    }

    return 1;
}
