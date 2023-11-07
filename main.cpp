#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <ncurses.h>

void render_frame(int width, int height) {
    constexpr int INDEX_MAX = 9;
    // + 1 for '\0', +1 for zero-indexing
    constexpr char const index[INDEX_MAX + 2] = " .,:;izn&#";

    constexpr float FONT_ASPECT_RATIO = 2.5f / 5.0f;
    float dx = width / 2.0f * FONT_ASPECT_RATIO;
    float dy = height / 2.0f;
    float max_dist = std::sqrt(dx*dx + dy*dy);
    int y;
    for (y = 0 ; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
            dx = (x - (width/2.0f)) * FONT_ASPECT_RATIO;
            dy = y - (height/2.0f);

            float dist = std::sqrt(dx*dx + dy*dy);
            float dist_ratio = dist / max_dist;
            int i = std::min(static_cast<int>(std::roundf(dist_ratio * INDEX_MAX)), INDEX_MAX);
            mvaddch(y, x, index[i]);
        }
    }
}

int main() {
    initscr();
    // hide cursor
    curs_set(0);
    // disable typing
    noecho();
    // make getch non-blocking
    nodelay(stdscr, TRUE);

    int width, height;
    getmaxyx(stdscr, height, width);

    char const *exit_msg = "Q to Exit";

    printw(exit_msg);

    bool loop = true;
    while (loop) {
        if (is_term_resized(height, width)) {
            getmaxyx(stdscr, height, width);
        }

        int ch = getch();
        if (ch == ERR) {
            // ERR is returned in non-blocking mode -- not actually an error to be handled
        } else if (ch == 'q') {
            loop = false;
        }

        render_frame(width, height);

        move(0, 0);
        printw(exit_msg);
        printw(" (%dx%d)", width, height);

        refresh();

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(16ms);
    }

    endwin();
    return 0;
}