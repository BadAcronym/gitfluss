#include <stdint.h>

int main
(
    void
){
    // TODO: read these from some config file
    const char *hardcoded = "/home/mandi/repository/River;/home/mandi/repository/river2D_mapedit";
    const char *authors   = "terribleacronym@gmail.com";
    uint8_t     actions   = 0; // enum int with like COMMITS_ONLY, COMMITS_MERGES, etc etc

    // TODO: figure out how to call binaries from a spawned shell safely on each platform.
    // then, for each repository in the path, with appropriate tempfile path:
    // gitfluss_run("cd repo; git log > tempfile");

    // TODO: display gathered data:
    // heatmap with calendar dates and one row per weekday, YTD
    // total actions YTD
    // total actions lifetime
    // most active repository

    return 0;
}
