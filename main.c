
/*
 * Programa incial para comunicar SPi con TRF
 */
/*
 * includes
 *
 * Dificultad:
 *   Se han de agregar las librerias a la propiedad include option
 *   del ARM y agregar la carpeta Release y el archivo .a de la carpeta CCS
 *   al apartado ARMLinker.
 *
 */
#include "rom.h"  //Configuracion de la memoria ROM
#include "rom_map.h"  //MAP -> En caso de existir una version mas reciente sobreescribe métodos de ROM
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "hw_uart.h"  //Header controlador de Hardware Uart
#include "hw_types.h"
#include "hw_ints.h"
#include "uart.h"  //Header controlador a nivel software Uart
#include "udma.h"
#include "interrupt.h"  //Header definiciones basicas controlador interrupciones
#include "utils.h"
#include "prcm.h"
#include "gpio.h"
#include "prcm.h"
#include "pin_mux_config.h"
#include "spi.h"

//Definimos las direcciones BASE del GSPI
unsigned long dirBaseSpi = GSPI_BASE;
unsigned long dirBaseUart = UARTA0_BASE;

//Variables para gestionar el envio de la cadena "Mensaje Recivido"
int car = 0;
char str[17] = "Mensaje Recibido\0";

//Definimos el vector de interrupciones
extern void (* const g_pfnVectors[])(void);  //no se muy bien de donde sale.//


void RT_Spi(){
    //Función en la que comprobamos la interrupcion producida y la tratamos

    //obtenemos el tipo de interrupción.
    unsigned long tipoInt = MAP_SPIIntStatus(dirBaseSpi, false);

    SPIIntClear(dirBaseSpi,tipoInt);

    MAP_SPIDataPutNonBlocking(dirBaseSpi,'1');

    if (tipoInt & SPI_INT_RX_FULL ){
        MAP_UARTCharGetNonBlocking(dirBaseUart,str[car]);
    }
}

//Funcion en la se envia un caracter por la UART cada vez que se ha enviado un caracter
//hasta alcanzar el elemento fin de la cadena de carateres.
void Tx_UART(){
    //Obtenemos el tipo de interrupcion
    unsigned long tipoInt = MAP_UARTIntStatus(dirBaseUart, false);
    //Limpiamos el registro de las interrupciones recibidas
    MAP_UARTIntClear(dirBaseUart, tipoInt);

    if((tipoInt & UART_INT_TX) && (car < 17)){
        MAP_UARTCharPutNonBlocking(dirBaseUart,str[car++]);
    }

    if(car == 17){
        car= 0;
    }
}

//Inicializacion de la Board (sin interrupciones no funciona)

/**FUNCION IMPORTANTE*/
static void BoardInit(void){
    //Inicializacion del vector de interrupcion
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);

    //Activacion procesador de interrupciones
    MAP_IntMasterEnable();
    //Se activa la interrupcion principal
    MAP_IntEnable(FAULT_SYSTICK);

    //Se activa el microcontrolador
    PRCMCC3200MCUInit();
}


/**
 * main.c
 */
int main(void)
{
    unsigned char dato;
    unsigned char nibble;
    unsigned char comando, ucGPIOPin;
    unsigned int uiGPIOPort;
    unsigned char ucDummy;


    /*Inicializacion y configuracion Board.*/
    BoardInit();

    PinMuxConfig();

    ucGPIOPin = 1 << (10 % 8);
    uiGPIOPort = GPIOA1_BASE;
    MAP_GPIOPinWrite(uiGPIOPort,ucGPIOPin,0xff);

    //Activacion interrupciones UART y SPI
    MAP_UARTIntRegister(dirBaseUart,Tx_UART);
    MAP_UARTIntEnable(dirBaseUart,UART_INT_TX);
    //ACtivación terminal UART
    MAP_UARTConfigSetExpClk(dirBaseUart,MAP_PRCMPeripheralClockGet(PRCM_UARTA0),
                           115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                           UART_CONFIG_PAR_NONE));
    //
    //
    //Activacion SPI
    //
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_GSPI);
    MAP_SPIReset(GSPI_BASE);

    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
             2000000,SPI_MODE_MASTER,SPI_SUB_MODE_1,
                (SPI_SW_CTRL_CS |
                  SPI_3PIN_MODE |
                  SPI_TURBO_OFF |
               SPI_CS_ACTIVELOW |
                       SPI_WL_8));

    MAP_SPIEnable(GSPI_BASE);
    //
    //
    //Fin activacion SPI
    //
    //


    //Registro de funcion a realizar para las interrupciones de recepción de datos del TRF en INT_GPIOA0 (no usado)
    //IntRegister(16, RT_Spi);
    //IntPrioritySet(16, INT_PRIORITY_LVL_1);

    //Activacion Interrupciones provenientes del TRF activos en flanco de subida
    GPIOIntTypeSet(GPIOA2_BASE, 0x2, GPIO_RISING_EDGE);
    GPIOIntClear(GPIOA2_BASE, 0x2);
    GPIOIntEnable(GPIOA2_BASE, 0x2);
    IntEnable(INT_GPIOA2); //Interrupciones de la direccion BASE A2

    //SPIIntEnable(GPIOA0_BASE, SPI_INT_RX_FULL);

    //Modo SOFT_INI nfc:
    comando = 0x03;
    comando = 0x80 | comando;
    comando = 0x9f & comando; //Normalizamos los valores para confirmar comando directo

    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    SPITransfer(GSPI_BASE, comando, &ucDummy, 1, SPI_CS_ENABLE);
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);

    //Envio dummy para reset de TRF
    comando = 0x00;
    comando = 0x80 | comando;
    comando = 0x9f & comando;
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    SPITransfer(GSPI_BASE, comando, &ucDummy, 1, SPI_CS_ENABLE);
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);

    //Valor direccion a leer:
    ucDummy = 0x04;
    /****************/
    ucDummy = 0x60 | ucDummy;

    //spiCS a 0 -> GPIOA1_BASE, 0x4 WritePin  -   Cambiado por actualizacion de TRF a 4.5
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    MAP_SPIDataPut(GSPI_BASE,ucDummy);  ///////////////Envio
    //MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);

    //MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    MAP_SPIDataGet(GSPI_BASE,&ucDummy);  /////////////////Recepcion
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);

    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    MAP_SPIDataPut(GSPI_BASE,ucDummy); ////////////////Envio
   // MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);

    //MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0x00);
    MAP_SPIDataGet(GSPI_BASE,&ucDummy);  //////////////Recepcion
    MAP_GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff);


    while(1){}
}
