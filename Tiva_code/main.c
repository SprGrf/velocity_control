#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "drivers/pinout.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/flash.h"
#include "driverlib/systick.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/ustdlib.h"
#include "inc/hw_pwm.h"
#include "driverlib/pwm.h"
#include "inc/hw_qei.h"
#include "driverlib/qei.h"
#include "driverlib/timer.h"

#include "main.h"

//#define TARGET_IS_TM4C129_RA0


//*****************************************************************************
//
// Defines for setting up the system clock.
//
//*****************************************************************************
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

//*****************************************************************************
//
// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
//
//*****************************************************************************
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

//*****************************************************************************
//
// The current IP address.
//
//*****************************************************************************
uint32_t g_ui32IPAddress;
//*****************************************************************************
//
// The system clock frequency.
//
//*****************************************************************************

uint32_t g_ui32SysClock;

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

#ifdef ENABLE_ETHERNET
// Initialize the UDP receive pcb
struct udp_pcb * udp_init_r(void);
// Send data over UDP
void udp_send_data(void* sbuf, u16_t len);
// Callback for UDP data reception
void udp_receive_data(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port);
// The variable that hold the UDP receive pcb
struct udp_pcb *Rpcb;
// Variables assgined to the controller pc IP and current board IP
struct ip_addr controller_ip, board_ip;
// Flag that is raised when the IP is assigned
volatile uint8_t gotIP = 0;
// Variables for lwip configuration
unsigned long device_ip,device_subnet,device_gateway;
#endif

// Flags raised when events for encoder send and pwm set are active
bool sendEncoder, setPWMvalue;
// Variable for received PWM command
int8_t pwmValue;

#ifdef ENABLE_ETHERNET
// Display the input IP address on UART
void DisplayIPAddress(uint32_t ui32Addr)
{
    char pcBuf[16];

    //
    // Convert the IP Address into a string.
    //
    usprintf(pcBuf, "%d.%d.%d.%d", ui32Addr & 0xff, (ui32Addr >> 8) & 0xff,
            (ui32Addr >> 16) & 0xff, (ui32Addr >> 24) & 0xff);

    //
    // Display the string.
    //
#ifdef ENABLE_UART
    UARTprintf(pcBuf);
#endif

}
#endif

#ifdef ENABLE_ETHERNET
// Ethernet lwip interrupt handler
void lwIPHostTimerHandler(void)
{
    uint32_t ui32Idx, ui32NewIPAddress;

    //
    // Get the current IP address.
    //
    ui32NewIPAddress = lwIPLocalIPAddrGet();

    //
    // See if the IP address has changed.
    //
    if(ui32NewIPAddress != g_ui32IPAddress)
    {
        //
        // See if there is an IP address assigned.
        //
        if(ui32NewIPAddress == 0xffffffff)
        {
            //
            // Indicate that there is no link.
            //
            //UARTprintf("Waiting for link.\n");
        }
        else if(ui32NewIPAddress == 0)
        {
            //
            // There is no IP address, so indicate that the DHCP process is
            // running.
            //
            //UARTprintf("Waiting for IP address.\n");
        }
        else
        {
            //
            // Display the new IP address.
            //
#ifdef ENABLE_UART
            UARTprintf("IP Address: ");
            DisplayIPAddress(ui32NewIPAddress);
            UARTprintf("\n");
#endif
            // Set the gotIP flag once IP is assigned
            gotIP = 1;
        }

        //
        // Save the new IP address.
        //
        g_ui32IPAddress = ui32NewIPAddress;

        //
        // Turn GPIO off.
        //
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);
    }

    //
    // If there is not an IP address.
    //
    if((ui32NewIPAddress == 0) || (ui32NewIPAddress == 0xffffffff))
    {
        //
        // Loop through the LED animation.
        //

        for(ui32Idx = 1; ui32Idx < 17; ui32Idx++)
        {

            //
            // Toggle the GPIO
            //
            MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1,
                    (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^
                     GPIO_PIN_1));

            SysCtlDelay(g_ui32SysClock/(ui32Idx << 1));
        }
    }
}
#endif

