#include "presentation.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {
struct Theme {
    Color player{91, 203, 238, 255};
    Color opponent{239, 118, 154, 255};
    std::array<Color, 10> cells{};
    std::array<bool, 10> hasCell{};
    int periodMs = 2400;
};
Theme theme;
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" :
        s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool integer(const std::string& s, int& value) {
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} && result.ptr == s.data() + s.size();
}
bool color(std::string text, Color& out) {
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream in(text);
    int r, g, b, a;
    if (!(in >> r >> g >> b >> a)) return false;
    in >> std::ws;
    if (!in.eof() || r < 0 || r > 255 || g < 0 || g > 255 ||
        b < 0 || b > 255 || a < 0 || a > 255) return false;
    out = {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
           static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
    return true;
}
}

void presentation_load(const char* path) {
    theme = Theme{};
    std::string font = "Font/NanumGothic.ttf";
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = line.substr(0, line.find('#'));
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        bool valid = false;
        if (key == "font") { font = value; valid = !value.empty(); }
        else if (key == "avatar.player") valid = color(value, theme.player);
        else if (key == "avatar.opponent") valid = color(value, theme.opponent);
        else if (key == "avatar.period_ms") {
            int period;
            valid = integer(value, period) && period >= 500 && period <= 10000;
            if (valid) theme.periodMs = period;
        } else if (key.compare(0, 5, "cell.") == 0) {
            int id;
            if (integer(key.substr(5), id) && id >= 0 && id < 10) {
                valid = color(value, theme.cells[id]);
                if (valid) theme.hasCell[id] = true;
            }
        }
        if (!valid) std::fprintf(stderr, "[theme] ignored invalid/unknown key: %s\n", key.c_str());
    }
    if (!renderer_load_font(font.c_str()) && font != "Font/NanumGothic.ttf")
        renderer_load_font("Font/NanumGothic.ttf");
}

std::vector<Color> presentation_palette(std::vector<Color> defaults) {
    for (size_t i = 0; i < defaults.size() && i < theme.cells.size(); ++i)
        if (theme.hasCell[i]) defaults[i] = theme.cells[i];
    return defaults;
}

void presentation_draw_avatar(ImageHandle image, int x, int y, int size,
                              bool opponent, double seconds, bool animate) {
    // The clock changes only border opacity, never bounds, hitboxes, or game ticks.
    Color accent = opponent ? theme.opponent : theme.player;
    const double wave = animate ? 0.5 + 0.5 * std::sin(
        seconds * 6283.185307179586 / theme.periodMs) : 1.0;
    accent.a = static_cast<unsigned char>(accent.a * (0.65 + 0.35 * wave));
    draw_rect(x, y, size, size, accent);
    draw_rect(x + 2, y + 2, size - 4, size - 4, {18, 22, 38, 255});
    int width = 0, height = 0;
    if (size <= 8 || !image_size(image, width, height) || width <= 0 || height <= 0) return;
    const double scale = double(size - 8) / std::max(width, height);
    const int w = std::max(1, int(width * scale));
    const int h = std::max(1, int(height * scale));
    draw_image(image, x + (size - w) / 2, y + (size - h) / 2, w, h);
}

void presentation_draw_portrait(ImageHandle image, int x, int y, int w, int h) {
    int iw=0, ih=0;
    if (w<=0 || h<=0 || !image_size(image,iw,ih) || iw<=0 || ih<=0) return;
    double scale=std::min(double(w)/iw,double(h)/ih);
    int dw=std::max(1,int(iw*scale)), dh=std::max(1,int(ih*scale));
    draw_image(image,x+(w-dw)/2,y+(h-dh)/2,dw,dh);
}
