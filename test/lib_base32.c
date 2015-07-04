#include "base32.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
    const char *str = "foo bar";
    size_t len = base32_encode_length(strlen(str));
    fail_unless(len = 13);
    char *tmp = malloc(len);
    fail_unless(base32_encode((uint8_t*)str, strlen(str), tmp, len) == 12);
    printf("OUT %s\n",tmp);
    fail_unless(strcmp(tmp,"mzxw6idcmfza") == 0);

    size_t len2 = base32_decode_floor(strlen(tmp));
    fail_unless(len2 == strlen(str));
    char *data = malloc(len2);
    fail_unless(base32_decode(tmp, len, (uint8_t*)data, len2) == len2);
    fail_unless(memcmp(data, str, base32_decode_floor(strlen(tmp))) == 0);

    const char *b = "RF354FEUIXRYFO4O372SXJLKJDIQ2XKIO3XU37OPKEOUEQ7OACWYRFLWDZU5E2QEMESWOS56BIL65HRBFQJHPF4HGWFD6F4E5EJWSA5NZYSHJJP6UZXPUTA5DGOIRIBJILKE65QPP6WBB3I54JRPHUG3XH6NRQTCTLHWAL73NAQ2BCBPKBL3FCZPCAWYNZ7ZN3OHFX2FOZ277IPPLBEZCP6SHDTKJ3CP6SUF2Q6PGZOBFDRFKOX6ERXQADLK6RH4BCOAVRBRQ3S4BTZWLQJI4JKTV7REN4AA22XUJ7AITQFMIMMG4XAA";
    size_t outlen = base32_decode_floor(strlen(b));
    fail_unless(outlen == 192);
    void *data2 = malloc(outlen);
    fail_unless(base32_decode(b, 0, data2, outlen) == outlen);

    return 0;
}

