#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <libc.h>
#include <stdlib.h>

#define CLEF 				666

#define WIDTH 				32
#define HEIGHT 				32

#define LONGUEUR_SEGMENT 	512 

#define NTDELAY				500000

#define ABS(x)				(x < 0 ? -x : x)
#define POS(x, y)			((t_pos){x, y})
#define SUBPOS(p1, p2)		POS(p2.x - p1.x, p2.y - p1.y)
#define PLAYER(team, pos)	((t_player){team, pos})

typedef struct 	s_pos
{
	int 		x;
	int 		y;
}				t_pos;

typedef struct 	s_player
{
	char		team;
	t_pos		position;
}				t_player;

typedef struct 	s_surrInfo
{
	char		sPlayers[9];
	t_pos		closest;
	t_pos		necell;
}				t_surrInfo;

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

int	normalize(int value)
{
	if (value == 0)
		return (0);
	return (value > 0 ? 1 : -1);
}

int		min_dist(t_pos origin, t_pos destination)
{
	t_pos	difference;

	difference = SUBPOS(origin, destination);
	return (ABS(difference.x) + ABS(difference.y));
}

void init_map(t_map *map)
{
	int i;
	int j;

	i = 0;
	map->height = HEIGHT;
	map->width = WIDTH;
	while (i < map->height)
	{
		j = 0;
		while (j < map->width)
		{
			map->map[i][j] = 0;
			j++;
		}
		i++;
	}
}

void  init_env(t_env *shm, char mPlayer)
{
	t_env 	env;

	if (mPlayer <= 2)
	{
		puts("Incorrect parameter: max players must be greater than 2");
		exit(1);
	}
	srand(time(NULL));
	init_map(&(env.map));
	env.nPlayer = 0;
	env.mPlayer = mPlayer;
	if ((int)shm != 1)
		shm = memcpy(shm, &env, sizeof(t_env));
	else { puts("Impossible d'attacher"); exit(1); }
}

t_player	place_player(t_env *shm, char team)
{
	int randX;
	int randY;
	char placeable;

	placeable = 0;
	while (!placeable)
	{
		randX = rand() % shm->map.height;
		randY = rand() % shm->map.width;
		if (shm->map.map[randX][randY] == 0)
			placeable = 1;
	}
	shm->map.map[randX][randY] = team;
	return (PLAYER(team, POS(randX, randY)));
}

void	leave(t_env *shm, char *msg)
{
	shmdt(shm);
	puts(msg);
	exit(-1);
}

void	disconnect(t_env *shm, int sem_id, int shm_id, char *msg)
{
	puts(msg);
	shm->nPlayer--;
	
	if (shm->nPlayer == 0)
	{
		puts("Deleting shared ressources...");
		shmdt(shm);
		semctl(sem_id, 0, IPC_RMID);
		shmctl(shm_id, IPC_RMID, 0);
	}
	else
		shmdt(shm);
	exit(0);
}

t_player	init_player(t_env *shm, char team)
{
	if (team <= 0)
	{
		puts("Incorrect parameter: team number must be greater than zero.");
		exit(1);
	}
	if (shm->nPlayer + 1 > shm->mPlayer)
		leave(shm, "Player limit reached");
	shm->nPlayer++;
	return (place_player(shm, team));
}

void	lock(int sem_id, struct sembuf *sops)
{
	sops->sem_num = 0; 
	sops->sem_op = -1;
	semop(sem_id, sops, 1);
}

void	unlock(int sem_id, struct sembuf *sops)
{
	sops->sem_num = 0;
	sops->sem_op = 1;
	semop(sem_id, sops, 1);
}

