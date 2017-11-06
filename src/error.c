#include "error.h"
#include <stdio.h>
#include <string.h>

int mystrerror_r(int errnum, char *buff, size_t len) {
    int status=0, rval=0;
    
    switch(errnum) {
    case NO_ANCESTRAL_ALLELE:
        rval = snprintf(buff, len, "No ancestral allele");
        break;
    case REF_ALT_MISMATCH:
        rval = snprintf(buff, len, "Inconsistent REF and ALT alleles");
        break;
    case BUFFER_OVERFLOW:
        rval = snprintf(buff, len, "Buffer overflow");
        break;
    case BAD_RAF_INPUT:
        rval = snprintf(buff, len, "Bad .raf input file");
        break;
    case BAD_SORT:
        rval = snprintf(buff, len, "Incorrect sort");
        break;
    default:
        status = strerror_r(errnum, buff, len);
    }
    if(rval>=len || status!=0) {
        fprintf(stderr,"%s:%s:%d: buffer overflow\n",
                __FILE__,__func__,__LINE__);
        exit(EXIT_FAILURE);
    }
    
    return 0;
}