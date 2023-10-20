#pragma once

#define NFC_TASK_PRIO 5
#define NFC_UID_INVALID 0

#define NFC_RETRIES 5

void nfc_init();
uint64_t nfc_get_current_uid();
