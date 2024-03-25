// Based on the example for demonstrating basic principles of FITkit3 usage.
//
// It includes GPIO - inputs from button press/release, outputs for LED control,
// timer in output compare mode for generating periodic events (via interrupt
// service routine) and speaker handling (via alternating log. 0/1 through
// GPIO output on a reasonable frequency). Using this as a basis for IMP projects
// as well as for testing basic FITkit3 operation is strongly recommended.
//            (c) 2019 Michal Bidlo, BUT FIT, bidlom@fit.vutbr.cz
// The project part demonstrates using of the Watchdog Timer module.
//					   Vladyslav Tverdokhlib, xtverd01@stud.fit.vutbr.cz
////////////////////////////////////////////////////////////////////////////
/* Header file with all the essential definitions for a given type of MCU */
#include "MK60D10.h"

/* Macros for bit-level registers manipulation */
#define GPIO_PIN_MASK 0x1Fu
#define GPIO_PIN(x) (((1)<<(x & GPIO_PIN_MASK)))

/* Mapping of LEDs and buttons to specific port pins: */
// Note: only D9, SW3 and SW5 are used in this sample app
#define LED_D9  0x20      // Port B, bit 5
#define LED_D10 0x10      // Port B, bit 4
#define LED_D11 0x8       // Port B, bit 3
#define LED_D12 0x4       // Port B, bit 2

#define BTN_SW2 0x400     // Port E, bit 10
#define BTN_SW3 0x1000    // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800     // Port E, bit 11

#define SPK 0x10          // Speaker is on PTA4

int pressed_up = 0, pressed_down = 0;
int beep_flag = 0;
unsigned int compare = 0x200;

/* A delay function */
void delay(long long bound) {

  long long i;
  for(i=0;i<bound;i++);
}

/* Initialize the MCU - basic clock and watchdog settings, */
void MCUInit(void)  {
    MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);

    /*
     * Watchdog setting.
    */

    /* Firstly unlocking watchdog. */
    WDOG->UNLOCK = 0xC520;
    WDOG->UNLOCK = 0xD928;

    WDOG->STCTRLH = 0x5; // periodic mode.
    //WDOG->STCTRLH = 0xD; // windowed mode.

    /* Watchdog time-out. */
    WDOG->TOVALH = 0;
    WDOG->TOVALL = 7000; // 7 sec.

    /* No division => a watchdog cycle is 1ms. */
    WDOG->PRESC = 0x0;

    /* Settings in case of windowed mode. */
    if (WDOG->STCTRLH == 0xD)
    {
    	WDOG->WINH = 0;
        WDOG->WINL = 1000; // 1 sec.
    }
}

void PortsInit(void)
{
    /* Turn on all port clocks */
    SIM->SCGC5 = SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTA_MASK;

    /* Set corresponding PTB pins (connected to LED's) for GPIO functionality */
    PORTB->PCR[5] = PORT_PCR_MUX(0x01); // D9
    PORTB->PCR[4] = PORT_PCR_MUX(0x01); // D10
    PORTB->PCR[3] = PORT_PCR_MUX(0x01); // D11
    PORTB->PCR[2] = PORT_PCR_MUX(0x01); // D12

    PORTE->PCR[10] = PORT_PCR_MUX(0x01); // SW2
    PORTE->PCR[12] = PORT_PCR_MUX(0x01); // SW3
    PORTE->PCR[27] = PORT_PCR_MUX(0x01); // SW4
    PORTE->PCR[26] = PORT_PCR_MUX(0x01); // SW5
    PORTE->PCR[11] = PORT_PCR_MUX(0x01); // SW6

    PORTA->PCR[4] = PORT_PCR_MUX(0x01);  // Speaker

    /* Change corresponding PTB port pins as outputs */
    PTB->PDDR = GPIO_PDDR_PDD(0x3C);     // LED ports as outputs
    PTA->PDDR = GPIO_PDDR_PDD(SPK);     // Speaker as output
    PTB->PDOR |= GPIO_PDOR_PDO(0x3C);    // turn all LEDs OFF
    PTA->PDOR &= GPIO_PDOR_PDO(~SPK);   // Speaker off, beep_flag is false
}

