#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <stdio.h>
#include <unistd.h>
#include <libc.h>
#include <stdlib.h>

#define CLEF 				666

#define WIDTH 				32
#define HEIGHT 				32

#define BUSY				8
#define TEAM 				7

#define LONGUEUR_SEGMENT 	512 

#define NTDELAY				250000

#define ABS(x)				(x < 0 ? -x : x)
#define POS(x, y)			((t_pos){x, y})
#define SUBPOS(p1, p2)		POS(p2.x - p1.x, p2.y - p1.y)
#define PLAYER(team, pos)	((t_player){team, 0, pos, NULL})
#define MSG(dest, player)			((t_msg){dest, player})

typedef struct 	s_pos
{
	int 		x;
	int 		y;
}				t_pos;

typedef struct 		s_player
{
	char			team;
	char 			state;
	t_pos			position;
	struct s_player	*target;
}					t_player;

typedef struct 	s_surrInfo
{
	char		sPlayers[9];
	t_pos		closest;
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
	int 		nPlayer;
	int 		mPlayer;
}				t_env;

typedef struct 	s_data
{
	int 			shm_id;
	int 			sem_id;
	struct sembuf 	sops;
	int 			que_id;
	t_env 			*shm;
	t_player		player;
}				t_data;

typedef struct 	s_msg
{
	long 		dest;
	t_player	target;
}				t_msg;

typedef struct 	s_ord
{
	t_pos		fAlly;
	t_pos		sAlly;
	t_pos		bAlly;
	t_pos		target;
}				t_ord;

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

void  init_env(t_env *shm, int mPlayer)
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
	exit(-1);
}

void	disconnect(t_env *shm, int sem_id, int shm_id, char *msg)
{
	shm->nPlayer--;
	
	if (shm->nPlayer == 0)
	{
		shmdt(shm);
		semctl(sem_id, 0, IPC_RMID);
		shmctl(shm_id, IPC_RMID, 0);
	}
	else
		shmdt(shm);
	exit(0);
}

t_player	init_player(t_env *shm, char team, int sem_id, struct sembuf *sops)
{
	if (team <= 0)
	{
		puts("Incorrect parameter: team number must be greater than zero.");
		unlock(sem_id, sops);
		exit(1);
	}
	if (shm->nPlayer + 1 > shm->mPlayer)
	{
		unlock(sem_id, sops);
		leave(shm, "");
	}
	shm->nPlayer++;
	return (place_player(shm, team));
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
				if (min_dist(player.position, POS(i, j)) == min_dist(player.position, closest))
					closest = (rand() % 2 ? closest : POS(i, j));
				else if (min_dist(player.position, POS(i, j)) < min_dist(player.position, closest))
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
	const t_pos directions[5] = {POS(1, 0), POS(-1, 0), POS(0, 1), POS(0, -1), POS(-1, -1)};
	t_pos 		dir;
	int 		i;

	i = 0;
	dir = directions[4];
	while (i < 4)
	{
		if (player->position.x + directions[i].x >= 0 && player->position.y + directions[i].y >= 0 &&
		 player->position.x + directions[i].x < shm->map.height && player->position.y + directions[i].y < shm->map.width &&
		 shm->map.map[player->position.x + directions[i].x][player->position.y + directions[i].y] == 0)
		{
			if (dir.x < 0 && dir.y < 0)
				dir = directions[i];
			else
				dir = (rand() % 2 ? dir : directions[i]);
		}
		i++;
	}
	if (dir.x < 0 && dir.y < 0)
		return;
	shm->map.map[player->position.x + dir.x][player->position.y + dir.y] = player->team;
	shm->map.map[player->position.x][player->position.y] = 0;
	player->position = POS(player->position.x + dir.x, player->position.y + dir.y);
}


void	get_closest_enemy(t_env *shm, t_surrInfo *sinfo, t_player player, t_pos index)
{
	int 	dist1;
	int 	dist2;

	dist1 = min_dist(player.position, index);
	dist2 = min_dist(player.position, sinfo->closest);
	if (dist1 < dist2)
		sinfo->closest = index;
	else if (dist1 == dist2)
		sinfo->closest = (rand() % 2 ? sinfo->closest : index);
}

t_surrInfo	scan_map(t_env *shm, t_player player)
{
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
			if (shm->map.map[i][j] > 0)
			{
				if ((shm->map.map[i][j] & TEAM) != player.team)
					get_closest_enemy(shm, &sinfo, player, POS(i, j));
				sinfo.sPlayers[(shm->map.map[i][j] & TEAM) - 1]++;
			}
			j++;
		}
		i++;
	}
	if (sinfo.closest.x == 255 && sinfo.closest.y == 255)
		sinfo.closest = POS(-1, -1);
	return (sinfo);
}

