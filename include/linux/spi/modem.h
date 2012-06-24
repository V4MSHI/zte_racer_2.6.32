/*
    Copyright (C) 2011 HISENSE COMPANY LIMITED.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met
    1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation andor other materials provided with the distribution.
    3. For alternatively licensed, multiple-licensed(dual-licensed, tri-licensed) , 
       this software is distributed under the terms of BSD license. 
    4. Neither the name of HISENSE COMPANY LIMITED nor
         the names of its contributors may be used to endorse or promote
         products derived from this software without specific prior written
         permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LINUX_SPI_MODEM_H
#define LINUX_SPI_MODEM_H

#define AP_RTS_TRUE			(1)
#define AP_RTS_FALSE		(0)

#define AP_RDY_TRUE			(0)
#define AP_RDY_FALSE		(1)

#define AP_WAKEUP_TRUE		(1)
#define AP_WAKEUP_FALSE		(0)

#define AP_SLEEP_TRUE		(0)
#define AP_SLEEP_FALSE		(1)

#define BP_RTS_TRUE			(0)
#define BP_RTS_FALSE		(1)

#define BP_RDY_TRUE			(0)
#define BP_RDY_FALSE		(1)

#define BP_SLEEP_TRUE		(0)
#define BP_SLEEP_FALSE		(1)

#define BP_WAKEUP_TRUE		(0)
#define BP_WAKEUP_FALSE		(1)

#define BP_LIFE_TRUE		(1)
#define BP_LIFE_FALSE		(0)

typedef enum {
	AP_RTS,
	AP_RDY,
	AP_WAKEUP,
	AP_SLEEP,
}AP_HANDSHAKE_PIN;

typedef enum {
	BP_RTS,
	BP_RDY,
	BP_WAKEUP,
	BP_SLEEP,
	BP_LIFE,
}BP_HANDSHAKE_PIN;

typedef enum {
	PWR_ON,
	PWR_OFF,
	PWR_RESETING,
}MODEM_PWR_STATE;

struct modem_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);	
	int (*off_state_setting)(void);
	int (*on_state_setting)(void);
	int (*ap_handshake_set)(AP_HANDSHAKE_PIN pin, int state);
	int (*bp_handshake_get)(BP_HANDSHAKE_PIN pin);
	int (*lp_switch)(unsigned int on);

	int bp_rts_irq_gpio;
	int bp_rdy_irq_gpio;
	int bp_wakeup_irq_gpio;
	int bp_life_irq_gpio;
	
	unsigned long private_data;
    
};

#endif
