#include <ftr.h>
#include <unistd.h>

int main(void) {
    ftr_init_file(NULL);

    // FTR_EXPR wraps the evaluation of usleep(100) in a trace span.
    // usleep returns int, so the result is captured and returned.
    int ret = FTR_EXPR("usleep(100)", usleep(100));
    (void)ret;

    ftr_close();
    return 0;
}