t_pos	get_nearest_empty_cell(t_env *shm, t_player player, t_pos target)
{
	t_pos closest;
	int i;
	int j;

	i = target.x - 1;
	closest = POS(255, 255);
	while (i <= target.x + 1)
	{
		j = target.y - 1;
		while (j <= target.y + 1)
		{
			if (i >= 0 && j >= 0 && i < shm->map.height && j < shm->map.width && shm->map.map[i][j] == 0 && !(i == target.x && j == target.y))
			{
				if (min_dist(player.position, POS(i, j)) < min_dist(player.position, closest))
					closest = POS(i, j);
			}
			j++;
		}
		i++;
	}
	if (closest.x == 255 && closest.y == 255)
		closest = POS(-1, -1);
	return (closest);
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
				printf("%c ", shm->map.map[i][j] + '0');
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

void	random_move(t_env *shm, t_player *player)
{
	int newX;
	int newY;
	int placeable;

	placeable = 0;
	while (!placeable)
	{
		if (rand() % 2)
		{
			newX = player->position.x + (rand() % 2 ? -1 : 1);
			newY = player->position.y;
		}
		else
		{
			newX = player->position.x;
			newY = player->position.y + (rand() % 2 ? -1 : 1);
		}
		if (newX >= 0 && newY >= 0 && newX < shm->map.height && newY < shm->map.width &&
			shm->map.map[newX][newY] == 0)
			placeable = 1;
	}
	shm->map.map[newX][newY] = player->team;
	shm->map.map[player->position.x][player->position.y] = 0;
	player->position = POS(newX, newY);
}

t_surrInfo	scan_suroundings(t_env *shm, t_player player, int radius)
{
	t_surrInfo	sinfo;
	t_pos		necell;
	int i;
	int j;

	sinfo.closest = POS(255, 255);
	sinfo.necell = POS(255, 255);
	memset(sinfo.sPlayers, 0, 9);
	i = player.position.x - radius;
	while (i <= player.position.x + radius)
	{
		j = player.position.y - radius;
		while (j <= player.position.y + radius)
		{
			if (i >= 0 && j >= 0 && i < shm->map.height && j < shm->map.width && !(i == player.position.x && j == player.position.y))
			{
				if (shm->map.map[i][j] > 0)
				{
					if (shm->map.map[i][j] != player.team)
					{
						necell = get_nearest_empty_cell(shm, player, POS(i, j));
						if (necell.x >= 0 && necell.y >= 0)
						{
							if (min_dist(player.position, necell) < min_dist(player.position, sinfo.necell))
								sinfo.necell = necell;
						}
						if (min_dist(player.position, POS(i, j)) < min_dist(player.position, sinfo.closest))
							sinfo.closest = POS(i, j);
					} 
					sinfo.sPlayers[shm->map.map[i][j] - 1]++;
				}
			}
			j++;
		}
		i++;
	}
	if (sinfo.closest.x == 255 && sinfo.closest.y == 255)
		sinfo.closest = POS(-1, -1);
	if (sinfo.necell.x == 255 && sinfo.necell.y == 255)
		sinfo.necell = POS(-1, -1);
	return (sinfo);
}

int one_team_remains(t_surrInfo sinfo)
{
	int team_counter;
	int i;

	i = 0;
	team_counter = 0;
	while (i < 9)
	{
		if (sinfo.sPlayers[i] > 0)
			team_counter++;
		i++;
	}
	if (team_counter > 1)
		return (0);
	return (1);
}

t_surrInfo	scan_map(t_env *shm, t_player player)
{
	t_pos		necell;
	t_surrInfo	sinfo;
	int i;
	int j;

	i = 0;
	sinfo.closest = POS(255, 255);
	memset(sinfo.sPlayers, 0, 9);
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			if (!(i == player.position.x && j == player.position.y))
			{
				if (shm->map.map[i][j] > 0)
				{
					if (shm->map.map[i][j] != player.team)
					{
						necell = get_nearest_empty_cell(shm, player, POS(i, j));
						if (min_dist(player.position, necell) < min_dist(player.position, sinfo.closest))
							sinfo.closest = necell;
					} 
					sinfo.sPlayers[shm->map.map[i][j] - 1]++;
				}
		//		shm->map.map[i][j] = 3;
			}
			j++;
		}
		i++;
	}
	if (sinfo.closest.x == 255 && sinfo.closest.y == 255)
		sinfo.closest = POS(-1, -1);
	return (sinfo);
}

