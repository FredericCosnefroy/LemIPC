#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <libc.h>
#include <stdlib.h>
#include <ncurses.h>
#include <locale.h>

#define CLEF 666

#define WIDTH 				32
#define HEIGHT 				32

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct 	s_map
{
	int 		height;
	int 		width;
	char		map[HEIGHT][WIDTH];
}				t_map;

typedef struct 	s_env
{
	t_map		map;
	int 		nPlayer;
	int 		mPlayer;
}				t_env;

void	map_copy(t_env *shm, t_map *map)
{
	int i;
	int j;

	i = 0;
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			map->map[i][j] = shm->map.map[i][j];
			j++;
		}
		i++;
	}
}

int		map_cmp(t_env *shm, t_map *map)
{
	int i;
	int j;

	i = 0;
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			if ((map->map[i][j] & 7) != (shm->map.map[i][j] & 7))
				return (0);
			j++;
		}
		i++;
	}
	return (1);
}

void	print_background(int color)
{
	int i;
	int j;

	i = 0;
	attron(COLOR_PAIR(color));
	while (i < LINES)
	{
		j = 0;
		while (j < COLS)
		{
			mvaddch(i, j, ' ');
			j++;
		}
		i++;
	}
	i = 0;
	while (i < COLS)
	{
		mvaddch(0, i, '^');
		mvaddch(6, i, 'v');
		i++;
	}
	i = 1;
	while (i < 6)
	{
		mvaddch(i, 0, '|');
		mvaddch(i, COLS - 1, '|');
		i++;
	}
	mvprintw(1, 2," _                   ___ ____   ____ ");	
	mvprintw(2, 2,"| |    ___ _ __ ___ |_ _|  _ \\ / ___|");    
	mvprintw(3, 2,"| |   / _ \\ '_ ` _ \\ | || |_) | |    "); 
	mvprintw(4, 2,"| |__|  __/ | | | | || ||  __/| |___ ");
	mvprintw(5, 2,"|_____\\___|_| |_| |_|___|_|    \\____|");


	attroff(COLOR_PAIR(color));

}

void	print_map(t_env *shm)
{
	int i;
	int j;

	i = 0;
	print_background(8);
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			attron(COLOR_PAIR(shm->map.map[i][j] & 7));
			if (shm->map.map[i][j] != 0)
				mvprintw(j + 8, ((COLS - (shm->map.width << 1)) >> 1) + (i << 1) , "%C", 9724);
			else
				mvaddch(j + 8, ((COLS - (shm->map.width << 1)) >> 1) + (i << 1) , ' ');
			mvaddch(j + 8, 1 + ((COLS - (shm->map.width << 1)) >> 1) + (i << 1), ' ');
			attroff(COLOR_PAIR(shm->map.map[i][j] & 7));
			j++;
		}
		i++;
	}
	refresh();
}



int main(int argc, char **argv)
{
	t_env *shm;
	t_map map;
	int shm_id;
	setlocale(LC_ALL, "en_US.UTF-8");
	initscr();

	start_color();
	init_color(11, 683, 628, 577);
	init_color(12, 300, 300, 300);
	init_pair(8, 12, 11);
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);
	curs_set(0);
	print_background(8);
	refresh();
	while ((shm_id = shmget(CLEF, sizeof(t_env), 0200)) == -1);
	shm = shmat(shm_id, NULL, 0);
	map_copy(shm, &map);
	while (shm->nPlayer < 1);
	int i;
	i = 0;
	while (shm->nPlayer != 0)
	{
		if (!map_cmp(shm, &map))
		{
			print_map(shm);
			map_copy(shm, &map);
		}
		i++;
	}
	shmdt(shm);
	getch();
	curs_set(1);
	clear();
	refresh();
	endwin();

	return (0);
}