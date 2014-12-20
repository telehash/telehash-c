#include "base32.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
    const char *str = "foo bar";
    char *tmp = base32_encode(str, strlen(str));
    fail_unless(strcmp(tmp,"mzxw6idcmfza") == 0);

    char *data = base32_decode(tmp, NULL);
    fail_unless(memcmp(data, str, base32_decode_length(strlen(tmp))) == 0);

    const char *b = "RF354FEUIXRYFO4O372SXJLKJDIQ2XKIO3XU37OPKEOUEQ7OACWYRFLWDZU5E2QEMESWOS56BIL65HRBFQJHPF4HGWFD6F4E5EJWSA5NZYSHJJP6UZXPUTA5DGOIRIBJILKE65QPP6WBB3I54JRPHUG3XH6NRQTCTLHWAL73NAQ2BCBPKBL3FCZPCAWYNZ7ZN3OHFX2FOZ277IPPLBEZCP6SHDTKJ3CP6SUF2Q6PGZOBFDRFKOX6ERXQADLK6RH4BCOAVRBRQ3S4BTZWLQJI4JKTV7REN4AA22XUJ7AITQFMIMMG4XAA";
    size_t outlen = 0;
    void *data2 = base32_decode(b, &outlen);
    fail_unless(data2);
    fail_unless(outlen % 24 == 0);
    fail_unless(outlen == 192);

    return 0;
}

