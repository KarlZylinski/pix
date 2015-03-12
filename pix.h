#pragma once

void pix_init(const char* window_title, unsigned window_width, unsigned window_height);
void pix_deinit();
void pix_process_events();
void pix_put(unsigned x, unsigned y, char r, char g, char b);
void pix_clear();
void pix_flip();
int pix_is_window_open();