//*****************************************************************************
//
// The interrupt handler for the SysTick interrupt.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    //
    // Call the lwIP timer handler.
    //
#ifdef ENABLE_ETHERNET
    lwIPTimer(SYSTICKMS);
#endif
}

#ifdef ENABLE_UART
// Configure the UART peripheral
void ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);

#ifdef UART_BUFFERED
    UARTEchoSet(false);
#endif
}
#endif

// Generic delay function
void cyclesdelay(unsigned long cycles)
{
	MAP_SysCtlDelay(cycles); // Tiva C series specific
}

#ifdef ENABLE_MOTOR
// Setup the PWM peripheral
void SetupPWM()
{
  SysCtlPWMClockSet(SYSCTL_PWMDIV_1);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
  GPIOPinConfigure(GPIO_PG0_M0PWM4);
  GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_0);

  PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_UP_DOWN |
                    PWM_GEN_MODE_NO_SYNC);

  //
  // Set the PWM period to 1000Hz.  To calculate the appropriate parameter
  // use the following equation: N = (1 / f) * SysClk.  Where N is the
  // function parameter, f is the desired frequency, and SysClk is the
  // system clock frequency.
  // In this case you get: (1 / 20000Hz) * 120MHz = 6000 cycles.  Note that
  // the maximum period you can set is 2^16.
  // TODO: modify this calculation to use the clock frequency that you are
  // using.
  //
  PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, 6000);

  // Configure Direction pin
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
  MAP_GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_4);
  MAP_GPIOPadConfigSet(GPIO_PORTL_BASE, GPIO_PIN_4, GPIO_STRENGTH_8MA,
                                GPIO_PIN_TYPE_STD_WPD);
  MAP_GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_4, 0);

}

// Fucntion to set the PWM output given the duty cycle
// PWM can range from -100 to 100, if the value is negative
// we reverse the motion by setting the DIR pin low for the drive
// positive direction correspond to DIR pin being high
int8_t SetPWMDuty(int8_t duty)
{
  // If duty cycle is 0 , disable the PWM generator and output
  if(!duty)
  {
    PWMOutputState(PWM0_BASE, PWM_OUT_4_BIT, false);
    PWMGenDisable(PWM0_BASE, PWM_GEN_2);
  }
  else
  {
    // Set DIR pin accordingly
    if(duty < 0)
    {
      MAP_GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_4, 0);
      duty = 100 - (100 + duty);
    }
    else
      MAP_GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_4, GPIO_PIN_4);

    if(duty == 100)
      duty = 95;

    // Set the PWM pulse width (duty cycle)
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_4,
                   (PWMGenPeriodGet(PWM0_BASE, PWM_GEN_2) / 100) * (uint32_t)duty);
    PWMOutputState(PWM0_BASE, PWM_OUT_4_BIT, true);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);
  }
  // Return the set duty cycle
  return duty;
}

// 5 KHz timer that sets the flag for encoder value transmission
void
Timer0IntHandler(void)
{
    //
    // Clear the timer interrupt.
    //
    ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Set the flag
    sendEncoder = true;

}
#endif

// Main application
int main(void)
{
   uint32_t status;
   uint8_t printIMU = 0;
   // UART buffer
   uint8_t charUART[256];
   // The first time the IMU gives an interrupt we can set the BIAS NULL
   // command for auto-bias correction on accelerometer and gyroscope data
   uint8_t firstIMU = 0;

#ifdef ENABLE_UART
    // Character that is used to receive commands from the UART
    // Used only for debugging
    unsigned char uCom = 0;
#endif

#ifdef ENABLE_ETHERNET
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];
    // UDP Send buffer
    uint8_t sendUDP[128];
    // Hold the number of transmission (used for debugging)
    uint32_t sends = 0;
#endif

    // Variable to hold the read encoder value
    int32_t encoderPos = 0;

    // Initialize the application flags
#ifdef ENABLE_MOTOR
    sendEncoder = false;
    setPWMvalue = false;
    pwmValue = 0;
