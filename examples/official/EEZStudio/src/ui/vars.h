#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_CO2 = 0,
    FLOW_GLOBAL_VARIABLE_TEMP = 1,
    FLOW_GLOBAL_VARIABLE_VTOC = 2,
    FLOW_GLOBAL_VARIABLE_HUMIDITY = 3
};

// Native global variables

extern const char *get_var_co2();
extern void set_var_co2(const char *value);
extern const char *get_var_temp();
extern void set_var_temp(const char *value);
extern const char *get_var_vtoc();
extern void set_var_vtoc(const char *value);
extern const char *get_var_humidity();
extern void set_var_humidity(const char *value);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/