int		is_over(t_env *shm)
{
	t_surrInfo	sinfo;
	int i;
	int j;

	i = 0;
	memset(sinfo.sPlayers, 0, 9);
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			if (shm->map.map[i][j] > 0)
				sinfo.sPlayers[shm->map.map[i][j] - 1]++;
			j++;
		}
		i++;
	}
	return (one_team_remains(sinfo));
}

void	print_surroundings(t_surrInfo sinfo, t_player player)
{
	int i;

	i = 0;
	printf("Team %d, {%d, %d}\n", player.team, player.position.x, player.position.y);
	while (i < 9){
		printf("%d ", sinfo.sPlayers[i]);
		i++;
	}
	printf("\nClosest:{%d, %d}\n", sinfo.closest.x, sinfo.closest.y);
}

char	is_dead(t_surrInfo sinfo, t_player player)
{
	int i;

	i = 0;
	while (i < 9)
	{
		if (player.team != i + 1 && sinfo.sPlayers[i] >= 2)
			return (1);
		i++;
	}
	return (0);
}

char	is_endangered(t_surrInfo cInfo, t_surrInfo nInfo, t_player player)
{
	int i;

	i = 0;
	while (i < 9)
	{
		if (player.team != i + 1 && ((((nInfo.sPlayers[i] >= 2 && cInfo.sPlayers[i] == 1) && nInfo.sPlayers[player.team] == 0))))
			return (1);
		i++;
	}
	return (0);
}

void	flee(t_env *shm, t_surrInfo sinfo, t_player *player)
{
	t_pos	closest_direction;
	t_pos	direction;
	int 	forbiddenX;
	int 	forbiddenY;

	closest_direction = SUBPOS(player->position, sinfo.closest);
	forbiddenX = normalize(closest_direction.x);
	forbiddenY = normalize(closest_direction.y);
	if (forbiddenX == 0)
	{
		if (rand() % 2)
			direction = POS(rand() % 2 ? -1 : 1, 0);
		else
			direction = POS(0, -forbiddenY);
	}
	else if (forbiddenY == 0)
	{
		if (rand() % 2)
			direction = POS(0, rand() % 2 ? -1 : 1);
		else
			direction = POS(-forbiddenX, 0);
	}
	else
	{
		if (rand() % 2)
			direction = POS(-forbiddenX, 0);
		else
			direction = POS(0, -forbiddenY);
	}
	if (player->position.x + direction.x >= 0 && player->position.x + direction.x < shm->map.height &&
		player->position.y + direction.y >= 0 && player->position.y + direction.y < shm->map.width &&
		 shm->map.map[player->position.x + direction.x][player->position.y + direction.y] == 0)
	{
		shm->map.map[player->position.x + direction.x][player->position.y + direction.y] = player->team;
		shm->map.map[player->position.x][player->position.y] = 0;
		player->position = POS(player->position.x + direction.x, player->position.y + direction.y);
	}
	//printf("I flee!\n");
}

t_pos	find_quickest_direction(t_env *shm, t_surrInfo sinfo, t_player player)
{
	const t_pos directions[5] = {POS(1, 0), POS(-1, 0), POS(0, 1), POS(0, -1), POS(-1, -1)};
	int current;
	int minDist;
	int fastest;
	int i;

	i = 0;
	minDist = 100000;
	fastest = 4;
	while (i < 4)
	{
		if (player.position.x + directions[i].x >= 0 && player.position.y + directions[i].y >= 0 &&
		 player.position.x + directions[i].x < shm->map.height && player.position.y + directions[i].y < shm->map.width &&
		 shm->map.map[player.position.x + directions[i].x][player.position.y + directions[i].y] == 0)
		{

			current = min_dist(POS(player.position.x + directions[i].x, player.position.y + directions[i].y), sinfo.necell);
			if (current == minDist)
				fastest = (rand() % 2) ? fastest : i;
			else if (current < minDist)
			{
				fastest = i;
				minDist = current;
			}
		}
		i++;
	}
	return (directions[fastest]);
}

