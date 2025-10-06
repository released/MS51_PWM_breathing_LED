/*_____ I N C L U D E S ____________________________________________________*/
#include "numicro_8051.h"

#include "misc_config.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

#define TIMER_DIV12_1ms_FOSC_240000  			(65536-2000)
#define TH0_INIT        						(HIBYTE(TIMER_DIV12_1ms_FOSC_240000)) 
#define TL0_INIT        						(LOBYTE(TIMER_DIV12_1ms_FOSC_240000))


//UART 0
bit BIT_TMP;
bit BIT_UART;
bit uart0_receive_flag=0;
unsigned char uart0_receive_data;

volatile struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS           (flag_PROJ_CTL.bit0)
#define FLAG_PROJ_REVERSE1                      (flag_PROJ_CTL.bit1)
#define FLAG_PROJ_REVERSE2                 		(flag_PROJ_CTL.bit2)
#define FLAG_PROJ_REVERSE3                      (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_REVERSE4                      (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_REVERSE5                      (flag_PROJ_CTL.bit5)
#define FLAG_PROJ_REVERSE6                      (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_REVERSE7                      (flag_PROJ_CTL.bit7)

/*_____ D E F I N I T I O N S ______________________________________________*/
#define UDIV_ROUND_NEAREST(a,b)                 ( ((unsigned long)(a) + ((unsigned long)(b)/2u)) / (unsigned long)(b) )

// #define USE_P15_PWM0_CH5
#define USE_P12_PWM0_CH0

#define USE_FLASH_TBL
// #define USE_RAM_TBL

volatile unsigned long counter_tick = 0;

#define BREATH_STEPS         				    (128u) 
#define BREATH_GAMMA_SCALE   			        (1023u)
#define BREATH_DEFAULT_MS    				    (500u)

// 24MHz / 128 = 187,500 Hz; 187,500 / 100 Hz = 1,875 ticks → PERIOD=1875-1
#define PWM_BASE_FREQ_HZ                        (100u)
#define PWM_DIV_FOR_100HZ                       (128u)
#define PWM_PERIOD_TICKS                        ((SYS_CLOCK / PWM_DIV_FOR_100HZ) / PWM_BASE_FREQ_HZ)  // =1875

typedef struct _breathing_led_manager_t
{
	unsigned char  	s_breath_enable;
	unsigned char  	s_active_high;
	unsigned int 	s_breath_period_ms;
	unsigned int 	s_step_interval_ms;
	unsigned int 	s_step_elapsed_ms;
	unsigned int  	s_step_idx;
	unsigned char 	s_dir_up;
	
}BREATHING_LED_MANAGER_T;


BREATHING_LED_MANAGER_T breathing_led = 
{
    0U,						/*s_breath_enable*/
    0U,						/*s_active_high*/
    BREATH_DEFAULT_MS,		/*s_breath_period_ms*/
    0U,						/*s_step_interval_ms*/
    0U,						/*s_step_elapsed_ms*/
    0U,						/*s_step_idx*/
	0U,						/*s_dir_up*/
};


static code unsigned int k_gamma22_0_1023_128[BREATH_STEPS] = 
{
      0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   6,   7,   8,   9,
     11,  12,  14,  16,  18,  20,  22,  24,  26,  29,  31,  34,  37,  40,  43,  46,
     49,  53,  56,  60,  64,  68,  72,  76,  81,  85,  90,  94,  99, 104, 110, 115,
    120, 126, 132, 137, 143, 150, 156, 162, 169, 176, 182, 189, 197, 204, 211, 219,
    227, 234, 242, 251, 259, 267, 276, 285, 294, 303, 312, 321, 331, 340, 350, 360,
    370, 380, 391, 401, 412, 423, 434, 445, 456, 468, 480, 491, 503, 515, 528, 540,
    553, 565, 578, 591, 605, 618, 632, 645, 659, 673, 687, 702, 716, 731, 746, 761,
    776, 791, 807, 822, 838, 854, 870, 887, 903, 920, 936, 953, 971, 988,1005,1023
};

