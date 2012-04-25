#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

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

    struct forf_stack  prog;
    struct forf_value  progvals[CSTACK_SIZE];
    struct forf_stack  cmd;
    struct forf_value  cmdvals[CSTACK_SIZE];
    struct forf_stack  data;
    struct forf_value  datavals[DSTACK_SIZE];
    struct forf_memory mem;
    long               memvals[MEMORY_SIZE];
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
constant(wait, ACT_WAIT);
constant(hop, ACT_HOP);
constant(right, ACT_RIGHT);
constant(left, ACT_LEFT);
constant(infect, ACT_INFECT);

static void
forf_critter_look(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;
    long direction = forf_pop_num(env);
    int x = c->position[0];
    int y = c->position[1];

    switch (direction) {
        case NORTH:
            y += 1;
            break;
        case EAST:
            x += 1;
            break;
        case SOUTH:
            y -= 1;
            break;
        case WEST:
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
    {"wait", forf_critter_const_wait},
    {"hop", forf_critter_const_hop},
    {"right", forf_critter_const_right},
    {"left", forf_critter_const_left},
    {"infect", forf_critter_const_infect},
    {"look", forf_critter_look},
    {"get-direction", forf_critter_get_direction},
    {"get-infections", forf_critter_get_infections},
    {"set-action!", forf_critter_set_action},
    {"random", forf_proc_random},
    {NULL, NULL}
};




void
dump_arena()
{
    int x;
    int y;

    for (y = width - 1; y >= 0; y -= 1) {
        for (x = 0; x < width; x += 1) {
            struct critter *c = critters[y][x];

            if (c) {
                printf("\033[4%dm", (((unsigned int)(c->genome) / 16) % 7) + 1);
                switch (c->direction) {
                    case NORTH:
                        putchar('^');
                        break;
                    case SOUTH:
                        putchar('v');
                        break;
                    case WEST:
                        putchar('<');
                        break;
                    case EAST:
                        putchar('>');
                        break;
                }
                printf("\033[0m");
            } else {
                putchar(' ');
            }
        }
        putchar('\n');
    }
}



int
read_genome(struct genome *g, struct forf_lexical_env *lenv, char *path)
{
    FILE *f = fopen(path, "r");

    if (! f) {
        return 0;
    }

    forf_stack_init(&g->prog, g->progvals, CSTACK_SIZE);
    forf_stack_init(&g->cmd, g->cmdvals, CSTACK_SIZE);
    forf_stack_init(&g->data, g->datavals, DSTACK_SIZE);
    forf_memory_init(&g->mem, g->memvals, MEMORY_SIZE);
    forf_env_init(&g->env, lenv, &g->data, &g->cmd, &g->mem, NULL);

    g->error_pos = forf_parse_file(&g->env, f);
    fclose(f);
    if (g->error_pos) {
        free(g);
        return 0;
    }

    forf_stack_copy(&g->prog, &g->cmd);

    return 1;
}

void
one_round()
{
    int x, y;

    for (y = 0; y < width; y += 1) {
        for (x = 0; x < width; x += 1) {
            struct critter *c = critters[y][x];
            struct genome *g;
            int ret;

            if (! c) {
                continue;
            }

            g = c->genome;

            forf_env_set_udata(&g->env, c);
            forf_stack_copy(&g->cmd, &g->prog);
            forf_stack_reset(&g->data);
            c->action = ACT_WAIT;
            ret = forf_eval(&g->env);
            if (! ret) {
                DUMP();
            }

            switch (c->action) {
                case ACT_RIGHT:
                    c->direction = (c->direction + 1) % 4;
                    break;
                case ACT_LEFT:
                    c->direction = (c->direction + 5) % 4;
                    break;
                case ACT_HOP:
                    DUMP();
                    break;
                case ACT_INFECT:
                    DUMP();
                    break;
            }
        }
    }
    dump_arena();
    sleep(1);
}

int
main(int argc, char *argv[])
{
    int i;

    struct forf_lexical_env lenv[LENV_SIZE];

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
    ngenomes = 0;
    genomes = (struct genome *)calloc(argc - 1, sizeof (struct genome));
    for (i = 1; i < argc; i += 1) {
        DUMP_p(genomes);
        DUMP_p(&genomes[ngenomes]);
        if (read_genome(&genomes[ngenomes], lenv, argv[i])) {
            forf_dump_stack(&(&genomes[ngenomes])->prog);
            ngenomes += 1;
        }
    }
    ncritters = ngenomes * INITIAL_CRITTERS;

    /* Compute size of the play area */
    for (width = 1; width * width < ncritters; width += 1);
    width *= 4;

    /* Dimension critters array */
    critters = (struct critter ***)calloc(width, sizeof (struct critter **));
    for (i = 0; i < width; i += 1) {
        critters[i] = (struct critter **)calloc(width, sizeof (struct critter *));
    }

    /* Create and scatter critters */
    for (i = 0; i < ngenomes; i += 1) {
        int j;

        for (j = 0; j < INITIAL_CRITTERS; j += 1) {
            struct critter *c = (struct critter *)calloc(1, sizeof (struct critter));
            int x, y;

            do {
                x = rand() % width;
                y = rand() % width;
            } while (critters[y][x]);

            c->position[0] = x;
            c->position[1] = y;
            c->direction = rand() % 4;
            c->infections = 0;
            c->action = ACT_WAIT;

            c->genome = &genomes[i];

            DUMP_d(i);
            forf_dump_stack(&c->genome->prog);

            critters[y][x] = c;
        }
    }

    for (i = 0; i < rounds; i += 1) {
        one_round();
    }

    return 0;
}
