#include <psa/crypto.h>
#include <stddef.h>
size_t larp_psa_volatile_slots(void) {
    mbedtls_psa_stats_t stats;
    mbedtls_psa_get_stats(&stats);
    return stats.MBEDTLS_PRIVATE(volatile_slots);
}