#if defined (USE_FLASH_TBL)
static code unsigned int s_breath_duty_ticks[BREATH_STEPS] = 
{
	0,    0,    0,    0,   2,   2,   2,   4,   4,   5,   7,   9,  11,  13,  15,  16,
	20,   22,   26,  29,  33,  37,  40,  44,  48,  53,  57,  62,  68,  73,  79,  84,
	90,   97,  103, 110, 117, 125, 132, 139, 148, 156, 165, 172, 181, 191, 202, 211,
	220,  231, 242, 251, 262, 275, 286, 297, 310, 322, 333, 346, 361, 374, 387, 401,
	416,  429, 443, 460, 474, 489, 506, 522, 539, 555, 572, 588, 606, 623, 641, 659,
	678,  696, 716, 735, 755, 775, 795, 815, 835, 857, 879, 899, 921, 943, 967, 989,
	1013,1035,1059,1083,1108,1132,1158,1182,1207,1233,1258,1286,1312,1339,1367,1394,
	1422,1449,1478,1506,1535,1564,1594,1625,1654,1685,1715,1746,1779,1810,1841,1874
};
#endif

#if defined (USE_RAM_TBL)
static unsigned int s_breath_duty_ticks[BREATH_STEPS] = {0};
#endif

/*_____ M A C R O S ________________________________________________________*/
#define SYS_CLOCK 								(24000000ul)

/*_____ F U N C T I O N S __________________________________________________*/


static unsigned long get_tick(void)
{
	return (counter_tick);
}

static void set_tick(unsigned long t)
{
	counter_tick = t;
}

static void tick_counter(void)
{
	counter_tick++;
}

#if defined (REDUCE_CODE_SIZE)
void send_UARTString(uint8_t* Data)
{
	#if 1
	uint16_t i = 0;

	while (Data[i] != '\0')
	{
		#if 1
		SBUF = Data[i++];
		#else
		UART_Send_Data(UART0,Data[i++]);		
		#endif
	}

	#endif

	#if 0
	uint16_t i = 0;
	
	for(i = 0;i< (strlen(Data)) ;i++ )
	{
		UART_Send_Data(UART0,Data[i]);
	}
	#endif

	#if 0
    while(*Data)  
    {  
        UART_Send_Data(UART0, (unsigned char) *Data++);  
    } 
	#endif
}

void send_UARTASCII(uint16_t Temp)
{
    uint8_t print_buf[16];
    uint16_t i = 15, j;

    *(print_buf + i) = '\0';
    j = (uint16_t)Temp >> 31;
    if(j)
        (uint16_t) Temp = ~(uint16_t)Temp + 1;
    do
    {
        i--;
        *(print_buf + i) = '0' + (uint16_t)Temp % 10;
        (uint16_t)Temp = (uint16_t)Temp / 10;
    }
    while((uint16_t)Temp != 0);
    if(j)
    {
        i--;
        *(print_buf + i) = '-';
    }
    send_UARTString(print_buf + i);
}
void send_UARTHex(uint16_t u16Temp)
{
    uint8_t print_buf[16];
    unsigned long i = 15;
    unsigned long temp;

    *(print_buf + i) = '\0';
    do
    {
        i--;
        temp = u16Temp % 16;
        if(temp < 10)
            *(print_buf + i) = '0' + temp;
        else
            *(print_buf + i) = 'a' + (temp - 10) ;
        u16Temp = u16Temp / 16;
    }
    while(u16Temp != 0);
    send_UARTString(print_buf + i);
}

#endif

void delay(uint16_t dly)
{
/*
	delay(100) : 14.84 us
	delay(200) : 29.37 us
	delay(300) : 43.97 us
	delay(400) : 58.5 us	
	delay(500) : 73.13 us	
	
	delay(1500) : 0.218 ms (218 us)
	delay(2000) : 0.291 ms (291 us)	
*/

	while( dly--);
}

static void breath_recalc_interval(void)
{
    unsigned long t = (unsigned long)breathing_led.s_breath_period_ms;
    unsigned int  steps_full  = (unsigned int)(2u*(BREATH_STEPS - 1u));

    breathing_led.s_step_interval_ms = (unsigned int)UDIV_ROUND_NEAREST(t, steps_full);
    if (breathing_led.s_step_interval_ms == 0) 
		breathing_led.s_step_interval_ms = 1;
	
	breathing_led.s_step_elapsed_ms = 0u;
}

void breath100_set_active_high(unsigned char ah)
{
    breathing_led.s_active_high = (ah ? 1u : 0u);
}

void breath_set_period_ms(unsigned int ms)
{
	if (ms == 0) 
		ms = 1;
    breathing_led.s_breath_period_ms = ms;
    breath_recalc_interval();
    breathing_led.s_step_elapsed_ms = 0;
}

