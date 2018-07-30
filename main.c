
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

    MAP_SPIIntClear(dirBaseSpi,tipoInt);

    if(tipoInt & SPI_INT_TX_EMPTY){
        MAP_SPIDataPutNonBlocking(dirBaseSpi,'1');
    }

    if (tipoInt & SPI_INT_RX_FULL ){
        MAP_UARTCharPutNonBlocking(dirBaseUart,str[car]);
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
    unsigned long dato;
    unsigned char nibble;

    /*Inicializacion y configuracion Board.*/
    BoardInit();
    PinMuxConfig();

    //Activacion interrupciones UART y SPI
    MAP_UARTIntRegister(dirBaseUart,Tx_UART);
    MAP_UARTIntEnable(dirBaseUart,UART_INT_TX);
    //ACtivación terminal UART
    MAP_UARTConfigSetExpClk(dirBaseUart,MAP_PRCMPeripheralClockGet(PRCM_UARTA0),
                           115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                           UART_CONFIG_PAR_NONE));

    //MAP_SPIIntRegister(dirBaseSpi,RT_Spi);
    //MAP_SPIIntEnable(dirBaseSpi,SPI_INT_TX_EMPTY|SPI_INT_RX_FULL );

    //MAP_SPIFIFOLevelSet(dirBaseSpi,1,1); //Se indica que la FIFO de envio y recepción tienen tam 1
    //MAP_SPIFIFOEnable(dirBaseSpi,SPI_TX_FIFO|SPI_RX_FIFO);

    MAP_SPIConfigSetExpClk(dirBaseSpi,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                           100000,SPI_MODE_MASTER,SPI_SUB_MODE_1,
                         (SPI_SW_CTRL_CS |
                         SPI_3PIN_MODE |
                         SPI_TURBO_OFF |
                         //SPI_CS_ACTIVELOW |
                         SPI_WL_8));


    MAP_SPIEnable(dirBaseSpi);
    //EN a 0 -> GPIOA1_BASE, 0x4 WritePin -> gpio11 ENable del TRF
    MAP_GPIOPinWrite(GPIOA1_BASE, 0x4, 1);


    //Envio primer caracter para iniciar bucle Trans/Recepción
    MAP_UARTCharPut(dirBaseUart,'1');
    //spiCS a 0 -> GPIOA1_BASE, 0x4 WritePin
    MAP_GPIOPinWrite(GPIOA1_BASE, 0x4, 0);
    MAP_SPIDataPut(dirBaseSpi,'A');
    //spiCS a 1 -> GPIOA1_BASE, 0x4 WritePin - Desactiva comunicacion
    MAP_GPIOPinWrite(GPIOA1_BASE, 0x4, 1);

    MAP_UARTCharPut(dirBaseUart,'2');
    MAP_SPIDataGet(dirBaseSpi,&dato);

    MAP_UARTCharPut(dirBaseUart,'<');
    MAP_UARTCharPut(dirBaseUart,'0');
    MAP_UARTCharPut(dirBaseUart,'x');

    nibble = (dato & 0x00F0) >> 4;
    if (nibble <= 9) {
        nibble += '0';
    } else {
        nibble += 'A';
    }
    MAP_UARTCharPut(dirBaseUart,nibble);

    nibble = dato & 0x000F;
    if (nibble <= 9) {
        nibble += '0';
    } else {
        nibble += 'A';
    }
    MAP_UARTCharPut(dirBaseUart,nibble);

    MAP_UARTCharPut(dirBaseUart,'>');

	while(1){
	    //MAP_SPIDataPutNonBlocking(dirBaseSpi,'A');
	}
}
