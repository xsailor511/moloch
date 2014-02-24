#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"
#
typedef struct {
    uint16_t   sshLen;
    uint8_t    sshCode;
} SSHInfo_t;

/******************************************************************************/
int ssh_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining)
{
    SSHInfo_t *ssh = uw;

    if (memcmp("SSH", data, 3) == 0) {
        unsigned char *n = memchr(data, 0x0a, remaining);
        if (n && *(n-1) == 0x0d)
            n--;

        if (n) {
            int len = (n - data);

            char *str = g_ascii_strdown((char *)data, len);

            if (!moloch_field_string_add(MOLOCH_FIELD_SSH_VER, session, str, len, FALSE)) {
                free(str);
            }
        }
        return 0;
    }

    if (session->which != 1)
        return 0;

    while (remaining >= 6) {
        if (ssh->sshLen == 0) {
            ssh->sshLen = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + 4;
            ssh->sshCode = data[5];
            if (ssh->sshLen == 0) {
                break;
            }
        }

        if (ssh->sshCode == 33 && remaining > 8) {
            uint32_t keyLen = data[6] << 24 | data[7] << 16 | data[8] << 8 | data[9];
            moloch_parsers_unregister(session, uw);
            if ((uint32_t)remaining > keyLen + 8) {
                char *str = g_base64_encode(data+10, keyLen);

                if (!moloch_field_string_add(MOLOCH_FIELD_SSH_KEY, session, str, (keyLen/3+1)*4, FALSE)) {
                    g_free(str);
                }
            }
            break;
        }

        if (remaining > ssh->sshLen) {
            remaining -= ssh->sshLen;
            ssh->sshLen = 0;
            continue;
        } else {
            ssh->sshLen -= remaining;
            remaining = 0;
            continue;
        }
    }
    return 0;
}
/******************************************************************************/
void ssh_free(MolochSession_t UNUSED(*session), void *uw)
{
    SSHInfo_t            *ssh          = uw;

    MOLOCH_TYPE_FREE(SSHInfo_t, ssh);
}
/******************************************************************************/
void ssh_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len))
{
    if (moloch_nids_has_tag(session, MOLOCH_FIELD_TAGS, "protocol:ssh"))
        return;

    moloch_nids_add_tag(session, MOLOCH_FIELD_TAGS, "protocol:ssh");

    SSHInfo_t            *ssh          = MOLOCH_TYPE_ALLOC0(SSHInfo_t);

    moloch_parsers_register(session, ssh_parser, ssh, ssh_free);
    ssh_parser(session, ssh, data, len);
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_field_define_internal(MOLOCH_FIELD_SSH_VER,       "sshver", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);
    moloch_field_define_internal(MOLOCH_FIELD_SSH_KEY,       "sshkey", MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT);

    moloch_parsers_classifier_register_tcp("ssh", 0, (unsigned char*)"SSH", 3, ssh_classify);
}