void breath_enable(unsigned char on)
{
    breathing_led.s_breath_enable = (on ? 1u : 0u);
}

void breath_init(unsigned char enable_led_gpio)
{
	if (enable_led_gpio)
	{
		// PWM init
        #if defined (USE_P12_PWM0_CH0)
		P12_PUSHPULL_MODE;	//Add this to enhance MOS output capability
		ENABLE_PWM0_CH0_P12_OUTPUT;	
        #endif
        #if defined (USE_P15_PWM0_CH5)
		P15_PUSHPULL_MODE;	//Add this to enhance MOS output capability
		ENABLE_PWM0_CH5_P15_OUTPUT;	
        #endif

		PWM0_IMDEPENDENT_MODE;		
		PWM0_CLOCK_DIV_128;
		PWMPH = HIBYTE(PWM_PERIOD_TICKS -1u);
		PWMPL = LOBYTE(PWM_PERIOD_TICKS -1u);

		set_PWMCON0_LOAD;
		set_PWMCON0_PWMRUN;	

		// breathing LED init
		breath100_set_active_high(0);	// LOW ACTIVE

		/*
			fomula : bpm = 60000 / full_ms
			general : 5000ms , 12 bpm
			relex : 10000ms , 6 bpm
			sleep : 14000ms , 4.3 bpm
		*/
		#if defined (PERIOD_1000MS)
		breath_set_period_ms(1000u);
		#endif
		#if defined (PERIOD_2000MS)
		breath_set_period_ms(2000u);
		#endif
		#if defined (PERIOD_3750MS)
		breath_set_period_ms(3750u);
		#endif
		#if defined (PERIOD_5000MS)
		breath_set_period_ms(5000u);
		#endif
		#if defined (PERIOD_10000MS)
		breath_set_period_ms(10000u);
		#endif
		#if defined (PERIOD_14000MS)
		breath_set_period_ms(14000u);
		#endif

		breathing_led.s_step_idx        = 0;
		breathing_led.s_step_elapsed_ms = 0;

        #if defined (USE_P12_PWM0_CH0) 
		PWM0H = HIBYTE(s_breath_duty_ticks[0]);
		PWM0L = LOBYTE(s_breath_duty_ticks[0]);
        #endif
        #if defined (USE_P15_PWM0_CH5) 
		PWM5H = HIBYTE(s_breath_duty_ticks[0]);
		PWM5L = LOBYTE(s_breath_duty_ticks[0]);
        #endif

		set_PWMCON0_LOAD;
				
		breath_enable(1);
	}
	else
	{		
		breath_enable(0);

		//reset parameter
		breathing_led.s_active_high = 0U;
		breathing_led.s_breath_period_ms = BREATH_DEFAULT_MS;
		breathing_led.s_step_interval_ms = 0U;
		breathing_led.s_step_elapsed_ms = 0U;
		breathing_led.s_step_idx = 0U;
		breathing_led.s_dir_up = 0U;

		// clr_PWMCON0_PWMRUN;
        #if defined (USE_P12_PWM0_CH0)
		DISABLE_PWM0_CH0_P12_OUTPUT;
		P12 = 1;
		P12_PUSHPULL_MODE;
        #endif
        #if defined (USE_P15_PWM0_CH5)
		DISABLE_PWM0_CH5_P15_OUTPUT;
		P15 = 1;
		P15_PUSHPULL_MODE;
        #endif
	}
}

unsigned int breath_write_duty_from_code_tbl(unsigned char idx, unsigned int period_minus1) /* idx: 0..127 */
{
    unsigned int duty;

    duty = s_breath_duty_ticks[idx];

	if (breathing_led.s_active_high == 0u)	// active low
	{
    	duty = (unsigned int)(period_minus1 - duty);
	}

	return duty;
}

