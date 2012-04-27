#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "forf.h"
#include "cgi.h"
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

char *house[] = {"cluster.4f", "dancer.4f", "spread.4f", NULL};

struct genome {
    struct forf_env env;
    int error_pos;
    int ord;
    char color[8];
    char name[50];
    char *path;

    /* XXX: move this crap to one_round */
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
    int x, y;
    int direction;   
    int infections;
    int action;
    unsigned int color;

    long memvals[MEMORY_SIZE];
    struct genome *genome;
};

/* Globals */
int rounds = 500;

int ngenomes = 0;
struct genome *genomes;

int width;
int ncritters;
struct critter *critters;


struct forf_lexical_env lenv[LENV_SIZE];

void
dump_stack(struct forf_stack *s)
{
  printf("Stack at %p: ", s);
  forf_print_stack(stdout, s, lenv);
  printf("\n");
}


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

#define action(name, val) \
    static void \
    forf_critter_ ## name(struct forf_env *env) \
    { \
    ((struct critter *)env->udata)->action = val; \
    }

action(wait, ACT_WAIT);
action(hop, ACT_HOP);
action(right, ACT_RIGHT);
action(left, ACT_LEFT);
action(infect, ACT_INFECT);

static struct critter *
whats_at(int x, int y)
{
    int i;

    for (i = 0; i < ncritters; i += 1) {
        if (! critters[i].genome) {
            break;
        }
        if ((critters[i].x == x) && (critters[i].y == y)) {
            return &critters[i];
        }
    }
    return NULL;
}

static int
modify_coord(int *x, int *y, long direction)
{
    switch (direction) {
        case NORTH:
            *y += 1;
            break;
        case EAST:
            *x += 1;
            break;
        case SOUTH:
            *y -= 1;
            break;
        case WEST:
            *x -= 1;
            break;
    }

    if ((*x < 0) || (*x >= width) ||
        (*y < 0) || (*y >= width)) {
        return WALL;
    }

    return 0;
}

