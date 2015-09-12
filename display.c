#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <libc.h>
#include <stdlib.h>

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
	char 		nPlayer;
	char 		mPlayer;
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
			if (map->map[i][j] != shm->map.map[i][j])
				return (0);
			j++;
		}
		i++;
	}
	return (1);
}

void	print_map(t_env *shm)
{
	int i;
	int j;

	i = 0;
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			if (shm->map.map[i][j] != 0)
			{
				switch(shm->map.map[i][j])
				{
					case 1: printf(ANSI_COLOR_RED); break;
					case 2: printf(ANSI_COLOR_CYAN); break;
					default:  printf(ANSI_COLOR_YELLOW);
				}
				printf("o ");
				//printf("%c ", shm->map.map[i][j] + '0');
				printf(ANSI_COLOR_RESET);
			}
			else
				printf(". ");
			j++;
		}
		puts("");
		i++;
	}
	puts("");
	puts("");
}

int main(int argc, char **argv)
{
	t_env *shm;
	t_map map;
	int shm_id;

	while ((shm_id = shmget(CLEF, sizeof(t_env), 0200)) == -1);
	shm = shmat(shm_id, NULL, 0);
	map_copy(shm, &map);
	while (shm->nPlayer != shm->mPlayer);
	int i;
	i = 0;
	while (shm->nPlayer != 0)
	{
		if (!map_cmp(shm, &map))
		{
			system("clear");
			print_map(shm);
			map_copy(shm, &map);
		}
		i++;
	}
	shmdt(shm);
	return (0);
}