void breath_1ms_IRQ(void)
{
	unsigned int duty = 0;

	if (breathing_led.s_breath_enable)
	{
		if (++breathing_led.s_step_elapsed_ms >= breathing_led.s_step_interval_ms) 
		{
			breathing_led.s_step_elapsed_ms = 0u;

			if (breathing_led.s_dir_up)
            {
                if (breathing_led.s_step_idx < (BREATH_STEPS - 1u))
                {
                    breathing_led.s_step_idx++;
                }
                else
                {
                    breathing_led.s_dir_up = 0u;
                    breathing_led.s_step_idx--;
                }
            }
            else
            {
                if (breathing_led.s_step_idx > 0u)
                {
                    breathing_led.s_step_idx--;
                }
                else
                {
                    breathing_led.s_dir_up = 1u;
                    breathing_led.s_step_idx++;
                }
            }

			duty = breath_write_duty_from_code_tbl(breathing_led.s_step_idx, PWM_PERIOD_TICKS - 1 );

            #if defined (USE_P12_PWM0_CH0) 
			PWM0H = HIBYTE(duty);
			PWM0L = LOBYTE(duty);
            #endif
            
            #if defined (USE_P15_PWM0_CH5) 
			PWM5H = HIBYTE(duty);
			PWM5L = LOBYTE(duty);        
            #endif
			set_PWMCON0_LOAD;
		}
	}    
}

/*
	generate s_breath_duty_ticks , base on PWM_PERIOD_TICKS - 1u
	by below two method
	USE_FLASH_TBL : generate table and copy into s_breath_duty_ticks (flash)
	USE_RAM_TBL : generate table and put in s_breath_duty_ticks (ram) 
*/
void breath_generate_duty_tbl(void)
{
	unsigned int i = 0;
	unsigned int duty = 0;
	unsigned int period = PWM_PERIOD_TICKS - 1u;
	unsigned long num = 0;

	#if defined (USE_FLASH_TBL)
	for ( i = 0 ; i < BREATH_STEPS ; i++)
	{
        num = (unsigned long) k_gamma22_0_1023_128[i] * period;
		duty = (unsigned int) UDIV_ROUND_NEAREST(num,1023);
		// duty = ( k_gamma22_0_1023_128[i] * (period) + 511 ) / 1023 ;
        
		printf("%4d,",duty);
        if ((i+1)%16 == 0)
        {
            printf("\r\n");
        }  	
	}
    printf("\r\n\r\n");	
	#endif

	#if defined (USE_RAM_TBL)
    for (i = 0; i < BREATH_STEPS; i++) 
	{
        /* duty = round( tbl[i] * period / 1023 ) */
        num = (unsigned long) k_gamma22_0_1023_128[i] * period;
		duty = (unsigned int) UDIV_ROUND_NEAREST(num,1023);
        if (duty > period) 
			duty = period;

        s_breath_duty_ticks[i] = duty;
    }


	#endif
}


void loop(void)
{
	// static uint16_t LOG = 0;	
	// if (FLAG_PROJ_TIMER_PERIOD_1000MS)
	// {
	// 	FLAG_PROJ_TIMER_PERIOD_1000MS = 0;	
	// 	printf("LOG : %4d\r\n",LOG++);
	// 	P12 ^= 1;		
	// }
}

void GPIO_Init(void)
{
	// P12 = 0;
	P17 = 0;
	P30 = 0;
	
	// P12_PUSHPULL_MODE;		
	P17_QUASI_MODE;		
	P30_PUSHPULL_MODE;	
}

void Timer0_IRQHandler(void)
{

	tick_counter();

	if ((get_tick() % 1000) == 0)
	{
		FLAG_PROJ_TIMER_PERIOD_1000MS = 1;

	}

	if ((get_tick() % 50) == 0)
	{

	}		

	breath_1ms_IRQ();
}

void Timer0_ISR(void) interrupt 1        // Vector @  0x0B
{
    _push_(SFRS);	
	
    clr_TCON_TF0;
	TH0 = TH0_INIT;
	TL0 = TL0_INIT;	
	
	Timer0_IRQHandler();

    _pop_(SFRS);	
}

void TIMER0_Init(void)
{
	/*
		formula : 16bit 
		(0xFFFF+1 - target)  / (24MHz/psc) = time base 
	*/	
	
	ENABLE_TIMER0_MODE1;	// mode 0 : 13 bit , mode 1 : 16 bit
    TIMER0_FSYS_DIV12;

	TH0 = TH0_INIT;
	TL0 = TL0_INIT;
	clr_TCON_TF0;

    set_TCON_TR0;                                  //Timer0 run
    ENABLE_TIMER0_INTERRUPT;                       //enable Timer0 interrupt
    ENABLE_GLOBAL_INTERRUPT;                       //enable interrupts
  
}