static void
forf_critter_look(struct forf_env *env)
{
    struct critter *c = (struct critter *)env->udata;
    long direction = forf_pop_num(env);
    int x = c->x;
    int y = c->y;
    int wall;
    long ret = EMPTY;

    wall = modify_coord(&x, &y, direction);

    if (wall) {
        ret = WALL;
    } else {
        struct critter *o = whats_at(x, y);
    
        if (! o) {
            ret = EMPTY;
        } else if (o->genome == c->genome) {
            ret = US;
        } else {
            ret = THEM;
        }
    }
    forf_push_num(env, ret);
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
forf_proc_random(struct forf_env *env)
{
    long max = forf_pop_num(env);

    forf_push_num(env, rand() % max);
}

static void
forf_proc_dump(struct forf_env *env)
{
    dump_stack(env->data);
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
    {"wait!", forf_critter_wait},
    {"hop!", forf_critter_hop},
    {"right!", forf_critter_right},
    {"left!", forf_critter_left},
    {"infect!", forf_critter_infect},
    {"look", forf_critter_look},
    {"get-direction", forf_critter_get_direction},
    {"get-infections", forf_critter_get_infections},
    {"random", forf_proc_random},
    {NULL, NULL},
    {"dump", forf_proc_dump},
};


void
dump_arena()
{
    int x;
    int y;

    putchar(' ');
    for (x = 0; x < width; x += 1) {
        putchar('-');
    }
    putchar(' ');
    putchar('\n');
    for (y = width - 1; y >= 0; y -= 1) {
        putchar('|');
        for (x = 0; x < width; x += 1) {
            struct critter *c = whats_at(x, y);

            if (c) {
                printf("\033[4%dm", c->genome->ord + 1);
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
        putchar('|');
        putchar('\n');
    }
}



int
read_genome(struct genome *g, char *path)
{
    FILE *f = fopen(path, "r");

    if (! f) {
        return -1;
    }

    forf_stack_init(&g->prog, g->progvals, CSTACK_SIZE);
    forf_stack_init(&g->cmd, g->cmdvals, CSTACK_SIZE);
    forf_stack_init(&g->data, g->datavals, DSTACK_SIZE);
    forf_memory_init(&g->mem, g->memvals, MEMORY_SIZE);
    forf_env_init(&g->env, lenv, &g->data, &g->cmd, &g->mem, NULL);

    g->error_pos = forf_parse_file(&g->env, f);
    fclose(f);
    if (g->error_pos) {
        return -1;
    }
    g->ord = ngenomes;

    forf_stack_copy(&g->prog, &g->cmd);

    return 0;
}

int
read_genome_str(struct genome *g, char *str)
{
    forf_stack_init(&g->prog, g->progvals, CSTACK_SIZE);
    forf_stack_init(&g->cmd, g->cmdvals, CSTACK_SIZE);
    forf_stack_init(&g->data, g->datavals, DSTACK_SIZE);
    forf_memory_init(&g->mem, g->memvals, MEMORY_SIZE);
    forf_env_init(&g->env, lenv, &g->data, &g->cmd, &g->mem, NULL);

    g->error_pos = forf_parse_string(&g->env, str);
    if (g->error_pos) {
        return -1;
    }

    g->ord = ngenomes;

    forf_stack_copy(&g->prog, &g->cmd);

    return 0;
}

static int
cmpcritter(const void *a, const void *b)
{
    struct critter *ca = (struct critter *)a;
    struct critter *cb = (struct critter *)b;

    return ca->infections - cb->infections;
}

struct genome *
one_round(int round)
{
    int i;
    static struct critter **order = NULL;
    struct genome *winner;

    if (! order) {
        order = (struct critter **)calloc(ncritters, sizeof (struct critter *));
        for (i = 0; i < ncritters; i += 1) {
            order[i] = &critters[i];
        }
    }

    winner = order[0]->genome;

    /* Shuffle order */
    for (i = 0; i < ncritters; i += 1) {
        int j = rand() % ncritters;
        struct critter *c;

        c = order[i];
        order[i] = order[j];
        order[j] = c;
    }

    /* Now sort by infections */
    qsort(order, ncritters, sizeof (struct critter *), cmpcritter);

    /* Run programs */
    for (i = 0; i < ncritters; i += 1) {
        struct critter *c = order[i];
        struct genome *g = c->genome;
        int ret;

        if (g != winner) {
            winner = NULL;
        }

        g->env.udata = c;
        forf_stack_copy(&g->cmd, &g->prog);
        forf_stack_reset(&g->data);
        c->action = ACT_WAIT;
        ret = forf_eval(&g->env);
        if (! ret) {
            // fprintf(stderr, "Error in %p: %s\n", c, forf_error_str[g->env.error]);
            continue;
        }
    }

    /* We can stop if there's only one genome */
    if (winner) {
        return winner;
    }

    /* Now apply moves */
    for (i = 0; i < ncritters; i += 1) {
        struct critter *c = order[i];

        switch (c->action) {
            case ACT_RIGHT:
                c->direction = (c->direction + 1) % 4;
                break;
            case ACT_LEFT:
                c->direction = (c->direction + 3) % 4;
                break;
            case ACT_HOP:
                {
                    int x1 = c->x;
                    int y1 = c->y;
                    int wall = modify_coord(&x1, &y1, c->direction);

                    if ((! wall) && (! whats_at(x1, y1))) {
                        c->x = x1;
                        c->y = y1;
                    }
                }
                break;
            case ACT_INFECT:
                {
                    int x1 = c->x;
                    int y1 = c->y;
                    int wall = modify_coord(&x1, &y1, c->direction);
                    struct critter *o;

                    if ((! wall) && (o = whats_at(x1, y1))) {
                        o->genome = c->genome;
                    }
                    c->infections += 1;

                    for (x1 = 0; x1 < c->genome->env.memory->size; x1 += 1) {
                        c->genome->env.memory->mem[x1] = 0;
                    }
                }
                break;
        }
    }

    return winner;
}

char *colors[] = {"4444ff", "00ff00", "008888", "ff0000", "880088", "888800", "888888", NULL};

void
print_header()
{
    int i;

    printf("[%d,[", width);
    for (i = 0; i < ngenomes; i += 1) {
        if (i > 0) {
            putchar(',');
        }
        printf("\"#%s\"", colors[i]);
    }
    printf("],[");
}

void
print_critters(int round)
{
    int i;

    if (round > 0) {
        putchar(',');
    }
    printf("\n    [");
    for (i = 0; i < ncritters; i += 1) {
        struct critter *c = &critters[i];

        if (i > 0) {
            putchar(',');
        }
        printf("[%d,%d,%d,%d]", 
               c->genome->ord, c->direction, c->x, c->y);
    }
    printf("]");
}

void
print_footer()
{
    printf("\n]]\n");
}

int
main(int argc, char *argv[])
{
    int i;
    char *web = strstr(argv[0], ".cgi");

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
    }

    if (web) {
        /* Run as a CGI */
        char   program[8192];

        if (cgi_init(argv)) {
            return 0;
        }
        cgi_header("application/json");

        /* Count house genomes */
        for (i = 0; house[i]; i += 1);
        genomes = (struct genome *)calloc(i + 1, sizeof (struct genome));

        /* Read them in */
        for (i = 0; house[i]; i += 1) {
            if (! read_genome(&genomes[ngenomes], house[i])) {
                ngenomes += 1;
            }
        }

        program[0] = 0;
        while (1) {
            size_t len;
            char   key[20];

            len = cgi_item(key, sizeof key);
            if (0 == len) break;
            switch (key[0]) {
                case 'p':
                    cgi_item(program, sizeof(program));
                    break;
            }
        }

        if (! read_genome_str(&genomes[ngenomes], program)) {
            ngenomes += 1;
        }
    } else {
        /* Run as CLI */

        /* Read programs */
        genomes = (struct genome *)calloc(argc - 1, sizeof (struct genome));
        for (i = 1; i < argc; i += 1) {
            if (! read_genome(&genomes[ngenomes], argv[i])) {
                ngenomes += 1;
            }
        }
    }

    ncritters = ngenomes * INITIAL_CRITTERS;

    /* Compute size of the play area */
    for (width = 1; width * width < ncritters; width += 1);
    width *= 2;

    /* Dimension critters */
    critters = (struct critter *)calloc(ncritters, sizeof (struct critter));

    /* Create and scatter critters */
    for (i = 0; i < ngenomes; i += 1) {
        int j;

        for (j = 0; j < INITIAL_CRITTERS; j += 1) {
            struct critter *c = &critters[i * INITIAL_CRITTERS + j];
            int x, y;

            do {
                x = rand() % width;
                y = rand() % width;
            } while (whats_at(x, y));

            c->x = x;
            c->y = y;
            c->direction = rand() % 4;
            c->infections = 0;
            c->genome = &genomes[i];
        }
    }

    if (web) {
        print_header();
    }

    for (i = 0; i < rounds; i += 1) {
        struct genome *g;

        if (web) {
            print_critters(i);
        } else {
            dump_arena();
            usleep(300000);
        }

        g = one_round(i);
        if (g) {
            break;
        }
    }

    if (web) {
        print_footer();
    }

    return 0;
}