#endif

#ifdef ENABLE_ETHERNET
    gotIP = 0;

    // Set the proper values for lwip configuration based on board selection

    // // 192.168.1.82
    device_ip = 0xC0A8010A;
    IP4_ADDR(&board_ip, 0xC0,0xA8,0x01,0x0A);

    // 255.255.255.0
    device_subnet = 0xFFFFFF00;

    // 192.168.1.1
    device_gateway = 0xC0A80101;

    // TODO:
    IP4_ADDR(&controller_ip, 0xC0,0xA8,0x01,0x06);

#endif

    // Start the system clock (120 MHz)
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);


    // TODO: Enable pins for relay signals
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
        SysCtlDelay(10000);
        GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_7);
        GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_4);
        GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_5);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, true);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, true);
		MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, true);
	    SysCtlDelay(100000);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, false);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, true);
		MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, true);
	    SysCtlDelay(100000);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, true);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, false);
		MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, false);
	    SysCtlDelay(100000);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, false);
	    MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, true);
		MAP_GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, true);



#ifdef ENABLE_ETHERNET
    // Set pins for ethernet functionality
    PinoutSet(true, false);
    MAP_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);
    MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);
#else
    PinoutSet(false, false);
#endif

#ifdef ENABLE_UART
    ConfigureUART();
#endif

#ifdef ENABLE_ETHERNET
    // Initialize the SysTick timer
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
    MAP_SysTickIntEnable();

    MAP_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
#ifdef ENABLE_UART
        UARTprintf("No MAC programmed!\n");
#endif
        while(1)
        {
        }
    }

#ifdef ENABLE_UART
    UARTprintf("Waiting for IP.\n");
#endif

    pui8MACArray[0] = ((ui32User0 >>  0) & 0xff);
    pui8MACArray[1] = ((ui32User0 >>  8) & 0xff);
    pui8MACArray[2] = ((ui32User0 >> 16) & 0xff);
    pui8MACArray[3] = ((ui32User1 >>  0) & 0xff);
    pui8MACArray[4] = ((ui32User1 >>  8) & 0xff);
    pui8MACArray[5] = ((ui32User1 >> 16) & 0xff);

    // lwIP stack initialization
    //lwIPInit(g_ui32SysClock, pui8MACArray, 0, 0, 0, IPADDR_USE_DHCP);
    lwIPInit(g_ui32SysClock, pui8MACArray, device_ip, device_subnet, device_gateway, IPADDR_USE_STATIC);

    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
#endif

    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);

    // Wait until an IP is assigned
#ifdef ENABLE_ETHERNET
    while(gotIP == 0)
    	SysCtlDelay(120);
#endif

#ifdef ENABLE_UART
    UARTprintf("Initializing...\n");
#endif

    // Configure motor interface modules
#ifdef ENABLE_MOTOR
    // Setup the PWM generator
    SetupPWM();

    // QEI Setup
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
        SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);
        GPIOPinConfigure(GPIO_PL3_IDX0);
        GPIOPinConfigure(GPIO_PL1_PHA0);
        GPIOPinConfigure(GPIO_PL2_PHB0);
        GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
        // TODO
        QEIConfigure(QEI0_BASE, (QEI_CONFIG_CAPTURE_A_B | QEI_CONFIG_RESET_IDX | QEI_CONFIG_QUADRATURE | QEI_CONFIG_NO_SWAP), 1999);
        //
        QEIVelocityConfigure(QEI0_BASE, QEI_VELDIV_1, 4000);
        // Enable the quadrature encoder.
        //
        QEIEnable(QEI0_BASE);
        QEIVelocityEnable(QEI0_BASE);
        QEIPositionSet(QEI0_BASE,(0x00000001 << 31));
        //
        // Delay for some time...
        //
        SysCtlDelay(12000);
#endif

    // Initialize the UDP receive pcb
#ifdef ENABLE_ETHERNET
    Rpcb = udp_init_r();
#endif


    // Configure 5 KHz tier for encoder count acquisition
