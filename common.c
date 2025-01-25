#include "common.h"

static bool random_was_seeded = false;

bool should_terminate = false;

int random_range(int min, int max)
{
    if (!random_was_seeded)
    {
        srand(time(NULL));
        random_was_seeded = true;
    }

    int random_number = rand();
    random_number = (random_number % (max - min + 1)) + min;
    return random_number;
}
