#ifndef HAL_TYPES_H
#define HAL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Common status contract for every pd_hal_* interface. */
typedef enum {
    PD_HAL_OK = 0,
    PD_HAL_ERR_NOT_IMPLEMENTED = -1,
    PD_HAL_ERR_IO = -2,
    PD_HAL_ERR_INVALID_ARG = -3,
    PD_HAL_ERR_TIMEOUT = -4,
    PD_HAL_ERR_UNSUPPORTED = -5,
} pd_hal_status_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_TYPES_H */