t_surrInfo	scan_suroundings(t_env *shm, t_player player, int radius)
{
	t_surrInfo	sinfo;
	t_pos		necell;
	int i;
	int j;

	sinfo.closest = POS(255, 255);
	memset(sinfo.sPlayers, 0, 9);
	i = player.position.x - radius;
	while (i <= player.position.x + radius)
	{
		j = player.position.y - radius;
		while (j <= player.position.y + radius)
		{
			if (i >= 0 && j >= 0 && i < shm->map.height && j < shm->map.width)
			{
				if (shm->map.map[i][j] > 0)
				{
					if ((shm->map.map[i][j] & TEAM) != player.team)
						get_closest_enemy(shm, &sinfo, player, POS(i, j));
					sinfo.sPlayers[(shm->map.map[i][j] & TEAM) - 1]++;
				}
			}
			j++;
		}
		i++;
	}
	if (sinfo.closest.x == 255 && sinfo.closest.y == 255)
		sinfo.closest = POS(-1, -1);
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
				sinfo.sPlayers[(shm->map.map[i][j] & TEAM) - 1]++;
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
		if (player.team != i + 1 && ((((nInfo.sPlayers[i] >= 2 && cInfo.sPlayers[i] == 1) && nInfo.sPlayers[player.team] == 1))))
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


/*
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
		shm->nPlayer--;
		shmdt(shm);
		unlock(sem_id, sops);
		exit(0);
	}
	nearInfo = scan_suroundings(shm, *player, 2);
	if (is_endangered(contactInfo, nearInfo, *player))
		flee(shm, nearInfo, player);
	else
	{
		farInfo = scan_suroundings(shm, *player, shm->map.width);
		if (farInfo.closest.x >= 0 && farInfo.closest.y >= 0 && farInfo.necell.x >= 0 && farInfo.necell.y >= 0)
		{
			hunt(shm, farInfo, player);
		}
	}
}
*/
int 	get_player_id(t_env *shm, t_pos player)
{
	return ((player.x * shm->map.height) + player.y);
}

int 	send_message(int que_id, t_msg *msg)
{
	int 	result;
	int 	length;

	length = sizeof(t_msg) - sizeof(long);
	if((result = msgsnd(que_id, msg, length, 0)) == -1)
	{
		return(-1);
	}
	return(result);
}

int 	read_message(int que_id, long dest, t_msg *msg)
{
	int 	result;
	int 	length;

	length = sizeof(t_msg) - sizeof(long);
	if((result = msgrcv(que_id, msg, length, dest, IPC_NOWAIT)) == -1)
	{
		return(-1);
	}
	return(result);
}


t_ord 	update_closest_allies(t_ord order, t_pos pos)
{
	order.sAlly = order.fAlly;
	order.fAlly = pos;
	return (order);
}

void 	get_closest_available_allies(t_env *shm, t_ord *order, t_pos index, t_pos enemy)
{
	int dist1;
	int dist2;
	int dist3;

	dist1 = min_dist(enemy, index);
	dist2 = min_dist(enemy, order->fAlly);
	dist3 = min_dist(enemy, order->sAlly);
	if (dist1 < dist2)
		*order = update_closest_allies(*order, index);
	else if (dist1 < dist3)
		order->sAlly = index;
	/*else if (dist1 == dist2)
	{
		if (dist2 == min_dist(enemy, order->sAlly))
		{
			if (rand() % 2)
				*order = update_closest_allies(*order, index);
		}
		else
		{
			*order = update_closest_allies(*order, index);
		}
	}*/
}

void	get_closest_unavailable_ally(t_env *shm, t_ord *order, t_pos index, t_player player)
{
	int dist1;
	int dist2;

	dist1 = min_dist(player.position, index);
	dist2 = min_dist(player.position, order->bAlly);
	if (dist1 < dist2)
		order->bAlly = index;
	else if (dist1 == dist2)
		order->bAlly = (rand() % 2 ? order->bAlly : index); 
}

t_ord	get_closest_allies(t_env *shm, t_player player, t_pos enemy)
{
	t_ord	order;
	int i;
	int j;

	order.target = enemy;
	order.fAlly = POS(255, 255);
	order.sAlly = POS(255, 255);
	order.bAlly = POS(255, 255);
	i = 0;
	while (i < shm->map.height)
	{
		j = 0;
		while (j < shm->map.width)
		{
			if ((shm->map.map[i][j] & TEAM) == player.team)
			{
				if(!(shm->map.map[i][j] & BUSY))
					get_closest_available_allies(shm, &order, POS(i, j), enemy);
				else
					get_closest_unavailable_ally(shm, &order, POS(i, j), player);
			}
			j++;
		}
		i++;
	}
	if (order.fAlly.x == 255 && order.fAlly.y == 255)
		order.fAlly = POS(-1, -1);
	if (order.sAlly.x == 255 && order.sAlly.y == 255)
		order.sAlly = POS(-1, -1);
	if (order.bAlly.x == 255 && order.bAlly.y == 255)
		order.bAlly = POS(-1, -1);
	return (order);
}

char	can_launch_attack(t_ord *order)
{
	return (order->fAlly.x >= 0 && order->fAlly.y >= 0 && order->sAlly.x >= 0 && order->sAlly.y >= 0);
}

t_pos	find_quickest_direction(t_env *shm, t_player player, t_pos target)
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
			current = min_dist(POS(player.position.x + directions[i].x, player.position.y + directions[i].y), target);
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

t_pos	relocate_target(t_map map, t_player player, t_player *target)
{
	int i;
	int j;

	if ((map.map[target->position.x][target->position.y] & TEAM) == target->team)
		return (target->position);
	i = target->position.x - 1;
	while (i <= target->position.x + 1)
	{
		j = target->position.y - 1;
		while (j <= target->position.y + 1)
		{
			if (i == target->position.x || j == target->position.y)
			{

				if ((map.map[i][j] & TEAM) == target->team)
					return (POS(i, j));
			}
			j++;
		}
		i++;
	}
	return (POS(-1, -1));
}

void	abandon_pursuit(t_data *data)
{
	//free(data->player.target);
	data->player.target = NULL;
	data->player.state = 0;
	data->shm->map.map[data->player.position.x][data->player.position.y] = data->player.team;
}

int		hunt(t_data *data)
{
	t_pos necell;
	t_pos direction;

	data->player.target->position = relocate_target(data->shm->map, data->player, data->player.target);
	if (data->player.target->position.x == -1)
	{
		abandon_pursuit(data);
		return (0);
	}
	necell = get_nearest_empty_cell(data->shm, data->player, data->player.target->position);
	direction = find_quickest_direction(data->shm, data->player, necell);
	if (direction.x == -1 && direction.y == -1)
		return (1);
	data->shm->map.map[data->player.position.x + direction.x][data->player.position.y + direction.y] = data->player.team | data->player.state;
	data->shm->map.map[data->player.position.x][data->player.position.y] = 0;
	data->player.position = POS(data->player.position.x + direction.x, data->player.position.y + direction.y);
	return (1);
}

void	rally(t_data *data, t_ord *order)
{
	t_pos direction;

	if (order->bAlly.x < 0)
	{
		random_move(data->shm, &(data->player));
		return;
	}
	direction = find_quickest_direction(data->shm, data->player, order->bAlly);
	if (direction.x == -1 && direction.y == -1)
		return ;
	data->shm->map.map[data->player.position.x + direction.x][data->player.position.y + direction.y] = data->player.team;
	data->shm->map.map[data->player.position.x][data->player.position.y] = 0;
	data->player.position = POS(data->player.position.x + direction.x, data->player.position.y + direction.y);
}

void	send_orders(t_data *data, t_ord *order)
{
	t_msg 	*msg;
	int 	target_team;
	char 	isfAlly;
	char 	issAlly;

	target_team = data->shm->map.map[order->target.x][order->target.y] & TEAM;
	if ((isfAlly = order->fAlly.x == data->player.position.x && order->fAlly.y == data->player.position.y))
	{
		data->player.state = BUSY;
		data->player.target = malloc(sizeof(t_player));
		*(data->player.target) = PLAYER(target_team, order->target);
		hunt(data);
	}
	else
	{
		msg = &(MSG(get_player_id(data->shm, order->fAlly) + 1, PLAYER(target_team, order->target)));
		send_message(data->que_id, msg);
		data->shm->map.map[order->fAlly.x][order->fAlly.y] |= BUSY;
	}
	if ((issAlly = order->sAlly.x == data->player.position.x && order->sAlly.y == data->player.position.y))
	{
		data->player.state = BUSY;
		data->player.target = malloc(sizeof(t_player));
		*(data->player.target) = PLAYER(target_team, order->target);
		hunt(data);
	}
	else
	{
		msg = &(MSG(get_player_id(data->shm, order->sAlly) + 1,  PLAYER(target_team, order->target)));
		send_message(data->que_id, msg);
		data->shm->map.map[order->sAlly.x][order->sAlly.y] |= BUSY;
	}
	if (!isfAlly && !issAlly)
	{
		data->player.state = BUSY;
		data->player.target = malloc(sizeof(t_player));
		*(data->player.target) = PLAYER(target_team, order->target);
		hunt(data);
	}
}

void	print_orders(t_data *data, t_ord *order)
{
	printf("player:%d%d 1st:%d/%d 2nd:%d/%d 3rd:%d/%d target:%d/%d\n", data->player.position.x, data->player.position.y, order->fAlly.x, order->fAlly.y, order->sAlly.x, order->sAlly.y, order->bAlly.x, order->bAlly.y, order->target.x, order->target.y);

}

void	make_decision(t_data *data)
{
	t_surrInfo	contactInfo;
	t_surrInfo	nearInfo;
	t_surrInfo 	mapInfo;
	t_ord		order;
	t_msg 		read;

	read = MSG(-1, PLAYER(-1, POS(-1, -1)));
	read_message(data->que_id, get_player_id(data->shm, data->player.position) + 1, &read);
	contactInfo = scan_suroundings(data->shm, data->player, 1);
	if (is_dead(contactInfo, data->player))
	{
		data->shm->map.map[data->player.position.x][data->player.position.y] = 0;
		data->shm->nPlayer--;
		shmdt(data->shm);
		unlock(data->sem_id, &(data->sops));
		exit(0);
	}
	nearInfo = scan_suroundings(data->shm, data->player, 2);
	if (is_endangered(contactInfo, nearInfo, data->player))
		flee(data->shm, nearInfo, &(data->player));
	else
	{
		mapInfo = scan_map(data->shm, data->player);
		if (data->player.state != BUSY)
		{
			if (!(data->shm->map.map[data->player.position.x][data->player.position.y] & BUSY))
			{
				if (mapInfo.closest.x >= 0 && mapInfo.closest.y >= 0)
				{
					order = get_closest_allies(data->shm, data->player, mapInfo.closest);
					if (can_launch_attack(&order))
						send_orders(data, &order);
					else
						rally(data, &order);
				}
			}
			else
			{
				data->player.state = BUSY;
				data->player.target = malloc(sizeof(t_player));
				*(data->player.target) = read.target;
				if (!hunt(data))
				{
					order = get_closest_allies(data->shm, data->player, mapInfo.closest);
					rally(data, &order);
				}
			}
		}
		else
		{
			if (!hunt(data))
			{
				order = get_closest_allies(data->shm, data->player, mapInfo.closest);
				rally(data, &order);
			}
		}
	}
}

int main(int argc, char **argv)
{
	t_data 			*data;

	srand(time(NULL));
	data = malloc(sizeof(t_data));
	if ((data->shm_id = shmget(CLEF, sizeof(t_env), 0600)) == -1)
	{
		if (argc != 3)
		{
			puts("Usage: ./a.out [max_players] [team]");
			exit(1);
		}
		puts("Creating shared memory...");
		if ((data->shm_id = shmget(CLEF, sizeof(t_env), IPC_CREAT | IPC_EXCL | 0600)) == -1)
		{
    		puts("Unable to create shared memory");
			exit(1);
		}
		puts("Creating semaphores...");
		if ((data->sem_id = semget(CLEF, 1, IPC_CREAT | IPC_EXCL | 0600)) == -1)
		{
			puts("Unable to create semaphores");
			shmctl(data->shm_id, IPC_RMID, 0);
			exit(1);
		}
		puts("Creating message queue...");
		if ((data->que_id = msgget(CLEF, IPC_CREAT | 0600)) == -1)
		{
			puts("Unable to create message queue");
			shmctl(data->shm_id, IPC_RMID, 0);
			shmctl(data->sem_id, IPC_RMID, 0);
			exit(1);
		}
		semctl(data->sem_id, 0, SETVAL, 1);
		data->shm = shmat(data->shm_id, NULL, 0);
		puts("Initialising resources...");
		lock(data->sem_id, &(data->sops));
		init_env(data->shm, atoi(argv[1]));
		data->player = init_player(data->shm, atoi(argv[2]), data->sem_id, &(data->sops));
		unlock(data->sem_id, &(data->sops));
	}
	else
	{
		if (argc != 2)
		{
			puts("Usage: ./a.out [team]");
			exit(1);
		}
		if ((data->sem_id = semget(CLEF, 1, 0600)) == -1)
		{
			puts("Unable to access semaphores");
			exit(1);
		}
		if ((data->que_id = msgget(CLEF, 0600)) == -1)
		{
			puts("Unable to access message queue");
			exit(1);
		}
		data->shm = shmat(data->shm_id, NULL, 0);
		lock(data->sem_id, &(data->sops));
		data->player = init_player(data->shm, atoi(argv[1]), data->sem_id, &(data->sops));
		unlock(data->sem_id, &(data->sops));
	}
	while (data->shm->nPlayer != data->shm->mPlayer);
	sleep(1);
	int i = 0;
	while (!is_over(data->shm))
	{
		lock(data->sem_id, &(data->sops));
		make_decision(data);
		usleep((NTDELAY / data->shm->nPlayer));
		unlock(data->sem_id, &(data->sops));
		usleep(50);
		i++;
	}
	disconnect(data->shm, data->sem_id, data->shm_id, "");
	return (0);
}