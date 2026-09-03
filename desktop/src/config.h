#pragma once

// Update checks hit this repo's GitHub Releases API. If it's unreachable the
// UI says so rather than pretending the check succeeded.
#define POLYPI_GITHUB_REPO "tommfr38/polyPi"

#ifndef POLYPI_VERSION
#define POLYPI_VERSION "0.1.0"
#endif