#ifdef ENABLE_MOTOR
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, g_ui32SysClock/5000);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);
#endif

    // Application Main Loop
    while(1)
    {

      // UART command interface for debugging
#ifdef ENABLE_UART

      switch(uCom)
      {
      default : break;
      }
      uCom = 0;
#endif

#ifdef ENABLE_MOTOR
      // If we received a PWM command set the corresponding PWM duty cyle and direction
      if(setPWMvalue == true)
            {
              //UARTprintf("Setting pwm to %d\n",pwmValue);
          	  if (pwmValue == 103)
          	  {
          		       UARTprintf("enableccw");
          		  	   GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, true);
          		  	   GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, false);
          		  	   UARTprintf("enableccw");
          	  }
          	  if (pwmValue == 101)
          	  {
          		        UARTprintf("kill");
      		  	        GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, false);
      		  	        GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, true);
      		  	        GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, true);
          		        UARTprintf("kill");
          	  }
          	  if (pwmValue == 102)
          	  {
          		        UARTprintf("enable");
          	      	    GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_7, true);
         		        UARTprintf("enable");
           	  }
              if (pwmValue == 104)
          	  {
          	            UARTprintf("enablecw");
          	    	    GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, false);
          	            GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, true);
          	            UARTprintf("enablecw");
           	  }
          	  else
          		  {
    		        UARTprintf("else");
          		    SetPWMDuty(pwmValue);
      		        UARTprintf("else");
          		  }
           }

      // If send encoder value event occurs, send data via UDP
      if(sendEncoder == true)
      {
        //sends++;
        encoderPos = (QEIVelocityGet(QEI0_BASE));
        //if(sends == 5000)
        //{
          //UARTprintf("Position %d\n",encoderPos);
          //sends = 0;
        //}
        sendEncoder = false;
#ifdef ENABLE_ETHERNET
        sendUDP[0] = 0x42;
        memcpy(&sendUDP[1],(uint8_t*)(&encoderPos),4);
        udp_send_data((void*)sendUDP,5);
#endif
      }
#endif
    }
}

#ifdef ENABLE_ETHERNET

// Initializze the UDP receive pcb
struct udp_pcb * udp_init_r(void)
{
  //err_t err;
  struct udp_pcb *pcb_r;
  pcb_r = udp_new();

  // Bind to given port , receive from any IP
  udp_bind(pcb_r, IP_ADDR_ANY, PORT_R);

#ifdef ENABLE_UART
  UARTprintf("UDP to receive at port %d...\n", PORT_R);
#endif

  // Set the receive data callback
  udp_recv(pcb_r, udp_receive_data, NULL);

  return pcb_r;
}

void udp_receive_data(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port)
{
    char * pPointer;

    //struct pbuf *p1;

    if (p != NULL)
    {
      //UARTwrite((char*)(p->payload), p->len);
      //UARTprintf("R : %s\n",(char*)(p->payload));

      pPointer = (char*)(p->payload);

     /* p1 = pbuf_alloc(PBUF_TRANSPORT,8,PBUF_RAM);
      memcpy (p1->payload, pData, 8);
      udp_send(pcb, p1);
      pbuf_free(p1);*/

      /*if(pPointer[0] == 0x31)
      {
    	  udp_send_data((void*)pData, 68);
      }*/
      // If we received PWM commnad (0x31 command byte)
      // extract the transmitted value
      if(pPointer[0] == 0x31)
      {
        pwmValue = pPointer[1];
        setPWMvalue = true;
      }

      /*if(pPointer[0] == 0x32)
      {
        sendEncoder = true;
      }*/

      pbuf_free(p);
    }
}

// Send data over UDP to the defined port
void udp_send_data(void* sbuf, u16_t len)
{
  struct pbuf *p;
  err_t err;

  p = pbuf_alloc(PBUF_TRANSPORT,len,PBUF_RAM);
  memcpy (p->payload, sbuf, len);
  err = udp_sendto(Rpcb, p, &controller_ip, PORT_S);

  pbuf_free(p);
}

#endif
