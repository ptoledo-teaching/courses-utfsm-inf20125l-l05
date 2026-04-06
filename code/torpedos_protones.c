#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 128
#define MAX_REASON 64
#define MAX_STEPS 1000
#define TRENCH_LIMIT 5.0f

static float absolute_value(float value)
{
    if (value < 0.0f)
    {
        return -value;
    }

    return value;
}

static int quantize_hundredths(float value)
{
    float scaled = value * 100.0f;

    if (scaled >= 0.0f)
    {
        return (int)(scaled + 0.5f);
    }

    return (int)(scaled - 0.5f);
}

static int read_line(char *buffer, size_t size)
{
    if (fgets(buffer, size, stdin) == NULL)
    {
        return 0;
    }

    return 1;
}

static int parse_unsigned_int(const char *line, unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed_value = 0;

    while (isspace((unsigned char)*line))
    {
        line++;
    }

    parsed_value = strtoul(line, &end, 10);
    if (line == end)
    {
        return 0;
    }

    while (isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
    {
        return 0;
    }

    *value = (unsigned int)parsed_value;
    return 1;
}

static int parse_float(const char *line, float *value)
{
    char *end = NULL;

    while (isspace((unsigned char)*line))
    {
        line++;
    }

    *value = strtof(line, &end);
    if (line == end)
    {
        return 0;
    }

    while (isspace((unsigned char)*end))
    {
        end++;
    }

    if (*end != '\0')
    {
        return 0;
    }

    return 1;
}

static int load_config(float *target_distance,
                       float *speed,
                       float *angle_deg,
                       float *initial_offset,
                       float *guidance,
                       float *tolerance)
{
    char line[MAX_LINE];

    if (!read_line(line, sizeof(line)) || !parse_float(line, target_distance))
    {
        return 0;
    }

    if (!read_line(line, sizeof(line)) || !parse_float(line, speed))
    {
        return 0;
    }

    if (!read_line(line, sizeof(line)) || !parse_float(line, angle_deg))
    {
        return 0;
    }

    if (!read_line(line, sizeof(line)) || !parse_float(line, initial_offset))
    {
        return 0;
    }

    if (!read_line(line, sizeof(line)) || !parse_float(line, guidance))
    {
        return 0;
    }

    if (!read_line(line, sizeof(line)) || !parse_float(line, tolerance))
    {
        return 0;
    }

    return 1;
}

static unsigned int get_seed(void)
{
    const char *seed_text = getenv("PROTON_TORPEDO_SEED");
    unsigned int seed = 0;
    time_t seconds = time(NULL);
    clock_t ticks = clock();

    if (seed_text != NULL && parse_unsigned_int(seed_text, &seed))
    {
        return seed;
    }

    return (unsigned int)seconds ^ (unsigned int)ticks;
}

static int config_is_valid(float target_distance,
                           float speed,
                           float guidance,
                           float tolerance)
{
    if (target_distance <= 0.0f)
    {
        return 0;
    }

    if (speed <= 0.0f)
    {
        return 0;
    }

    if (guidance < 0.0f)
    {
        return 0;
    }

    if (tolerance <= 0.0f)
    {
        return 0;
    }

    return 1;
}

static float calculate_initial_drift(float angle_deg)
{
    return angle_deg / 50.0f;
}

static float calculate_turbulence(void)
{
    int raw = (rand() % 21) - 10;
    return (float)raw / 100.0f;
}

static float calculate_correction(float offset, float guidance)
{
    if (offset > 0.0f)
    {
        return guidance;
    }

    if (offset < 0.0f)
    {
        return -guidance;
    }

    return 0.0f;
}

static void initialize_state(float initial_offset,
                             float angle_deg,
                             int *step,
                             float *distance,
                             float *offset,
                             float *drift_per_step,
                             float *stability,
                             int *active)
{
    assert(step != NULL);
    assert(distance != NULL);
    assert(offset != NULL);
    assert(drift_per_step != NULL);
    assert(stability != NULL);
    assert(active != NULL);

    *step = 0;
    *distance = 0.0f;
    *offset = initial_offset;
    *drift_per_step = calculate_initial_drift(angle_deg);
    *stability = 100.0f;
    *active = 1;
}

static void update_stability(float drift_per_step,
                             float turbulence,
                             float correction,
                             float *stability)
{
    float stability_cost = 0.0f;

    assert(stability != NULL);

    stability_cost += absolute_value(turbulence) * 35.0f;
    stability_cost += absolute_value(correction) * 8.0f;
    stability_cost += absolute_value(drift_per_step) * 3.0f;

    *stability -= stability_cost;
    if (*stability < 0.0f)
    {
        *stability = 0.0f;
    }
}

static void execute_step(float speed,
                         float guidance,
                         float tolerance,
                         int *step,
                         float *distance,
                         float *offset,
                         float drift_per_step,
                         float *stability,
                         int *active)
{
    float turbulence = 0.0f;
    float correction = 0.0f;

    assert(step != NULL);
    assert(distance != NULL);
    assert(offset != NULL);
    assert(stability != NULL);
    assert(active != NULL);
    assert(speed > 0.0f);
    assert(tolerance > 0.0f);

    turbulence = calculate_turbulence();
    correction = calculate_correction(*offset, guidance);

    *offset += drift_per_step;
    *offset += correction;
    *offset += turbulence;
    *distance += speed;
    *step += 1;

    update_stability(drift_per_step, turbulence, correction, stability);

    if (absolute_value(*offset) > TRENCH_LIMIT)
    {
        *active = 0;
    }
}

static void evaluate_result(float tolerance,
                            int active,
                            int step,
                            float distance,
                            float offset,
                            float stability,
                            int *success,
                            int *steps,
                            float *final_distance,
                            float *final_offset,
                            float *final_stability,
                            char reason[MAX_REASON])
{
    int offset_hundredths = 0;
    int tolerance_hundredths = 0;

    assert(success != NULL);
    assert(steps != NULL);
    assert(final_distance != NULL);
    assert(final_offset != NULL);
    assert(final_stability != NULL);
    assert(reason != NULL);

    *success = 0;
    *steps = step;
    *final_distance = distance;
    *final_offset = offset;
    *final_stability = stability;
    strcpy(reason, "excessive deviation");

    if (!active)
    {
        strcpy(reason, "torpedo outside trench");
        return;
    }

    if (stability < 40.0f)
    {
        strcpy(reason, "unstable detonator");
        return;
    }

    offset_hundredths = quantize_hundredths(absolute_value(offset));
    tolerance_hundredths = quantize_hundredths(tolerance);

    if (offset_hundredths < tolerance_hundredths)
    {
        *success = 1;
        strcpy(reason, "target hit");
        return;
    }
}

static void run_simulation(float target_distance,
                           float speed,
                           float angle_deg,
                           float initial_offset,
                           float guidance,
                           float tolerance,
                           int *success,
                           int *steps,
                           float *final_distance,
                           float *final_offset,
                           float *final_stability,
                           char reason[MAX_REASON])
{
    int step = 0;
    float distance = 0.0f;
    float offset = 0.0f;
    float drift_per_step = 0.0f;
    float stability = 0.0f;
    int active = 0;

    initialize_state(initial_offset,
                     angle_deg,
                     &step,
                     &distance,
                     &offset,
                     &drift_per_step,
                     &stability,
                     &active);

    while (active && distance < target_distance && step < MAX_STEPS)
    {
        execute_step(speed,
                     guidance,
                     tolerance,
                     &step,
                     &distance,
                     &offset,
                     drift_per_step,
                     &stability,
                     &active);
    }

    evaluate_result(tolerance,
                    active,
                    step,
                    distance,
                    offset,
                    stability,
                    success,
                    steps,
                    final_distance,
                    final_offset,
                    final_stability,
                    reason);
}

static void print_result(int success,
                         int steps,
                         float final_distance,
                         float final_offset,
                         float final_stability,
                         const char *reason)
{
    assert(reason != NULL);

    if (success)
    {
        printf("Mission        : SUCCESS\n");
    }
    else
    {
        printf("Mission        : FAILURE\n");
    }

    printf("Steps          : %d\n", steps);
    printf("Final distance : %.2f\n", final_distance);
    printf("Final offset   : %.2f\n", final_offset);
    printf("Final stability: %.2f\n", final_stability);
    printf("Reason         : %s\n", reason);
}

int main(void)
{
    float target_distance = 0.0f;
    float speed = 0.0f;
    float angle_deg = 0.0f;
    float initial_offset = 0.0f;
    float guidance = 0.0f;
    float tolerance = 0.0f;
    int success = 0;
    int steps = 0;
    float final_distance = 0.0f;
    float final_offset = 0.0f;
    float final_stability = 0.0f;
    char reason[MAX_REASON];
    unsigned int seed = 0;

    if (!load_config(&target_distance,
                     &speed,
                     &angle_deg,
                     &initial_offset,
                     &guidance,
                     &tolerance))
    {
        fprintf(stderr, "Error: could not read mission configuration.\n");
        return 1;
    }

    if (!config_is_valid(target_distance, speed, guidance, tolerance))
    {
        fprintf(stderr, "Error: mission configuration is not valid.\n");
        return 2;
    }

    seed = get_seed();
    srand(seed);

    run_simulation(target_distance,
                   speed,
                   angle_deg,
                   initial_offset,
                   guidance,
                   tolerance,
                   &success,
                   &steps,
                   &final_distance,
                   &final_offset,
                   &final_stability,
                   reason);

    print_result(success,
                 steps,
                 final_distance,
                 final_offset,
                 final_stability,
                 reason);

    return 0;
}