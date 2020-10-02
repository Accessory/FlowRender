#pragma once

#include <regex>

namespace FlowRenderConst {
    const std::regex MAIN_FIND(R"(\#\!([vlfrcie])\(([^\)]+)\))");
}