void LPTMR0_IRQHandler(void)
{
    // Set new compare value set by up/down buttons
    LPTMR0_CMR = compare;                // !! the CMR reg. may only be changed while TCF == 1
    LPTMR0_CSR |=  LPTMR_CSR_TCF_MASK;   // writing 1 to TCF tclear the flag

    GPIOB_PDOR |= LED_D9;
    GPIOB_PDOR |= LED_D10;
    GPIOB_PDOR |= LED_D11;
    GPIOB_PDOR |= LED_D12;

    beep_flag = !beep_flag;              // see beep_flag test in main()
}

void LPTMR0Init(int count)
{
    SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK; // Enable clock to LPTMR
    LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;   // Turn OFF LPTMR to perform setup
    LPTMR0_PSR = ( LPTMR_PSR_PRESCALE(0) // 0000 is div 2
                 | LPTMR_PSR_PBYP_MASK   // LPO feeds directly to LPT
                 | LPTMR_PSR_PCS(1)) ;   // use the choice of clock
    LPTMR0_CMR = count;                  // Set compare value
    LPTMR0_CSR =(  LPTMR_CSR_TCF_MASK    // Clear any pending interrupt (now)
                 | LPTMR_CSR_TIE_MASK    // LPT interrupt enabled
                );
    NVIC_EnableIRQ(LPTMR0_IRQn);         // enable interrupts from LPTMR0
    LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;    // Turn ON LPTMR0 and start counting
}

int main(void)
{
    MCUInit();
    PortsInit();
    LPTMR0Init(compare);

    int cntr = -1; // counter for leds.

    while (1) {
    	/* Pressing the up button. */
        if (!pressed_up && !(GPIOE_PDIR & BTN_SW5))
        {
        	cntr++;
            pressed_up = 1;

            LPTMR0Init(compare); // clock reset.
            beep_flag = 1; // to beep at first.

            /* Setting for watchdog refresh. */
        	WDOG->REFRESH = 0xA602;
        	WDOG->REFRESH = 0xB480;
        }
        /* Ignoring button hold. */
        else if (GPIOE_PDIR & BTN_SW5) pressed_up = 0;

        /* Case of start after reset. */
        if(cntr == -1)
        {
            GPIOB_PDOR &= LED_D9;
            GPIOB_PDOR &= LED_D10;
            GPIOB_PDOR &= LED_D11;
            GPIOB_PDOR &= LED_D12;
        }

        /* LED Configuration depending on the counter .*/
        if(cntr % 4 == 0)
        {
            GPIOB_PDOR ^= LED_D9; // invert D9 state

            GPIOB_PDOR |= LED_D10;
            GPIOB_PDOR |= LED_D11;
            GPIOB_PDOR |= LED_D12;
        }
        if(cntr % 4 == 1)
        {
            GPIOB_PDOR ^= LED_D10; // invert D10 state

            GPIOB_PDOR |= LED_D9;
            GPIOB_PDOR |= LED_D11;
            GPIOB_PDOR |= LED_D12;
        }
        if(cntr % 4 == 2)
        {
            GPIOB_PDOR ^= LED_D11; // invert D11 state

            GPIOB_PDOR |= LED_D9;
            GPIOB_PDOR |= LED_D10;
            GPIOB_PDOR |= LED_D12;
        }
        if(cntr % 4 == 3)
        {
            GPIOB_PDOR ^= LED_D12; // invert D12 state

            GPIOB_PDOR |= LED_D9;
            GPIOB_PDOR |= LED_D10;
            GPIOB_PDOR |= LED_D11;
        }

         /* Test beep_flag in order to beep or not beep. */
         if (beep_flag) {
             GPIOA_PDOR ^= SPK; // invert speaker state.
             delay(10000);
         } else
             GPIOA_PDOR &= ~SPK; // logic 0 on speaker port if beep is false.
    }
    return 0;
}
