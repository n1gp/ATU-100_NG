#ifndef ICOM_INTERFACE_H
#define ICOM_INTERFACE_H

#ifdef ENABLE_EUSART
#define ICOM_START    (!PORTCbits.RC6)
#define ICOM_KEY_IN   PORTCbits.RC7
#else
#define ICOM_START    (!PORTAbits.RA6)
#define ICOM_KEY_IN   PORTAbits.RA7
#endif

void icom_interface_init(void);
uint8_t icom_interface_active(void);
void icom_interface_suspend(void);
void icom_interface_clear(void);
void icom_interface_timer_tick(void);
uint8_t icom_interface_event(void);
void icom_interface_tuning(uint8_t active);

#endif