void Serial_ISR (void) interrupt 4 
{
    _push_(SFRS);

    if (RI)
    {   
      uart0_receive_flag = 1;
      uart0_receive_data = SBUF;
      clr_SCON_RI;                                         // Clear RI (Receive Interrupt).
    }
    if  (TI)
    {
      if(!BIT_UART)
      {
          TI = 0;
      }
    }

    _pop_(SFRS);	
}

void UART0_Init(void)
{
	#if 1
	const unsigned long u32Baudrate = 115200;
	P06_QUASI_MODE;    //Setting UART pin as Quasi mode for transmit
	
	SCON = 0x50;          //UART0 Mode1,REN=1,TI=1
	set_PCON_SMOD;        //UART0 Double Rate Enable
	T3CON &= 0xF8;        //T3PS2=0,T3PS1=0,T3PS0=0(Prescale=1)
	set_T3CON_BRCK;        //UART0 baud rate clock source = Timer3

	RH3    = HIBYTE(65536 - (SYS_CLOCK/16/u32Baudrate));  
	RL3    = LOBYTE(65536 - (SYS_CLOCK/16/u32Baudrate));  
	
	set_T3CON_TR3;         //Trigger Timer3
	
	/*
		set UART priority
		IPH / EIPH / EIPH1 	, IP / EIP / EIP2 
		0  						0  					Level 0 (lowest) 
		0  						1  					Level 1 
		1  						0  					Level 2 
		1  						1  					Level 3 (highest) 

		SET_INT_UART0_LEVEL0;	//clr_IP_PS; clr_IPH_PSH; //0
		SET_INT_UART0_LEVEL1;	//clr_IP_PS; set_IPH_PSH; //1
		SET_INT_UART0_LEVEL2;	//set_IP_PS; clr_IPH_PSH; //2
		SET_INT_UART0_LEVEL3;	//set_IP_PS; set_IPH_PSH; //3
	*/
	
	ENABLE_UART0_INTERRUPT;
	ENABLE_GLOBAL_INTERRUPT;

	set_SCON_TI;
	BIT_UART=1;
	#else	
    UART_Open(SYS_CLOCK,UART0_Timer3,115200);
    ENABLE_UART0_PRINTF; 
	#endif
}


void MODIFY_HIRC_24(void)
{
	unsigned char u8HIRCSEL = HIRC_24;
    unsigned char data hircmap0,hircmap1;
//    unsigned int trimvalue16bit;
    /* Check if power on reset, modify HIRC */
    set_CHPCON_IAPEN;
    SFRS = 0 ;
	#if 1
    IAPAL = 0x38;
	#else
    switch (u8HIRCSEL)
    {
      case HIRC_24:
        IAPAL = 0x38;
      break;
      case HIRC_16:
        IAPAL = 0x30;
      break;
      case HIRC_166:
        IAPAL = 0x30;
      break;
    }
	#endif
	
    IAPAH = 0x00;
    IAPCN = READ_UID;
    set_IAPTRG_IAPGO;
    hircmap0 = IAPFD;
    IAPAL++;
    set_IAPTRG_IAPGO;
    hircmap1 = IAPFD;
    // clr_CHPCON_IAPEN;

	#if 0
    switch (u8HIRCSEL)
    {
		case HIRC_166:
		trimvalue16bit = ((hircmap0 << 1) + (hircmap1 & 0x01));
		trimvalue16bit = trimvalue16bit - 15;
		hircmap1 = trimvalue16bit & 0x01;
		hircmap0 = trimvalue16bit >> 1;

		break;
		default: break;
    }
	#endif
	
    TA = 0xAA;
    TA = 0x55;
    RCTRIM0 = hircmap0;
    TA = 0xAA;
    TA = 0x55;
    RCTRIM1 = hircmap1;
    clr_CHPCON_IAPEN;
    // PCON &= CLR_BIT4;
}


void SYS_Init(void)
{
    MODIFY_HIRC_24();

    // ALL_GPIO_QUASI_MODE;
    ENABLE_GLOBAL_INTERRUPT;                // global enable bit	
}

void main (void) 
{
    SYS_Init();

    UART0_Init();
	GPIO_Init();
	TIMER0_Init();

	breath_generate_duty_tbl();	// to generate tbl : s_breath_duty_ticks
	breath_init(1);
		
    while(1)
    {
		loop();
			
    }
}



