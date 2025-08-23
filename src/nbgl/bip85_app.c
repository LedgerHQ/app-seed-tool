#include <os.h>

#include "../common/common.h"
#include "../common/bip85/common_bip85.h"
#include "./bip39_mnemonic.h"

#if defined(SCREEN_SIZE_WALLET)

typedef struct bip85_buffer_struct {
    // buffer to hold BIP85 app output
    uint8_t buffer[BIP85_ENTROPY_LENGTH];
    uint8_t length;
    // type of BIP85 app we are using
    uint8_t type;
    // BIP85 derivation path index
    unsigned int index;

} bip85_buffer_t;

static bip85_buffer_t app_data = {0};

void bip85_type_set(const uint8_t type) {
    app_data.type = type;
}

uint8_t bip85_type_get(void) {
    return app_data.type;
}

void bip85_index_set(const uint32_t index) {
    app_data.index = index;
}

uint32_t bip85_index_get(void) {
    return app_data.index;
}

void bip85_app_reset(void) {
    memzero(&app_data, sizeof(app_data));
}

void bip85_app_bip39_gen(void){
    app_data.length = bolos_ux_bip85_bip39(app_data.buffer, 0, bip39_mnemonic_final_size_get(), app_data.index);
    bip39_mnemonic_encode(app_data.buffer, app_data.length);
}
#endif
