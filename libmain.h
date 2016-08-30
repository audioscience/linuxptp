
struct clock;

struct clock *ptp4l_setup(char *config);
void ptp4l_destroy(struct clock *clock);