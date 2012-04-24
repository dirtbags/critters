#include <stdio.h>
#include <stdlib.h>

#include "forf.h"
#include "dump.h"

#define LENV_SIZE 100

#define INITIAL_CRITTERS 5

#define DSTACK_SIZE 200
#define CSTACK_SIZE 500
#define MEMORY_SIZE 10

enum {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

enum {
    EMPTY,
    WALL,
    US,
    THEM
};

enum {
    ACT_WAIT,
    ACT_HOP,
    ACT_LEFT,
    ACT_RIGHT,
    ACT_INFECT
};


struct genome {
    struct forf_env env;
    int error_pos;
    char color[8];
    char name[50];
    char *path;

    struct forf_stack  _prog;
    struct forf_value  _progvals[CSTACK_SIZE];
};

struct critter {
    int position[2];
    int direction;   
    int infections;
    int action;

    long memvals[MEMORY_SIZE];
    struct genome *genome;
};

/* Globals */
int rounds = 500;

int ngenomes;
struct genome *genomes;

int width;
int ncritters;
struct critter ***critters;



#define constant(name, val)                         \
    static void                                     \
    forf_critter_const_ ## name(struct forf_env *env)     \
    {                                               \
        forf_push_num(env, val);                    \
    }

constant(north, NORTH);
constant(east, EAST);
constant(south, SOUTH);
constant(west, WEST);
constant(empty, EMPTY);
constant(wall, WALL);
constant(us, US);
constant(them, THEM);

static void
forf_critter_look(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;
    long direction = forf_pop_num(env);
    int x = c->position[0];
    int y = c->position[1];

    switch (direction) {
        NORTH:
            y += 1;
            break;
        EAST:
            x += 1;
            break;
        SOUTH:
            y -= 1;
            break;
        WEST:
            x -= 1;
            break;
    }

    if ((x >= 0) && (y >= 0) &&
        (x < width) && (y < width)) {
        struct critter *o = critters[y][x];
    
        if (! o) {
            forf_push_num(env, EMPTY);
        } else if (o->genome == c->genome) {
            forf_push_num(env, US);
        } else {
            forf_push_num(env, THEM);
        }
    } else {
        forf_push_num(env, WALL);
    }
}

static void
forf_critter_get_direction(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;

    forf_push_num(env, (long)c->direction);
}

static void
forf_critter_get_infections(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;

    forf_push_num(env, (long)c->infections);
}

static void
forf_critter_set_action(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;
    long action = forf_pop_num(env);

    c->action = (int)action;
}

static void
forf_proc_random(struct forf_env *env)
{
    long max = forf_pop_num(env);

    forf_push_num(env, rand() % max);
}

struct forf_lexical_env critter_lenv_addons[] = {
    {"north", forf_critter_const_north},
    {"east", forf_critter_const_east},
    {"south", forf_critter_const_south},
    {"west", forf_critter_const_west},
    {"empty", forf_critter_const_empty},
    {"wall", forf_critter_const_wall},
    {"us", forf_critter_const_us},
    {"them", forf_critter_const_them},
    {"look", forf_critter_look},
    {"get-direction", forf_critter_get_direction},
    {"get-infections", forf_critter_get_infections},
    {"set-action!", forf_critter_set_action},
    {"random", forf_proc_random},
    {NULL, NULL}
};







struct forf_env env;

struct genome *
read_genome(char *path)
{
    FILE *f = fopen(path, "r");
    struct genome *g = NULL;

    if (! f) {
        return NULL;
    }
}

    
int
main(int argc, char *argv[])
{
    int i;

    struct forf_lexical_env lenv[LENV_SIZE];
    struct forf_stack  cmd;
    struct forf_value  cmdvals[CSTACK_SIZE];
    struct forf_stack  data;
    struct forf_value  datavals[DSTACK_SIZE];
    struct forf_memory mem;
    long               memvals[MEMORY_SIZE];

    lenv[0].name = NULL;
    lenv[0].proc = NULL;
    if ((! forf_extend_lexical_env(lenv, forf_base_lexical_env, LENV_SIZE)) ||
        (! forf_extend_lexical_env(lenv, critter_lenv_addons, LENV_SIZE))) {
        fprintf(stderr, "Unable to initialize lexical environment.\n");
        return 1;
    }

    /* We only need slightly random numbers */
    {
        char *s    = getenv("SEED");
        int   seed = atoi(s?s:"");

        if (! seed) {
            seed = getpid();
        }

        srand(seed);
        fprintf(stdout, "// SEED=%d\n", seed);
    }

    /* Read programs */
    for (ngenomes = 0; ngenomes < argc; ngenomes += 1) {
        struct genome *g = read_genome(argv[ngenomes + 1]);
    }
    ncritters = ngenomes * INITIAL_CRITTERS;
    genomes = (struct genome *)malloc(sizeof (struct genome) * ngenomes);

    /* Compute size of the play area */
    for (width = 1; width * width < ncritters; width += 1);
    width *= 2;
    critters = (struct critter ***)malloc(sizeof (struct critter *) * width * width);

    /* Create and scatter critters */
    for (i = 0; i < ngenomes; i += 1) {
        int j;

        for (j = 0; j < INITIAL_CRITTERS; j += 1) {
            struct critter *c = (struct critter *)calloc(1, sizeof (struct critter));
            int x, y;

            do {
                x = rand() % width;
                y = rand() % width;
            } while (! critters[y][x]);

            c->position[0] = x;
            c->position[1] = y;
            c->direction = rand() % 4;
            c->infections = 0;
            c->action = ACT_WAIT;

            c->genome = &genomes[i];
        }
    }

    return 0;
}
