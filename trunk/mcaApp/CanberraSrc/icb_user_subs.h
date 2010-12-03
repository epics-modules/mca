/* icb_user_subs.h */
/*
 * Author:  Mark Rivers (based on code by Tim Mooney)
 * Date:    July 23, 2000
 *
 * These routines provide a simple way to communicate with ICB modules which
 * are not currently supported by icb_handler_subs.c
 */

#ifdef __cplusplus
extern "C" {
#endif

int parse_ICB_address(char *address, int *ni_module, int *icb_addr);
int write_icb(int ni_module, int icb_addr, int reg, int registers,
        unsigned char *values);
int read_icb(int ni_module, int icb_addr, int reg, int registers,
        unsigned char *values);

#ifdef __cplusplus
}
#endif /*__cplusplus */
