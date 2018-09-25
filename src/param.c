#include "param.h"
#include "misc.h"
#include <string.h>
#include <stdlib.h>

void Param_init(Param *self, const char *name, double value,
                 double low, double high,
                 ParamType type) {
    assert(self);
    if(low > value || high < value) {
        fprintf(stderr,"%s:%d: can't initialize parameter \"%s\".\n"
                " Value (%g) is not in [%lg, %lg]\n",
                __FILE__,__LINE__, name, value, low, high);
        exit(EXIT_FAILURE);
    }
    self->name = strdup(name);
    CHECKMEM(self->name);
    self->value = value;
    self->low = low;
    self->high = high;
    self->type = type;
    self->formula = self->constr = NULL;
}

// frees only memory allocated within Param, not Param itself
void Param_freePtrs(Param *self) {
    free(self->name);
    if(self->formula)
        free(self->formula);
    if(self->constr)
        te_free(self->constr);
}

/// Print name and value of a Param if it is of type "onlytype"
void Param_print(Param *self, ParamType onlytype, FILE *fp) {
    if(self && (self->type == onlytype))
        fprintf(fp, "   %8s = %lg\n", self->name, self->value);
}

