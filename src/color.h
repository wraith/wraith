#ifndef _COLOR_H
#define _COLOR_H

#define C_BLACK                 1
#define C_BLUE                  2
#define C_GREEN                 3
#define C_CYAN                  4
#define C_RED                   5
#define C_PURPLE                6
#define C_BROWN                 7
#define C_LIGHTGREY             8
#define C_DARKGREY              9
#define C_LIGHTBLUE             10
#define C_LIGHTGREEN            11
#define C_LIGHTCYAN             12
#define C_LIGHTRED              13
#define C_LIGHTPURPLE           14
#define C_YELLOW                15
#define C_WHITE                 16
#define BOLD_OPEN               1
#define BOLD_CLOSE              2
#define UNDERLINE_OPEN          3
#define UNDERLINE_CLOSE         4
#define COLOR_OPEN              5
#define COLOR_CLOSE             6
#define FLASH_OPEN              7
#define FLASH_CLOSE             8
#define BLACK(idx) 		color(idx, COLOR_OPEN, C_BLACK)
#define BLUE(idx) 		color(idx, COLOR_OPEN, C_BLUE)
#define GREEN(idx) 		color(idx, COLOR_OPEN, C_GREEN)
#define CYAN(idx) 		color(idx, COLOR_OPEN, C_CYAN)
#define RED(idx) 		color(idx, COLOR_OPEN, C_RED)
#define PURPLE(idx) 		color(idx, COLOR_OPEN, C_PURPLE)
#define BROWN(idx) 		color(idx, COLOR_OPEN, C_BROWN)
#define LIGHTGREY(idx) 		color(idx, COLOR_OPEN, C_LIGHTGREY)
#define DARKGREY(idx) 		color(idx, COLOR_OPEN, C_DARKGREY)
#define LIGHTBLUE(idx) 		color(idx, COLOR_OPEN, C_LIGHTBLUE)
#define LIGHTGREEN(idx)		color(idx, COLOR_OPEN, C_LIGHTGREEN)
#define LIGHTCYAN(idx) 		color(idx, COLOR_OPEN, C_LIGHTCYAN)
#define LIGHTRED(idx) 		color(idx, COLOR_OPEN, C_LIGHTRED)
#define LIGHTPURPLE(idx) 	color(idx, COLOR_OPEN, C_LIGHTPURPLE)
#define YELLOW(idx) 		color(idx, COLOR_OPEN, C_YELLOW)
#define WHITE(idx) 		color(idx, COLOR_OPEN, C_WHITE)
#define COLOR_END(idx) 		color(idx, COLOR_CLOSE, 0)
#define BOLD(idx) 		color(idx, BOLD_OPEN, 0)
#define BOLD_END(idx) 		color(idx, BOLD_CLOSE, 0)
#define UNDERLINE(idx) 		color(idx, UNDERLINE_OPEN, 0)
#define UNDERLINE_END(idx) 	color(idx, UNDERLINE_CLOSE, 0)
#define FLASH(idx) 		color(idx, FLASH_OPEN, 0)
#define FLASH_END(idx) 		color(idx, FLASH_CLOSE, 0)

#endif /* !_COLOR_H */