void	hunt(t_env *shm, t_surrInfo sinfo, t_player *player)
{
	t_pos direction;

	direction = find_quickest_direction(shm, sinfo, *player);
	if (direction.x == -1 && direction.y == -1)
	{
		puts("Huge fail\n");
		return;
	}
	shm->map.map[player->position.x + direction.x][player->position.y + direction.y] = player->team;
	shm->map.map[player->position.x][player->position.y] = 0;
	player->position = POS(player->position.x + direction.x, player->position.y + direction.y);
}

void	make_decision(t_env *shm, int sem_id, int shm_id, t_player *player, struct sembuf *sops)
{
	t_surrInfo 	contactInfo;
	t_surrInfo	nearInfo;
	t_surrInfo	farInfo;

	if (shm->map.map[player->position.x][player->position.y] != player->team)
		puts("\nATTENTION\nJ'ai rien a foutre la\n");
	contactInfo = scan_suroundings(shm, *player, 1);
	if (is_dead(contactInfo, *player))
	{
		shm->map.map[player->position.x][player->position.y] = 0;
		unlock(sem_id, sops);
		disconnect(shm, sem_id, shm_id, "I died, bye bye");
	}
	nearInfo = scan_suroundings(shm, *player, 2);
	if (is_endangered(contactInfo, nearInfo, *player))
		flee(shm, nearInfo, player);
	else
	{
		//farInfo = scan_map(shm, *player);
		farInfo = scan_suroundings(shm, *player, shm->map.width);
		if (farInfo.closest.x >= 0 && farInfo.closest.y >= 0)
		{
		//	print_surroundings(farInfo, *player);
			hunt(shm, farInfo, player);
		}
		else
		{
			puts("Je me touche la nouille");
		}
	//	else
	//		random_move(shm, player);
	}
}



int main(int argc, char **argv)
{
	t_player		player;
	int 			pilot;
	int 			sem_id;
	int 			shm_id;
	struct sembuf 	sops;
	t_env 			*shm;

	if ((shm_id = shmget(CLEF, sizeof(t_env), 0600)) == -1)
	{
		if (argc != 3)
		{
			puts("Usage: ./a.out [max_players] [team]");
			exit(1);
		}
		puts("Creating shared memory...");
		if ((shm_id = shmget(CLEF, sizeof(t_env), IPC_CREAT | IPC_EXCL | 0666)) == -1)
		{
    		puts("Unable to create shared memory");
			exit(1);
		}
		puts("Creating semaphores...");
		if ((sem_id = semget(CLEF, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1)
		{
			puts("Unable to create semaphores");
			shmctl(shm_id, IPC_RMID, 0);
			exit(1);
		}
		semctl(sem_id, 0, SETVAL, 1);

		pilot = 1;
		shm = shmat(shm_id, NULL, 0);
		puts("Initialising resources...");
		lock(sem_id, &sops);
		init_env(shm, atoi(argv[1]));
		player = init_player(shm, atoi(argv[2]));
		unlock(sem_id, &sops);
	}
	else
	{
		if (argc != 2)
		{
			puts("Usage: ./a.out [team]");
			exit(1);
		}
		if ((sem_id = semget(CLEF, 1, 0600)) == -1)
		{
			puts("Unable to access semaphores");
			exit(1);
		}
		pilot = 0;
		shm = shmat(shm_id, NULL, 0);
		lock(sem_id, &sops);
		player = init_player(shm, atoi(argv[1]));
		unlock(sem_id, &sops);
	}
	system("clear");
	printf("Player [%d/%d] joined team %d\n", shm->nPlayer, shm->mPlayer, player.team);
	while (shm->nPlayer != shm->mPlayer);
	int i = 0;
	while (!is_over(shm))
	{
		lock(sem_id, &sops);
	//	print_surroundings(scan_suroundings(shm, player, 20), player);
	//	random_move(shm, &player);
		make_decision(shm, sem_id, shm_id, &player, &sops);
		usleep((NTDELAY / shm->nPlayer));
		unlock(sem_id, &sops);
		usleep((NTDELAY / shm->nPlayer));
		i++;
	}
	disconnect(shm, sem_id, shm_id, "I died, bye bye");
	return (0);
}